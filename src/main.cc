#include <boost/serialization/array.hpp>
#include <cereal/types/array.hpp>
#include "data.h"
#include "router.h"
#include "channel_runner.h"
#include "all_to_all_gap_runner.h"
#include "bench2.h"

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

void bench1(size_t run_iters, int max_gap,router::routing_table routing_table, size_t int_count) {
    std::cout << "Iterations: " << run_iters << " packet size " << int_count << " ints " << " max gap " << max_gap << std::endl;
    all_to_all_gap_runner<1024>{run_iters, max_gap, std::move(routing_table), int_count}.run();
}

int main(int argc, char** argv) {
    using namespace std;
    if (argc == 1) {
        cerr << "Use: ./test 0 run_iterations routing_table_file flush_size sync_iterations\n";
        cerr << "  or ./test 1 run_iterations routing_table_file max_gap packet_size\n";
        cerr << "  or ./test 2 run_iterations routing_table_file packet_size\n";
        cerr << "  or ./test 3 run_iterations min_packet_size max_packet_size\n";
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
        default: cerr << "test number " << test_num << " does not exist\n";
    }
    return 0;
}

