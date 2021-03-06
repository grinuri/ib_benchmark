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
#include "exchange_metadata.h"
#include "data.h"


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
    size_t packet_size
) {
    std::cout << "World size " << comm.size() << " test: 2-sided all to all, packet_size " << 
        (packet_size / 1024) << " KB, iterations " << iterations << std::endl;
    tag_all2all(comm, iterations, routing_table, packet_size, packet_size, false);
}
void send_0_to_1_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    size_t packet_size 
) {
    std::cout << "World size " << comm.size() << " test: send 0 to 1, packet_size " << 
        (packet_size / 1024) << " KB, iterations " << iterations << std::endl;
    
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
    size_t packet_size
) {
    boost::mpi::environment e;
    boost::mpi::communicator w;

    std::cout << "World size " << comm.size() << " test: 1-sided all to all, packet_size " << 
        (packet_size / 1024) << " KB, iterations " << iterations << std::endl;
    router router(comm.size(), comm.rank(), std::move(routing_table));
    auto route = router();

    std::vector<packet_t> to_send(comm.size());
    std::vector<packet_t> to_receive(comm.size(), packet_t(packet_size));

    std::vector<uint64_t> atomics(comm.size());

    size_t sent_bytes = generate_data(
        comm, iterations, route, to_send, packet_size, packet_size
    );
    

    auto[remote_mem, remote_keys, local_mem] = exchange_metadata(comm, route, to_receive);
    auto[remote_mem_atomics, remote_keys_atomics, local_mem_atomics] = exchange_metadata(comm, route, atomics);

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
        << stats.upstream_bandwidth() * 8 / 1000000000 << " GBit/s" << std::endl;
}

void rdma_circular_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    router::routing_table routing_table,
    size_t chunk_size
) {
    constexpr size_t BUFF_SIZE = 10 * 1024 * 1024;
    if (BUFF_SIZE % chunk_size != 0) {
        throw std::runtime_error("Buffer size must be a multiple of chunk size");
    }
    size_t total_iters = (BUFF_SIZE / chunk_size) * iterations;
    
    std::cout << "World size " << comm.size() << " test: 1-side circular, buffer size " << 
        (BUFF_SIZE / 1024) << " KB, chunk size " << (chunk_size / 1024) << " KB, iterations " << iterations << std::endl;

    // send same data to all
    std::vector<char> send_area(BUFF_SIZE + 2 * sizeof(uint64_t));
    // buffer per peer
    std::vector<std::vector<char>> receive_areas(comm.size(), std::vector<char>(BUFF_SIZE + 2 * sizeof(uint64_t)));

    router router(comm.size(), comm.rank(), std::move(routing_table));
    auto route = router();
    
    size_t sent_bytes = iterations * BUFF_SIZE * route.size();
    
    std::vector<circular_adapter> to_send(comm.size(), circular_adapter(send_area));
    std::vector<circular_adapter> to_receive;
    std::transform(
        begin(receive_areas), 
        end(receive_areas), 
        std::back_inserter(to_receive), 
        [](auto& area) { return circular_adapter(area); }
    );

    auto [remote_mem, remote_keys, local_mem] = exchange_metadata(comm, route, to_receive);
    std::vector<circular_adapter> remote_circulars;
    std:transform(
        begin(remote_mem),
        end(remote_mem),
        std::back_inserter(remote_circulars),
        [](auto& mem) { return circular_adapter(static_cast<char*>(mem.address()), mem.size()); }        
    );

    size_t chunk_number = 0;
    
    NetStats stats; // start after data creation and key exchange overhead
    stats.update_sent(sent_bytes);

    while (chunk_number < total_iters) {
        for (size_t rank : route) {
            comm.async_put_memory(
                rank, 
                ucp::memory(to_send[rank].data_area(), chunk_size), 
                (uintptr_t)remote_circulars[rank].data_area() + (chunk_size * chunk_number) % BUFF_SIZE, 
                remote_keys[rank]
            );
        }
        comm.get_worker().fence();
        for (size_t rank : route) {
            comm.atomic_post(
                rank, 
                UCP_ATOMIC_POST_OP_ADD, 
                chunk_size, 
                8, 
                (uintptr_t)remote_circulars[rank].end_ptr(), 
                remote_keys[rank]
            );
        }
        ++chunk_number;
        comm.get_context().poll();
    }
    
    comm.get_worker().flush();
    
    for (auto rank : route) {
        while (*to_receive[rank].end_ptr() < total_iters * chunk_size) {
            //std::cout << getpid() << " rank " << rank << " " << to_receive[rank].begin() << " real " << *to_receive[rank].begin_ptr() << "," << to_receive[rank].end() << " real end " << *to_receive[rank].end_ptr() << std::endl;
            //std::cout << getpid() << " other rank " << comm.rank() << " " << to_receive[comm.rank()].begin() << " real " << *to_receive[comm.rank()].begin_ptr() << "," << to_receive[comm.rank()].end() << std::endl;
            comm.get_context().poll();
        }
    }
    
    stats.finish();
    
    std::cout << getpid() << " rank " << comm.rank() << " sent total of : "
        << stats.bytes_sent() / (1 << 30) << " GB" << " in  " << stats.seconds_passed() << std::endl;

    std::cout << getpid() << " rank " << comm.rank() << " upstream bandwidth: "
        << stats.upstream_bandwidth() * 8 / 1000000000 << " Gbit/s" << std::endl;
}

