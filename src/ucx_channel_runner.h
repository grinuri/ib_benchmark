#pragma once
#include <thread>
#include <boost/mpi.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include "communication/communicator.h"
#include "communication/backend_ucx.h"
#include <cereal/types/array.hpp>
#include "router.h"
#include "data.h"
#include "util/net_stats.h"

namespace ib_bench {
namespace mpi = boost::mpi;

/// Async-sends all to all (or some to some, depending on the routing table),
/// via independent channels
template <class... ChannelTypes>
struct ucx_channel_runner {

    using channel_priorities = std::array<size_t, sizeof...(ChannelTypes)>;

    ucx_channel_runner(
        ucp::communicator& comm,
        size_t iters_to_run,
        size_t flush_size,
        size_t iters_to_sync,
        const channel_priorities& channel_priorities,
        router::routing_table routing_table
    ) :
        m_comm(comm, flush_size),
        m_comm_size(m_comm.size()),
        m_iters_to_run(iters_to_run),
        m_iters_to_sync(iters_to_sync),
        m_channel_priorities(channel_priorities),
        m_router((size_t)m_comm.size(), (size_t)m_comm.rank(), std::move(routing_table))
    {
        std::srand(0);
    }

    void sync() {
        auto per_channel = [=]<size_t... Is>(std::index_sequence<Is...>) {
            (..., m_comm.template synchronize<Is>());
        };
        per_channel(std::index_sequence_for<ChannelTypes...>{});
    }

    int m_packet_id = 0;
    // send random data to a single channel
    template <size_t PORT, typename T>
    void send_random_to_channel(size_t dest) {
        for (size_t i = 0; i < 1 + m_channel_priorities[PORT]; ++i) {
            auto data = generator<T>(m_comm.rank())();
            data.data[0] = ++m_packet_id;
            m_stats.update_sent(data.size());
//            std::cout << " send random " << m_comm.rank() << "->" << dest << std::endl;
            m_comm.template send<PORT>(std::move(data), dest);
        }
    }

    // send random data of types that correspond to the channels
    void send_random(size_t dest) {
        auto per_channel = [=]<size_t... Is>(std::index_sequence<Is...>) {
            (..., send_random_to_channel<Is, ChannelTypes>(dest));
        };
        per_channel(std::index_sequence_for<ChannelTypes...>{});
    }

    void mark_eof() {
        auto per_channel = [=]<size_t... Is>(std::index_sequence<Is...>) {
            (..., m_comm.mark_eof(Is));
        };
        per_channel(std::index_sequence_for<ChannelTypes...>{});
    }

    template <size_t PORT, typename T>
    void receive_channel() {
        m_comm.template try_receive<PORT>();
    }

    void receive() {
        auto per_channel = [=]<size_t... Is>(std::index_sequence<Is...>) {
            (..., receive_channel<Is, ChannelTypes>());
        };
        per_channel(std::index_sequence_for<ChannelTypes...>{});
    }

    void run() {
        using namespace ib_bench;
        std::thread main_loop([this] { m_comm.run(); });

        m_stopped = false;
        auto send = [&]() {
            for (size_t iters = 0; iters < m_iters_to_run; ++iters) {
                //std::cout << getpid() << " Iter: " << iters << std::endl;
                auto route = m_router();
                //for (auto d : route) {
                    //std::cout << m_comm.rank() << "==>" << d << std::endl;
                //}
                boost::for_each(route, [this](auto dest) { send_random(dest); });
                if (iters % m_iters_to_sync == 0) {
                    sync();
                }
            }
            mark_eof();
            BENCH_LOG_DEBUG(boost::format("[%d] send exit") % m_comm.rank());
        };

        auto receive = [&]() {
            while (!m_stopped) {
                this->receive();
            }
            BENCH_LOG_DEBUG(boost::format("[%d] recv exit") % m_comm.rank());
        };

        std::thread sender{send};
        std::thread receiver{receive};

        sender.join();
        main_loop.join();
        // at this point the communicator is closed, stop the receiver
        m_stopped = true;
        m_stats.finish();
        receiver.join();
        std::cout << "Rank " << m_comm.rank() << " sent " << (m_stats.bytes_sent() / 1024 / 1024) <<
            " MB " << m_stats.seconds_passed() << " sec " << (m_stats.upstream_bandwidth() * 8 / (1 << 30)) << " Gbit/s" << std::endl;
    }

private:
    SRCommunicator<UCXBackend, ChannelTypes...> m_comm;
    size_t m_comm_size;
    size_t m_iters_to_run;
    size_t m_iters_to_sync;
    std::atomic_bool m_stopped;
    channel_priorities m_channel_priorities;
    router m_router;
    NetStats m_stats;
};

}

