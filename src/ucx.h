#pragma once

#include <iostream>
#include <vector>
#include <communicator.h>
#include "router.h"

void tag_all2all_variable(
    ucp::communicator& comm, 
    size_t iterations, 
    ib_bench::router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size
);
void tag_all2all_fixed(
    ucp::communicator& comm, 
    size_t iterations, 
    ib_bench::router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size
);
void rdma_all2all_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    ib_bench::router::routing_table routing_table, 
    size_t min_packet_size, 
    size_t max_packet_size
);
void rdma_circular_ucx(
    ucp::communicator& comm, 
    size_t iterations, 
    ib_bench::router::routing_table routing_table
);
