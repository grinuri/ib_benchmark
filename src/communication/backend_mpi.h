#pragma once

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include <utility>

#include <boost/mpi.hpp>

namespace ib_bench {

namespace bmpi = boost::mpi;

/**
 * An MPI backend for the SRCommunicator class. Uses a non-blocking send and
 * receive.
 *
 */
class MPIBackend {
public:
    using msg_t = std::string;

    /// flush_size - max size of send buffer per remote host
    MPIBackend(size_t flush_size = 1000);
    ~MPIBackend();
    MPIBackend(MPIBackend&&) = default;

public:
    /**
     * Puts the requested message in a buffer of messages. Flushes the buffer
     * when FLUSH_SIZE (static below) is reached. Can also flush manually using
     * flush_send_buffers().
     */
    void send(const msg_t& msg, size_t dest);

    /// Check if any pending send requests remain
    bool done_sending();
    std::optional<std::vector<msg_t>> try_receive();
    /// Send all data in buffers
    void flush_send_buffers();

     /// Broadcasts to all hosts (including the sending host)
    void broadcast(const msg_t& buffer);
    size_t rank() const;
    size_t size() const;

    /// A method for validating the front-end's type between different processes
    template <class FrontEnd>
    void validate_frontend_type();

private:
    void flush_one_buffer(size_t buffer_num);

     /// Returns a new MPI recv request (use at ctor or after a recv is done)
    bmpi::request new_recv_request();

    /**
     * Goes over the send requests (=possibly pending messages) and checks
     * if they were handled by the MPI. Clears the requests that were handled.
     */
    void clear_send_requests();

    bmpi::environment m_env;
    bmpi::communicator m_world;
    std::vector<msg_t> m_recv_buff;
    bmpi::request m_recv_req;
    std::queue<bmpi::request> m_send_reqs;
    std::vector<std::vector<std::string>> m_send_buffers;
    size_t m_flush_size;
};

} // namespace ib_bench

#include "backend_mpi.inl"
