#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include <cereal/types/array.hpp>

#include "data.h"
#include "router.h"
#include "channel_runner.h"
#include "ucx_channel_runner.h"
#include "all_to_all_gap_runner.h"
#include "all_to_all_gap_runner_shmem.h"
#include "bench2.h"
#include "ucx.h"
#include <communicator.h>


using namespace ib_bench;

void bench0(
    size_t run_iters,
    size_t flush_size,
    size_t sync_iters,
    router::routing_table routing_table
) {
    return channel_runner<
        ct_ints<2>,
        ct_ints<8>,
        ct_ints<16>,
        ct_ints<64>,
        ct_ints<1024>,
        ct_ints<2048>
    >{
        run_iters,
        flush_size,
        sync_iters,
        {1, 2, 3, 2, 1, 0},
        std::move(routing_table)
    }.run();
}

void bench0_ucx(
    const ucp::communicator& comm,
    size_t run_iters,
    size_t flush_size,
    size_t sync_iters,
    router::routing_table routing_table
) {
    return ucx_channel_runner<
        ct_ints<2>,
        ct_ints<8>,
        ct_ints<16>,
        ct_ints<64>,
        ct_ints<1024>,
        ct_ints<2048>
    >{
        comm,
        run_iters,
        flush_size,
        sync_iters,
        {1, 2, 3, 2, 1, 0},
        std::move(routing_table)
    }.run();
}

void bench1(size_t run_iters, int max_gap,router::routing_table routing_table, size_t byte_count) {
    all_to_all_gap_runner runner{run_iters, max_gap, std::move(routing_table), byte_count};
    runner.run();
}

void bench5(size_t run_iters, int max_gap,router::routing_table routing_table) {
    all_to_all_gap_runner<(1024 * 8)> runner{run_iters, max_gap, std::move(routing_table)};
    runner.run();
}

void bench4(size_t run_iters, int max_gap,router::routing_table routing_table, size_t byte_count) {
    all_to_all_gap_runner_shmem{run_iters, max_gap, std::move(routing_table), byte_count}.run();
}

int main(int argc, char** argv) {
    using namespace std;
    if (argc == 1) {
        cerr << "Use: ./test 0 run_iterations routing_table_file flush_size sync_iterations\n";
        cerr << "  or ./test 1 run_iterations routing_table_file max_gap packet_size\n";
        cerr << "  or ./test 2 run_iterations routing_table_file packet_size\n";
        cerr << "  or ./test 3 run_iterations min_packet_size max_packet_size\n";
        cerr << "  or (like 1, but shmem) ./test 4 run_iterations routing_table_file max_gap packet_size\n";
        cerr << "  or ./test 5 run_iterations routing_table_file max_gap\n";
        return -1;
    }
    char *end = nullptr;
    int test_num = strtol(argv[1], &end, 10);
    size_t run_iters = strtoul(argv[2], &end, 10);
    std::string routing_table_name;
    router::routing_table routing_table;

    if (test_num != 2) {
        routing_table_name = argv[3];
        routing_table = load_routing_table(routing_table_name);
    }
    namespace mpi = boost::mpi;

    size_t world_size = 2;
    auto var = std::getenv("OMPI_COMM_WORLD_SIZE");
    if (var) {
      static mpi::environment env;
      static mpi::communicator mpi_comm;
      world_size = mpi_comm.size();
    }

    auto comm = var ? ucp::create_world<ucp::oob::mpi::connector>(world_size, true) :
        ucp::create_world<ucp::oob::tcp_ip::connector>(world_size, true);

    switch (test_num) {
        case 0: bench0(run_iters, strtoul(argv[4], &end, 10), strtoul(argv[5], &end, 10), std::move(routing_table)); break;
        case 1: bench1(run_iters, strtol(argv[4], &end, 10), std::move(routing_table), strtoul(argv[5], &end, 10)); break;
        case 2: bench1(run_iters, 1, std::move(routing_table), strtoul(argv[4], &end, 10)); break;
        case 3: {
            size_t min_packet_size = strtoul(argv[3], &end, 10);
            size_t max_packet_size = strtoul(argv[4], &end, 10);
            bench2(run_iters, min_packet_size, max_packet_size);
            break;
        }
        case 4: bench4(run_iters, strtol(argv[4], &end, 10), std::move(routing_table), strtoul(argv[5], &end, 10)); break;
        case 5: bench5(run_iters, strtol(argv[4], &end, 10), std::move(routing_table)); break;

        case 21: {
            size_t min_packet_size = strtoul(argv[4], &end, 10);
            size_t max_packet_size = strtoul(argv[5], &end, 10);
            tag_all2all_variable(comm, run_iters, std::move(routing_table), min_packet_size, max_packet_size);
            break;
        }
        case 22: {
            size_t min_packet_size = strtoul(argv[4], &end, 10);
            size_t max_packet_size = strtoul(argv[5], &end, 10);
            tag_all2all_fixed(comm, run_iters, std::move(routing_table), min_packet_size, max_packet_size);
            break;
        }
        case 23: {
            size_t min_packet_size = strtoul(argv[4], &end, 10);
            size_t max_packet_size = strtoul(argv[5], &end, 10);
            rdma_all2all_ucx(comm, run_iters, std::move(routing_table), min_packet_size, max_packet_size);
            break;
        }
        case 24: {
            rdma_circular_ucx(comm, run_iters, std::move(routing_table));
            break;
        }
        case 25: bench0_ucx(comm, run_iters, strtoul(argv[4], &end, 10), strtoul(argv[5], &end, 10), std::move(routing_table)); break;
        case 26: {
            size_t packet_size = strtoul(argv[4], &end, 10);
            send_0_to_1_ucx(comm, run_iters, packet_size);
            break;
        }
        default: cerr << "test number " << test_num << " does not exist\n";
    }
    return 0;
}

