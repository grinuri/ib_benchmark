#include <util/log.h>
#include "backend_ucx.h"
#include <boost/serialization/vector.hpp>

namespace ib_bench {

UCXBackend::UCXBackend(const ucp::communicator& comm, size_t flush_size) :
    m_world(comm),
    m_recv_req(new_recv_request()),
    m_send_buffers(size()),
    m_flush_size(flush_size)
{ }

UCXBackend::~UCXBackend() {
    // Sync all nodes before closing
    flush_send_buffers();
    BENCH_LOG_DEBUG("Communicator MPI backend waiting for all nodes to finish");
//    m_world.barrier();
}

void UCXBackend::validate_frontend_type(const std::string& type_name) {
    BENCH_LOG_DEBUG(
        boost::format("[%d] Validating comm frontend type") % rank());
/*
    m_world.barrier();
    auto reduce = [](const std::string& name1, const std::string& name2) {
        return name1 == name2 ? name1 : std::string();
    };

    std::string out;
    bmpi::reduce(m_world, type_name, out, reduce, 0);
    if (rank() == 0 && out.empty()) {
        throw std::runtime_error("Fatal error! Communicator types of "
                                 "different nodes do not match!");
    }
    if (rank() == 0) {
        BENCH_LOG_DEBUG("[0] Comm type validation successful");
    }
*/
}

ucp::request UCXBackend::new_recv_request() {
    m_arrived_size = m_world.get_worker().get_pending_size(m_world.rank());
    if (m_arrived_size) {
//        std::cout << getpid() << " probe rank " << m_world.rank() << " size " << m_arrived_size << std::endl;
        m_recv_buff.clear();
        m_recv_buff.push_back(std::string(m_arrived_size, 0));
        return m_world.async_receive(m_recv_buff[0], m_world.rank());
    }
    return ucp::request();
}

void UCXBackend::clear_send_requests() {
    m_world.get_context().poll();
    while (!m_send_reqs.empty() && !m_send_reqs.front().in_progress()) {
        m_send_reqs.pop();
    }
}

void UCXBackend::send(const msg_t& msg, size_t dest) {
    m_send_buffers[dest].push_back(msg);
    if (m_send_buffers[dest].size() >= m_flush_size) {
        flush_one_buffer(dest);
    }
}

void UCXBackend::flush_one_buffer(size_t buffer_num) {
    // If there's nothing to flush, save water!
    if (m_send_buffers[buffer_num].size() == 0) {
        return;
    }
    for (const auto& buf : m_send_buffers[buffer_num]) {
//        std::cout << getpid() << " send to tag " << buffer_num << " size " << buf.size() << std::endl;
        auto req = m_world.async_send(buffer_num, buf, buffer_num);
        m_world.get_context().poll();
        if (req.in_progress()) {
            m_send_reqs.push(std::move(req));
        }
    }
    m_send_buffers[buffer_num].clear();
    clear_send_requests();
}

void UCXBackend::flush_send_buffers() {
    for (size_t i = 0; i < size(); ++i) {
        flush_one_buffer(i);
    }
}

bool UCXBackend::done_sending() {
    // sending is done only when all send requests were cleared
    clear_send_requests();
    return m_send_reqs.empty();
}

std::optional<std::vector<UCXBackend::msg_t>> UCXBackend::try_receive() {
    m_world.get_context().poll();
    if (!m_recv_req.is_ptr()) {
        m_recv_req = new_recv_request();
        return std::nullopt;
    }
    auto got_recv = !m_recv_req.in_progress();
    if (!got_recv) {
        return std::nullopt;
    }
    auto rv = std::move(m_recv_buff);
    m_arrived_size = 0;
    m_recv_req = new_recv_request();
    return rv;
}

void UCXBackend::broadcast(const msg_t& buffer) {
    for (size_t i = 0; i < size(); ++i) {
        send(buffer, i);
    }
}

size_t UCXBackend::rank() const {
    return m_world.rank();
}

size_t UCXBackend::size() const {
    return m_world.size();
}

}
