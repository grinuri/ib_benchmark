#include "bench2.h"

#include <boost/mpi/communicator.hpp>
#include <boost/mpi/collectives.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/access.hpp>

#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <numeric>

#include "util/net_stats.h"

#include <communicator.h>
#include "ucx.h"

using namespace ib_bench;

namespace {
using packet_t = std::vector<char>;

size_t generate_size(size_t min, size_t max) {
    return min + double(max - min) * ((double)std::rand() / (double)RAND_MAX) ;
}

char generate_char() {
    return 'a' + std::rand() % 26;
}

packet_t generate_packet(size_t min_size, size_t max_size) {
    packet_t packet(generate_size(min_size, max_size));
    std::generate(packet.begin(), packet.end(), generate_char);
    return packet;
}

template <class Buffers>
size_t generate_data(
    ucp::communicator& comm, 
    size_t iterations, 
    const router::route& route,
    Buffers& to_send,
    size_t buff_size_min,    
    size_t buff_size_max    
) {
    std::srand(10 + comm.rank());
    size_t total_bytes = 0;
    for (int i : route) {
        to_send[i] = generate_packet(buff_size_min, buff_size_max);
        total_bytes += iterations * to_send[i].size();
    }
//    if (to_send[comm.rank()].size() == 0) {
//        to_send[comm.rank()] = generate_packet(buff_size_min, buff_size_max);
//    }
    return total_bytes;
}

}

void tag_all2all(
    ucp::communicator& comm, 
    size_t iterations, 
    router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size,
    bool variable
) {
    boost::mpi::environment env;
    boost::mpi::communicator world;
    std::vector<packet_t> in_packets(world.size());
    std::vector<packet_t> out_packets(world.size());

    router router(comm.size(), comm.rank(), std::move(routing_table));
    auto route = router();

    size_t sent_bytes = generate_data(
        comm, iterations, route, in_packets, max_packet_size, max_packet_size
    );

    if (!variable) {
        for (auto& pack : out_packets) {
            pack.resize(max_packet_size);
        }
    }

    NetStats stats; // start after data creation overhead
    stats.update_sent(sent_bytes);

    for (size_t i = 0; i < iterations; ++i) {
        comm.all_to_all(in_packets, out_packets, 0, variable);
    }

    stats.finish();

    std::cout << "rank " << world.rank() << " sent total of : "
        << stats.bytes_sent() / (1 << 20) << " MB" << std::endl;

    std::cout << "rank " << world.rank() << " upstream bandwidth: "
        << stats.upstream_bandwidth() * 8 / (1 << 30) << " GBit/s" << std::endl;
}

void tag_all2all_variable(
    ucp::communicator& comm, 
    size_t iterations, 
    router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size
) {
    tag_all2all(comm, iterations, routing_table, min_packet_size, max_packet_size, true);
}

void tag_all2all_fixed(
    ucp::communicator& comm, 
    size_t iterations, 
    router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size
) {
    tag_all2all(comm, iterations, routing_table, min_packet_size, max_packet_size, false);
}

struct metadata {
    std::vector<ucp::memory> remote_mem;
    std::vector<ucp::rkey> remote_keys;
    std::vector<ucp::registered_memory> local_mem;
};

template <class T>
auto get_data(T& buff, std::enable_if_t<std::is_pod_v<T>>* = 0) {
    return &buff;
}

template <class T>
auto get_size(T& buff, std::enable_if_t<std::is_pod_v<T>>* = 0) {
    return sizeof(buff);
}

template <class Buff>
auto get_data(Buff& buff) -> decltype(buff.data()) {
    return buff.data();
}

template <class Buff>
auto get_size(Buff& buff) -> decltype(buff.size()) {
    return buff.size();
}


template <class Buffers>
metadata exchange_metadata(ucp::communicator& comm, const router::route& route, Buffers& buffs) {
    std::vector<ucp::registered_memory> registered_mem(comm.size());
    for (size_t rank : route) {
        registered_mem[rank] = comm.get_context().register_memory(
            get_data(buffs[rank]), get_size(buffs[rank])
        );
    }
    if (!registered_mem[comm.rank()]) {
        registered_mem[comm.rank()] = comm.get_context().register_memory(
            get_data(buffs[comm.rank()]), get_size(buffs[comm.rank()])
        );
    }

    std::vector<ucp::memory> remote_mem(comm.size());
    std::vector<ucp::rkey> remote_keys(comm.size());

    for (size_t rank : route) {
        comm.async_obtain_memory(
            rank, 
            remote_mem[rank], 
            remote_keys[rank], 
            ucp::checked_completion
        );
    }

    for (size_t rank : route) {
        comm.async_expose_memory(
            rank, 
            registered_mem[rank], 
            ucp::checked_completion
        );
    }
    comm.get_worker().fence();
    comm.get_worker().flush();
    comm.run();
    return {remote_mem, remote_keys, registered_mem};
}

void send_0_to_1_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    size_t packet_size 
) {
    if (comm.size() != 2) {
        throw std::runtime_error("This test requires world size == 2");
    }
    std::vector<char> to_send = generate_packet(packet_size, packet_size);
    std::vector<char> to_receive(packet_size);
    size_t sent_bytes = packet_size * iterations;
    
    NetStats stats; // start after data creation overhead
    stats.update_sent(sent_bytes);

    size_t iter = 0;
    if (comm.rank() == 0) {
        while (iter++ < iterations) {
            comm.async_send(1, to_send, 1);
            comm.run();
        }
    } else if (comm.rank() == 1) {
        while (iter++ < iterations) {
            comm.async_receive(to_receive, 1);
            comm.run();
        }
    }

    stats.finish();

    std::cout << "rank " << comm.rank() << " sent total of : "
        << stats.bytes_sent() / (1 << 20) << " MB" << std::endl;

    std::cout << "rank " << comm.rank() << " upstream bandwidth: "
        << stats.upstream_bandwidth() * 8 / 1000000000 << " GBit/s" << std::endl;
}

void rdma_all2all_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size
) {
    boost::mpi::environment e;
    boost::mpi::communicator w;

    router router(comm.size(), comm.rank(), std::move(routing_table));
    auto route = router();

    std::vector<packet_t> to_send(comm.size());
    std::vector<packet_t> to_receive(comm.size(), packet_t(max_packet_size));

    std::vector<uint64_t> atomics(comm.size());

    size_t sent_bytes = generate_data(
        comm, iterations, route, to_send, max_packet_size, max_packet_size
    );
    

    auto[remote_mem, remote_keys, local_mem] = exchange_metadata(comm, route, to_receive);
    auto[remote_mem_atomics, remote_keys_atomics, local_mem_atomics] = exchange_metadata(comm, route, atomics);

    for (size_t rank : route) {
        std::cout << getpid() << "route " << rank << std::endl;
    }    
    
    NetStats stats; // start after data creation and key exchange overhead
    stats.update_sent(sent_bytes);


    for (size_t i = 1; i <= iterations; ++i) {
        for (size_t rank : route) {
            comm.async_put_memory(
                rank, 
                ucp::memory(to_send[rank]), 
                (uintptr_t)remote_mem[rank].address(), 
                remote_keys[rank],
                ucp::checked_completion
            );
        }
        comm.get_worker().fence();

        for (size_t rank : route) {
            comm.atomic_post(
                rank, 
                UCP_ATOMIC_POST_OP_ADD, 
                1, 
                8, 
                (uintptr_t)remote_mem_atomics[rank].address(), 
                remote_keys_atomics[rank]
            );
        }
        comm.get_worker().fence();
        comm.run();
    }    
    comm.get_worker().flush();

    for (size_t rank : route) {
        comm.run();
        while (atomics[rank] < iterations) {
            comm.run();
        }
    }

    stats.finish();
    
    std::cout << getpid() << " rank " << comm.rank() << " sent total of : "
        << stats.bytes_sent() / (1 << 30) << " GB" << " in  " 
        << stats.seconds_passed() << std::endl;

    std::cout << getpid() << " rank " << comm.rank() << " upstream bandwidth: "
        << stats.upstream_bandwidth() * 8 / (1 << 30) << " GBit/s" << std::endl;
}

void rdma_circular_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    router::routing_table routing_table
) {
/*
    constexpr size_t buff_size = 10 * 1024 * 1024;
    constexpr size_t chunk_size = 32 * 1024;

    std::vector<packet_t> to_send(comm.size());
    std::vector<packet_t> to_receive(comm.size(), packet_t(buff_size));
    std::vector<std::pair<uint64_t, uint64_t>> pointers(comm.size());

    router router(comm.size(), comm.rank(), std::move(routing_table));
    auto route = router();
    
    size_t sent_bytes = generate_data(
        comm, iterations, route, to_send, chunk_size, chunk_size
    );

    auto[remote_mem, remote_keys, local_mem] = exchange_metadata(comm, route, to_receive);
    auto[remote_ptrs, remote_ptrs_keys, local_ptrs] = 
        exchange_metadata(comm, route, pointers);

    NetStats stats; // start after data creation and key exchange overhead
    stats.update_sent(sent_bytes);

    size_t chunk_number = 0;

    while (chunk_number < (buff_size / chunk_size) * iterations) {
        for (size_t rank : route) {
            comm.async_put_memory(
                rank, 
                ucp::memory(to_send[rank], chunk_size), 
                (uintptr_t)remote_mem[rank].address() + (chunk_size * chunk_number) % buff_size, 
                remote_keys[rank],
                ucp::checked_completion
            );
        }
        comm.get_worker().fence();

        for (size_t rank : route) {
            comm.atomic_post(
                rank, 
                UCP_ATOMIC_POST_OP_ADD, 
                1, 
                8, 
                (uintptr_t)remote_ptrs[rank].address(), 
                remote_ptrs_keys[rank]
            );
        }
        ++chunk_number;
        comm.run();
    }
    
    comm.get_worker().flush();

    stats.finish();
    
    std::cout << getpid() << " rank " << comm.rank() << " sent total of : "
        << stats.bytes_sent() / (1 << 30) << " GB" << " in  " << stats.seconds_passed() << std::endl;

    std::cout << getpid() << " rank " << comm.rank() << " upstream bandwidth: "
        << stats.upstream_bandwidth() / (1 << 30) << " GB/s" << std::endl;
*/
}

