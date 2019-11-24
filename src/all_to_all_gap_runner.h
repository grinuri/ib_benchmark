#pragma once
#include <thread>
#include <boost/mpi.hpp>
#include "router.h"
#include "data.h"

namespace ib_bench {
namespace mpi = boost::mpi;

/// Async-sends all to all, but delays sending i'th packet until (i-gap)'th is received
template <size_t PacketSize>
struct all_to_all_gap_runner {
    all_to_all_gap_runner(
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size
    ) :
        m_env(mpi::threading::serialized),
        m_max_gap(max_gap),
        m_iters_to_run(iters_to_run),
        m_router((size_t)m_comm.size(), (size_t)m_comm.rank(), std::move(routing_table)),
        m_packet_size(packet_size)
    {
        VALIDATE(
            m_router.is_complete(),
            "This test requires complete (all-to-all) routing table"
        );
        VALIDATE(
            m_env.thread_level() == mpi::threading::serialized,
            "Requested threading level is not supported"
        );
    }

    void run() {
        using data_type = rt_ints<>;
        m_stopped = false;
        generator<data_type> generator(m_comm.rank(), m_packet_size / sizeof(int));

        std::thread sender{
            [&]() {
                int sent = 0;
                for (size_t iters = 0; iters < m_iters_to_run; ++iters) {
                    auto packet = generator();
                    int delta = 0;
                    while ((delta = (packet.id - m_latest_complete)) > m_max_gap + 1) {
                        std::this_thread::yield();
                    }
                    auto route = m_router();
                    for (int dest : route) {
                        ++sent;
                        m_stats.update_sent(packet.size());
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_comm.isend(dest, 0, std::move(packet)).test();
                    }
                }
                m_stopped = true;
                BENCH_LOG_DEBUG(boost::format("[%d] stopped, sent %d") % m_comm.rank() % sent);
            }
        };

        std::thread receiver{
            [&]() {
                std::unordered_map<size_t, int> received_ids;
                size_t iters = 0;
                size_t total_iters = m_iters_to_run * (m_comm.size() - 1);
                while (iters < total_iters) {
                    static data_type data;

                    if (try_receive(mpi::any_source, 0, data)) {
                        ++iters;
                        if (received_ids.find(data.id) == received_ids.end()) {
                            received_ids[data.id] = 0;
                        }
                        // since mpi messages from i to j are sent in order,
                        // if we got (id == K) from all the world, then we got every (id < K) for sure
                        if (++received_ids[data.id] == m_comm.size() - 1) {
                            m_latest_complete = data.id;
                            // erasing just to save some memory
                            received_ids.erase(data.id);
                        }
                    }
                }
                BENCH_LOG_DEBUG(boost::format("[%d] receive thread exited, received %d") % m_comm.rank() % iters);
            }
        };
        sender.join();
        m_stats.finish();
        receiver.join();
        std::cout << "Rank " << m_comm.rank() << " sent " << (m_stats.bytes_sent() / 1024) <<
            " KB " << m_stats.seconds_passed() << " sec " << (m_stats.upstream_bandwidth() / 1024 / 1024) << " MB/s" << std::endl;
    }

private:

    template <class T>
    bool try_receive(int source, int tag, T& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_receive_req_opt) {
            m_receive_req_opt = m_comm.irecv(source, tag, value);
        }
        if (m_receive_req_opt->test()) {
            m_receive_req_opt = m_comm.irecv(source, tag, value);
            return true;
        }
        return false;
    }

    mpi::environment m_env;
    mpi::communicator m_comm;
    int m_max_gap;
    size_t m_iters_to_run;
    std::atomic_bool m_stopped;
    std::atomic_int m_latest_complete = 0;
    std::optional<mpi::request> m_receive_req_opt;
    router m_router;
    mutable std::mutex m_mutex;
    size_t m_packet_size;
    NetStats m_stats;
};

}

