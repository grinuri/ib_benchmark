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

template <class T, class Enable = void>
struct data_getter {
    data_getter(T& t) : m_t(t) { }
    auto get_data() const {
        return m_t.data();
    }

    auto get_size() const {
        return m_t.size();
    }

private:
    T& m_t;
};

template <class T>
struct data_getter<T, std::enable_if_t<std::is_pod_v<T>>> {
    data_getter(T& t) : m_t(t) { }
    auto get_data() const {
        return &m_t;
    }

    auto get_size() const {
        return sizeof(m_t);
    }

private:
    T& m_t;
};

template <class T, class U>
struct data_getter<std::pair<T, U>> {
    data_getter(std::pair<T, U>& t) : m_t(t) { }
    auto get_data() const {
        std::cout << " pair getter data " << &m_t << std::endl;
        return &m_t;
    }

    auto get_size() const {
        std::cout << " pair getter size " << sizeof(m_t) << std::endl;
        return sizeof(m_t);
    }
    
private:
    std::pair<T, U>& m_t;
};


template <class Buffers>
metadata exchange_metadata(ucp::communicator& comm, const router::route& route, Buffers& buffs) {
    std::vector<ucp::registered_memory> registered_mem(comm.size());
    for (size_t rank : route) {
        data_getter getter(buffs[rank]);
        registered_mem[rank] = comm.get_context().register_memory(
            getter.get_data(), getter.get_size()
        );
    }
    if (!registered_mem[comm.rank()]) {
        data_getter getter{buffs[comm.rank()]};
        registered_mem[comm.rank()] = comm.get_context().register_memory(
            getter.get_data(), getter.get_size()
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
