#pragma once
#include <thread>
#include <shmem.h>
#include "router.h"
#include "data.h"

namespace ib_bench {

/// Async-sends all to all, but delays sending i'th packet until (i-gap)'th is received
struct all_to_all_gap_runner_shmem {

    using data_type = shmem_rt_ints;

    struct shmem {
        shmem() {
            shmem_init();
        }
        ~shmem() {
            shmem_finalize();
        }
    };

    all_to_all_gap_runner_shmem(
        size_t iters_to_run,
        int max_gap,
        router::routing_table routing_table,
        size_t packet_size
    ) :
        m_max_gap(max_gap),
        m_iters_to_run(iters_to_run),
        m_router((size_t)shmem_n_pes(), (size_t)shmem_my_pe(), std::move(routing_table)),
        m_packet_size(packet_size),
        m_dest_data(shmem_n_pes()),
        m_received_ids(shmem_n_pes())
    {
        std::cout << "Iterations: " << m_iters_to_run << " packet size " <<
            m_packet_size << " bytes " << " max gap " << m_max_gap << std::endl;
        VALIDATE(
            m_router.is_complete(),
            "This test requires complete (all-to-all) routing table"
        );
        VALIDATE(
            m_packet_size % sizeof(data_type::value_type) == 0,
            "Unaligned packet size"
        )
        for (auto& dest_data : m_dest_data) {
            dest_data.data.resize(m_packet_size / sizeof(data_type::value_type));
        }
        m_received_ids[shmem_my_pe()] = std::numeric_limits<int>::max();
    }

    void run() {
        generator<data_type> generator(shmem_my_pe(), m_packet_size / sizeof(int));
        send_receive();
        m_stats.finish();
        std::cout << "Rank " << shmem_my_pe() << " sent " << (m_stats.bytes_sent() / 1024) <<
            " KB " << m_stats.seconds_passed() << " sec " << (m_stats.upstream_bandwidth() / 1024 / 1024) << " MB/s" << std::endl;
    }

private:
    struct request_and_data {
        std::shared_ptr<data_type> data_ptr;
    };

    bool may_send(const data_type& packet) {
        return (packet.id() - m_latest_complete) <= (m_max_gap + 1);
    }

    void send_to_peers(data_type&& packet) {
        int id = packet.id();
        auto route = m_router();
        // must own the data until fence
        for (int dest : route) {
            m_stats.update_sent(packet.size());
            shmem_putmem(m_dest_data[shmem_my_pe()].data.data(), packet.data.data(), packet.data.size(), dest);
        }
        shmem_fence();
        for (int dest : route) {
            shmem_int_p(&m_received_ids[shmem_my_pe()], id, dest);
        }
    }

    // waits until everyone one sent at least id
    void wait_until(int id) {
        // TODO: is it legitimate race?
        int latest = *std::min_element(begin(m_received_ids), end(m_received_ids));
        if (latest >= id) {
            m_latest_complete = latest;
            return;
        }
        for (int rank = 0; rank < shmem_n_pes(); ++rank) {
            if (rank != shmem_my_pe()) {
                shmem_int_wait_until(&m_received_ids[rank], SHMEM_CMP_GE, id);
            }

        }
        // TODO: is it legitimate race?
        m_latest_complete = *std::min_element(begin(m_received_ids), end(m_received_ids));
    }

    auto make_generator(int rank, size_t size) {
        return generator<data_type>(rank, size / sizeof(typename data_type::value_type));
    }

    void send_receive() {
        auto generator = make_generator(shmem_my_pe(), m_packet_size);
        int last_id = 0;
        for (size_t iters = 0; iters < m_iters_to_run; ++iters) {
            auto packet = generator();
            last_id = packet.id();
            bool sent = false;
            while (!sent) {
                if (may_send(packet)) {
                    send_to_peers(std::move(packet));
                    sent = true;
                }
                wait_until(m_latest_complete + 1);
            }
        }
        // ensure that we receive every send from every peer
        wait_until(last_id);
    }

    shmem m_shmem;
    int m_max_gap;
    size_t m_iters_to_run;
    int m_latest_complete = 0;
    router m_router;
    size_t m_packet_size;
    NetStats m_stats;
    std::vector<data_type, shmem_allocator<data_type>> m_dest_data;
    std::vector<int, shmem_allocator<int>> m_received_ids;
    std::list<request_and_data> m_send_queue;
};

}

