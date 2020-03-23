#pragma once
#include <thread>
#include <boost/mpi.hpp>
#include "router.h"
#include "data.h"

namespace ib_bench {
namespace mpi = boost::mpi;

/// Async-sends all to all, but delays sending i'th packet until (i-gap)'th is received
template <size_t PacketSize = 0>
struct all_to_all_gap_runner {

    static constexpr bool static_packet = PacketSize > 0;

    using data_type = std::conditional_t<
        static_packet,
        ct_ints<PacketSize / sizeof(rt_ints<>::value_type)>,
        rt_ints<>
    >;
    struct request_and_data {
        std::shared_ptr<data_type> data_ptr;
        mpi::request request;
    };

    // static case
    template <size_t PS = PacketSize, class T = std::enable_if_t<(PS > 0)>>
    all_to_all_gap_runner(
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table
    ) : all_to_all_gap_runner(
            iters_to_run, max_gap, std::move(routing_table), PacketSize, 0
        )
    { }

    // dynamic case
    template <size_t PS = PacketSize, class T = std::enable_if_t<PS == 0>>
    all_to_all_gap_runner(
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size
    ): all_to_all_gap_runner(
            iters_to_run, max_gap, std::move(routing_table), packet_size, 0
        )
    { }

    void run() {
        send_receive();
        m_stats.finish();
        std::cout << "Rank " << m_comm.rank() << " sent " << (m_stats.bytes_sent() / 1024) <<
            " KB " << m_stats.seconds_passed() << " sec " << (m_stats.upstream_bandwidth() / 1024 / 1024) <<
            " MB/s" << std::endl;
    }

private:
    all_to_all_gap_runner(
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size,
        int
    ) :
        m_env(mpi::threading::single),
        m_max_gap(max_gap),
        m_iters_to_run(iters_to_run),
        m_router((size_t)m_comm.size(), (size_t)m_comm.rank(), std::move(routing_table)),
        m_packet_size(packet_size)
    {
        std::cout << "Iterations: " << m_iters_to_run << " packet size " <<
            m_packet_size << " bytes " << " max gap " << m_max_gap << std::endl;
        VALIDATE(
            m_router.is_complete(),
            "This test requires complete (all-to-all) routing table"
        );
//        VALIDATE(
//            m_env.thread_level() == mpi::threading::funneled,
//            "Requested threading level is not supported"
//        );
    }

    bool may_send(const data_type& packet) {
        return (packet.id - m_latest_complete) <= (m_max_gap + 1);
    }

    void test_sent() {
        while (m_send_queue.size() && m_send_queue.front().request.test()) {
            m_send_queue.pop_front();
        }
    }

    void wait_for_sent() {
        while (m_send_queue.size()) {
            m_send_queue.front().request.wait();
            m_send_queue.pop_front();
        }
    }

    void send_to_peers(data_type&& packet) {
        auto route = m_router();
        // must own the data until the requet is complete
        auto data_ptr = std::make_shared<data_type>(std::move(packet));
        //mpi::content content = mpi::get_content(*data_ptr); // does not work
        for (int dest : route) {
            m_stats.update_sent(data_ptr->size());
            request_and_data rnd;
            rnd.data_ptr = data_ptr;
            rnd.request = m_comm.isend(dest, 0, *data_ptr);
            // if done quickly, no need to store
            if (!rnd.request.test()) {
                m_send_queue.push_back(std::move(rnd));
            }
        }
    }

    bool try_receive(int source, int tag, data_type& value) {
        if (!m_receive_req_opt) {
            m_receive_req_opt = m_comm.irecv(source, tag, value);
        }
        if (m_receive_req_opt->test()) {
            m_receive_req_opt = m_comm.irecv(source, tag, value);
            return true;
        }
        return false;
    }

    void receive_from_peers() {
        // we discard the data
        data_type data;
        if (try_receive(mpi::any_source, 0, data)) {
            ++m_receives;
            if (m_received_ids.find(data.id) == m_received_ids.end()) {
                m_received_ids[data.id] = 0;
            }
            // since mpi messages from i to j are sent in order,
            // if we got (id == K) from all the world, then we got every (id < K) for sure
            if (++m_received_ids[data.id] == m_comm.size() - 1) {
                m_latest_complete = data.id;
                // erasing just to save some memory
                m_received_ids.erase(data.id);
            }
        }
    }

    // static case
    template <size_t PS = PacketSize>
    auto make_generator(int rank, size_t, std::enable_if_t<(PS > 0)>* = 0) {
        return generator<data_type>(rank);
    }

    // dynamic case
    template <size_t PS = PacketSize>
    auto make_generator(int rank, size_t size, std::enable_if_t<(PS == 0)>* = 0) {
        return generator<data_type>(rank, size / sizeof(typename data_type::value_type));
    }

    void send_receive() {
        auto generator = make_generator(m_comm.rank(), m_packet_size);

        for (size_t iters = 0; iters < m_iters_to_run; ++iters) {
            auto packet = generator();
            bool sent = false;
            while (!sent) {
                if (may_send(packet)) {
                    send_to_peers(std::move(packet));
                    sent = true;
                }
                receive_from_peers();
                test_sent();
            }
        }
        // ensure that we receive every send from every peer
        size_t expected_total_receives = m_iters_to_run * (m_comm.size() - 1);
        while (m_receives < expected_total_receives) {
            test_sent();
            receive_from_peers();
        }
        // ensure all the send requests are complete
        wait_for_sent();
    }

    mpi::environment m_env;
    mpi::communicator m_comm;
    int m_max_gap;
    size_t m_iters_to_run;
    int m_latest_complete = 0;
    std::optional<mpi::request> m_receive_req_opt;
    router m_router;
    size_t m_packet_size;
    NetStats m_stats;
    std::unordered_map<size_t, int> m_received_ids;
    std::list<request_and_data> m_send_queue;
    size_t m_receives = 0;
};

}

