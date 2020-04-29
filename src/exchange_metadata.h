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
