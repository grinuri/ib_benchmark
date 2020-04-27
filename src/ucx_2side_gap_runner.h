#pragma once
#include <thread>
#include <communicator.h>
#include "router.h"
#include "data.h"

namespace ib_bench {

/// Async-sends all to all, but delays sending i'th packet until (i-gap)'th is received
template <size_t PacketSize = 0>
struct tag_gap_runner {

    static constexpr bool static_packet = PacketSize > 0;

    using data_type = std::conditional_t<
        static_packet,
        ct_ints<PacketSize / sizeof(rt_ints<>::value_type)>,
        ucx_rt_ints
    >;
    struct request_and_data {
        std::shared_ptr<data_type> data_ptr;
        //ucp::request request;
    };

    // static case
    template <size_t PS = PacketSize, class T = std::enable_if_t<(PS > 0)>>
    tag_gap_runner(
        ucp::communicator& comm,
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table
    ) : tag_gap_runner(
            comm, iters_to_run, max_gap, std::move(routing_table), PacketSize, 0
        )
    { }

    // dynamic case
    template <size_t PS = PacketSize, class T = std::enable_if_t<PS == 0>>
    tag_gap_runner(
        ucp::communicator& comm,
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size
    ): tag_gap_runner(
            comm, iters_to_run, max_gap, std::move(routing_table), packet_size, 0
        )
    { }

    void run() {
        send_receive();
        m_stats.finish();
        std::cout << "Rank " << m_comm.rank() << " sent " << (m_stats.bytes_sent() / 1024 / 1024) <<
            " MB " << m_stats.seconds_passed() << " sec " << (m_stats.upstream_bandwidth() * 8 / 1000000000) <<
            " Gbit/s" << std::endl;
    }

private:
    tag_gap_runner(
        ucp::communicator& comm,
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size,
        int
    ) :
        m_comm(comm),
        m_max_gap(max_gap),
        m_iters_to_run(iters_to_run),
        m_router((size_t)m_comm.size(), (size_t)m_comm.rank(), std::move(routing_table)),
        m_packet_size(packet_size),
        m_received(comm.size()),
        m_sent(max_gap + 1),
        m_route(m_router())
    {
        std::cout << "Iterations: " << m_iters_to_run << " packet size " <<
            m_packet_size << " bytes " << " max gap " << m_max_gap << std::endl;
        VALIDATE(
            m_router.is_complete(),
            "This test requires complete (all-to-all) routing table"
        );
        for (auto& buf : m_received) {
            // TODO: don't do +2 hardcoded
            buf.data.resize(m_packet_size + 2);
        }
        for (auto& buf : m_sent) {
            // TODO: don't do +2 hardcoded
            buf.data.resize(m_packet_size + 2);
        }
    }

    bool may_send(const data_type& packet) {
        return (packet.id() - m_latest_complete) <= (m_max_gap + 1);
    }

    void wait_for_send() {
        while (m_pending_send > 0) {
            m_comm.get_context().poll();
        }
    }

    void send_to_peers(const data_type& packet) {
        for (int dest : m_route) {
            m_stats.update_sent(packet.size());
            ++m_pending_send;
            // tag is sender rank
            m_comm.async_send(
                dest, 
                boost::asio::const_buffer(packet.data.data(), packet.size()), 
                m_comm.rank(), 
                [this](auto status, auto) { ucp::check(status), --m_pending_send; }
            );
        }
    }

    void receive_from_peers() {
        for (int source : m_route) {
            // tag is sender rank
            m_comm.async_receive(
                boost::asio::buffer(m_received[source].data.data(), m_received[source].size()), 
                source, 
                [this, source](auto, auto) { 
                    ++m_receives; 
                    auto id = m_received[source].id();
                    if (m_received_ids.find(id) == m_received_ids.end()) {
                        m_received_ids[id] = 0;
                    }
                    // since ucx messages from i to j are sent in order,
                    // if we got (id == K) from all the world, then we got every (id < K) for sure
                    if (++m_received_ids[id] == m_comm.size() - 1) {
                        m_latest_complete = id;
                        // erasing just to save some memory
                        m_received_ids.erase(id);
                    }
                }
            );
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
        m_pending_send = 0;
        auto generator = make_generator(m_comm.rank(), m_packet_size);

        for (size_t iters = 0; iters < m_iters_to_run; ++iters) {
            auto& to_send = m_sent[m_sent_free_index];
            // to speed up, just set meta, no need in actual data
            generator.set_meta(to_send);
            //m_sent[m_sent_free_index] = generator();
            bool sent = false;
            while (!sent) {
                m_comm.get_context().poll();
                if (may_send(to_send)) {
                    send_to_peers(to_send);
                    sent = true;
                    receive_from_peers();
                }
            }
            m_comm.get_context().poll();
        }
        m_comm.run();
    }

    ucp::communicator& m_comm;
    int m_max_gap;
    size_t m_iters_to_run;
    int m_latest_complete = 0;
    std::optional<ucp::request> m_receive_req_opt;
    router m_router;
    size_t m_packet_size;
    NetStats m_stats;
    std::unordered_map<size_t, int> m_received_ids;
    size_t m_receives = 0;
    size_t m_pending_send = 0;
    std::vector<data_type> m_received;
    std::vector<data_type> m_sent;
    size_t m_sent_free_index = 0;
    router::route m_route;
};

}

