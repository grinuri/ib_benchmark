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

void bench2(size_t iterations, size_t min_packet_size, size_t max_packet_size) {
    boost::mpi::environment env;
    boost::mpi::communicator world;
    std::vector<packet_t> in_packets(world.size());
    std::vector<packet_t> out_packets(world.size());
    std::srand(10 + world.rank());

    size_t sent_bytes = 0;
    for (int i = 0; i < world.size(); ++i) {
        in_packets[i] = generate_packet(min_packet_size, max_packet_size);
        sent_bytes += iterations * in_packets[i].size();
    }

    NetStats stats; // start after data creation overhead
    stats.update_sent(sent_bytes);

    for (size_t i = 0; i < iterations; ++i) {
        boost::mpi::all_to_all(world, in_packets, out_packets);

        for (packet_t received_packet : out_packets) {
            stats.update_received(received_packet.size());
        }
        out_packets.clear();
    }

    stats.finish();

    std::vector<size_t> sent_stats(world.size());
    std::vector<size_t> received_stats(world.size());

    sent_stats[world.rank()]     = stats.bytes_sent();
    received_stats[world.rank()] = stats.bytes_received();

    boost::mpi::all_gather(world, &sent_stats[world.rank()]    , 1, &sent_stats[0]);
    boost::mpi::all_gather(world, &received_stats[world.rank()], 1, &received_stats[0]);

    size_t total_sent = std::accumulate(sent_stats.begin(), sent_stats.end(), 0);
    size_t total_received = std::accumulate(received_stats.begin(), received_stats.end(), 0);

    if (total_sent != total_received) {
        throw std::runtime_error("total_sent != total_received)");
    }

    std::cout << "rank " << world.rank() << " sent total of : "
        << stats.bytes_sent() / (1 << 20) << " MB" << std::endl;

    std::cout << "rank " << world.rank() << " received total of : "
              << stats.bytes_received() / (1 << 20) << " MB" << std::endl;

    std::cout << "rank " << world.rank() << " upstream bandwidth: "
        << stats.upstream_bandwidth() / (1 << 20) << " MB/s" << std::endl;

    std::cout << "rank " << world.rank() << " downstream bandwidth: "
        << stats.downstream_bandwidth() / (1 << 20) << " MB/s" << std::endl;
}
