#pragma once
#include <thread>
#include <communicator.h>
#include "router.h"
#include "data.h"
#include "exchange_metadata.h"

namespace ib_bench {

/// Async-sends all to all, but delays sending i'th packet until (i-gap)'th is received
template <size_t PacketSize = 0>
struct rdma_gap_runner {

    static constexpr bool static_packet = PacketSize > 0;

    using data_type = std::conditional_t<
        static_packet,
        ct_ints<PacketSize / sizeof(rt_ints<>::value_type)>,
        ucx_rt_ints
    >;

    // static case
    template <size_t PS = PacketSize, class T = std::enable_if_t<(PS > 0)>>
    rdma_gap_runner(
        ucp::communicator& comm,
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table
    ) : rdma_gap_runner(
            comm, iters_to_run, max_gap, std::move(routing_table), PacketSize, 0
        )
    { }

    // dynamic case
    template <size_t PS = PacketSize, class T = std::enable_if_t<PS == 0>>
    rdma_gap_runner(
        ucp::communicator& comm,
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size
    ): rdma_gap_runner(
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
    rdma_gap_runner(
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
        // TODO: undertand why 2*max_gap circular buffer is not enough
        m_received(comm.size()),
        m_sent(max_gap * comm.size()),
        m_atomics(comm.size()),
        m_route(m_router())
    {
        std::cout << "Iterations: " << m_iters_to_run << " packet size " <<
            m_packet_size << " bytes " << " max gap " << m_max_gap << std::endl;
        VALIDATE(
            m_router.is_complete(),
            "This test requires complete (all-to-all) routing table"
        );
        auto generator = make_generator(m_comm.rank(), m_packet_size);

        // generate the buffers, just to set correct size
        for (auto& buf : m_received) {
            buf = generator(m_comm.rank());
        }
        for (auto& buf : m_sent) {
            buf = generator(m_comm.rank());
        }
    }

    bool may_send(const data_type& packet) {
        size_t latest_complete = *std::min_element(begin(m_atomics), end(m_atomics));
        return (packet.id() - latest_complete) <= m_max_gap;
    }

    void wait_for_atomics() {
        while (*std::min_element(begin(m_atomics), end(m_atomics)) < m_iters_to_run) {
            m_comm.get_context().poll();
        }
    }

    void send_to_peers(
        const std::vector<ucp::memory>& remote_mem, 
        const std::vector<ucp::rkey>& remote_keys, 
        const std::vector<ucp::memory>& remote_mem_atomics, 
        const std::vector<ucp::rkey>& remote_keys_atomics, 
        data_type& packet
    ) {
        for (int dest : m_route) {
            m_stats.update_sent(packet.size());
            m_comm.async_put_memory(
                dest, 
                ucp::memory(packet.container()), 
                (uintptr_t)remote_mem[dest].address(), 
                remote_keys[dest]
            );
            m_comm.get_worker().fence();
            m_comm.atomic_post(
                dest, 
                UCP_ATOMIC_POST_OP_ADD, 
                1, 
                8, 
                (uintptr_t)remote_mem_atomics[dest].address(), 
                remote_keys_atomics[dest]
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
        auto generator = make_generator(m_comm.rank(), m_packet_size);
        auto[remote_mem, remote_keys, local_mem] = exchange_metadata(m_comm, m_route, m_received);
        auto[remote_mem_atomics, remote_keys_atomics, local_mem_atomics] = exchange_metadata(m_comm, m_route, m_atomics);

        for (size_t iters = 0; iters < m_iters_to_run; ++iters) {
            auto& to_send = m_sent[m_sent_free_index];
            // to speed up, just set meta, no need in actual data
            //to_send = generator(m_comm.rank());
            generator.set_meta(to_send);
            //m_sent[m_sent_free_index] = generator();
            m_atomics[m_comm.rank()] = to_send.id();
            while (!may_send(to_send)) {
                m_comm.get_context().poll();
            }
            //std::cout << getpid() << " m_sent_free_index " << m_sent_free_index << std::endl;
            //std::cout << getpid() << " m_received_free_index " << m_received_free_index << std::endl;
            send_to_peers(remote_mem, remote_keys, remote_mem_atomics, remote_keys_atomics, to_send);
            m_sent_free_index = (m_sent_free_index + 1) % m_sent.size();
            m_comm.get_context().poll();
        }
        m_comm.run();
        wait_for_atomics();
    }

    ucp::communicator& m_comm;
    int m_max_gap;
    size_t m_iters_to_run;
    router m_router;
    size_t m_packet_size;
    NetStats m_stats;
    // multiple buffers per source
    std::vector<data_type> m_received;
    std::vector<data_type> m_sent;
    std::vector<uint64_t> m_atomics;
    size_t m_sent_free_index = 0;
    size_t m_received_free_index = 0;
    router::route m_route;
};

}

