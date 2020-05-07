#pragma once
#include <vector>
#include <communicator.h>
#include "router.h"

namespace ib_bench {

struct metadata {
    std::vector<ucp::memory> remote_mem;
    std::vector<ucp::rkey> remote_keys;
    std::vector<ucp::registered_memory> local_mem;
};


template <class Buffers>
metadata exchange_metadata(ucp::communicator& comm, const router::route& route, Buffers& buffs) {
    std::vector<ucp::registered_memory> registered_mem(comm.size());
    for (size_t rank : route) {
        ucp::data_getter getter(buffs[rank]);
        registered_mem[rank] = comm.get_context().register_memory(
            getter.data(), getter.size()
        );
    }
    if (!registered_mem[comm.rank()]) {
        ucp::data_getter getter{buffs[comm.rank()]};
        registered_mem[comm.rank()] = comm.get_context().register_memory(
            getter.data(), getter.size()
        );
    }

    std::vector<ucp::memory> remote_mem(comm.size());
    std::vector<ucp::rkey> remote_keys(comm.size());

    for (size_t rank : route) {
        comm.async_obtain_memory(
            rank, 
            remote_mem[rank], 
            remote_keys[rank]
        );
    }

    for (size_t rank : route) {
        comm.async_expose_memory(
            rank, 
            registered_mem[rank]
        );
    }
    comm.get_worker().fence();
    comm.get_worker().flush();
    comm.run();
    return {remote_mem, remote_keys, registered_mem};
}

}
