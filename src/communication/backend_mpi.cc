#include <util/log.h>
#include "backend_mpi.h"
#include <boost/serialization/vector.hpp>

namespace ib_bench {

MPIBackend::MPIBackend(size_t flush_size) :
    m_env(boost::mpi::threading::single),
    m_recv_req(new_recv_request()),
    m_send_buffers(size()),
    m_flush_size(flush_size) {
}

MPIBackend::~MPIBackend() {
    // Sync all nodes before closing
    flush_send_buffers();
    BENCH_LOG_DEBUG("Communicator MPI backend waiting for all nodes to finish");
    m_world.barrier();
}

bmpi::request MPIBackend::new_recv_request() {
    return m_world.irecv(bmpi::any_source, 0, m_recv_buff);
}

void MPIBackend::clear_send_requests() {
    while (!m_send_reqs.empty() && m_send_reqs.front().test()) {
        m_send_reqs.pop();
    }
}

void MPIBackend::send(const msg_t& msg, size_t dest) {
    m_send_buffers[dest].push_back(msg);
    if (m_send_buffers[dest].size() >= m_flush_size) {
        flush_one_buffer(dest);
    }
}

void MPIBackend::flush_one_buffer(size_t buffer_num) {
    // If there's nothing to flush, save water!
    if (m_send_buffers[buffer_num].size() == 0) {
        return;
    }
    auto req = m_world.isend(buffer_num, 0, std::move(m_send_buffers[buffer_num]));
    if (!req.test()) {
        m_send_reqs.push(req);
    }
    m_send_buffers[buffer_num].clear();
    clear_send_requests();
}

void MPIBackend::flush_send_buffers() {
    for (size_t i = 0; i < size(); ++i) {
        flush_one_buffer(i);
    }
}

bool MPIBackend::done_sending() {
    // sending is done only when all send requests were cleared
    clear_send_requests();
    return m_send_reqs.empty();
}

auto MPIBackend::try_receive() -> std::optional<std::vector<msg_t>> {
    auto got_recv = m_recv_req.test();
    if (!got_recv) {
        return std::nullopt;
    }
    auto rv = std::move(m_recv_buff);
    m_recv_req = new_recv_request();
    return rv;
}

void MPIBackend::broadcast(const msg_t& buffer) {
    for (size_t i = 0; i < size(); ++i) {
        send(buffer, i);
    }
}

size_t MPIBackend::rank() const {
    return m_world.rank();
}

size_t MPIBackend::size() const {
    return m_world.size();
}

}
