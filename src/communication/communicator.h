#pragma once

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <tuple>
#include <util/squeue.h>
#include <util/list.h>
#include <util/accurate_timer.h>
#include <numeric>

#include "util/log.h"
#include <iostream>

namespace ib_bench {

/***
 *  2-Way Communicator. The Communicator consists of 2-way channels
 *  (each given as a separate template argument), and provides a compile-time type-safe interface
 *  for sending and receiving over each channel, assuming all of the hosts share the same executable.
 *  The communicator provides an EOF interface for a host to declare that it will send no further data,
 *  allowing each host to determine when a channel is closed (if all of the hosts declare
 *  EOF). This ensures that no data is lost, and no host stops listening before all is done.
 *
 *  A typical use of this class would be the following:
 *
 *  1) A thread would be opened to use run(), which contains the main polling loop.
 *  2) Worker threads would use the send<N>() and receive<N>() interfaces to communicate over the N'th channel.
 *     Once all sending is done, mark_eof(N) is called.
 *  3) Receiving continues until receive<N>() returns an empty optional. At this point, the N'th channel is
 *     considered closed.
 *  4) Once all channels are closed, run() will exit.
 *
 * @tparam Backend - Backend communication class. Required to have a non-blocking send(msg),
 * and try_receive(), broadcast(msg), done_sending(), size(), rank() interfaces.
 * done_sending() is an interface for checking if the backend has finished sending all messages.
 * size() returns the total amount of nodes, and rank() returns the current node's index.
 * @tparam ChannelTypes - Channel types
 */
template <class Backend, class ...ChannelTypes>
class SRCommunicator {
public:
    SRCommunicator(SRCommunicator<Backend, ChannelTypes...>&&) = delete;
    static constexpr size_t CHANS_AMOUNT = sizeof...(ChannelTypes);
    static_assert(CHANS_AMOUNT <= 256, "Maximum allowed channels is 256");
    using raw_msg_t = typename Backend::msg_t;
    using chan_types_list = typename meta::list::of<ChannelTypes...>;

    explicit SRCommunicator(std::unique_ptr<Backend>&& backend_ptr);

    template <class ...BackendArgs>
    explicit SRCommunicator(BackendArgs&& ...args);
    ~SRCommunicator();

    template <size_t CHAN_NUM>
    void send(const meta::list::get<chan_types_list, CHAN_NUM>& obj,
        size_t dest);

    /**
     * Blocking receive. Returns std::optional<CHANNEL_TYPE>. An empty optional
     * signals that the channel is closed.
     */
    template <size_t CHAN_NUM>
    auto receive();

    /// Non-blocking receive. Returns nullopt if nothing to receive
    template <size_t CHAN_NUM>
    auto try_receive();

    bool is_closed(size_t ch_Num) const;

    /**
     * Wait until 1 and 2 are satisfied for all nodes:
     * 1) synchronize() was called.
     * 2) messages that other nodes had sent before calling synchronize()
     *    were handled.
     * WARNING: CHAN_NUM's receive handler must be single threaded to use this!
     */
    template <size_t CHAN_NUM>
    void synchronize();

    /**
     * Marks the channel as EOF, meaning that this node guarantees that after
     * it empties the channel's send queue it will send no more data. Calling
     * send() on an EOF channel throws.
     */
    void mark_eof(size_t ch_num);

     /// @return rank of the given node
    size_t rank() const;

     /// @return amount of communicated nodes (including current one)
    size_t size() const;

    /**
     *  Runs the communicator's main poll loop, which polls the backend for
     *  receiving, and the communicator's send queues for sending, and handles
     *  appropriately. Stops once all channels are closed.
     *  Execute in a dedicated thread.
     */
    void run();

     /// Wait until the Communicator receives any message (cond. wait)
    void wait_recv();

    /**
     * Block wait until ALL communication is over, then extract the backend
     * and return it.
     */
    std::unique_ptr<Backend> extract_backend();
private:
    enum class MsgType : uint8_t {
        data = 0,
        eof = 1,
        sync = 2,
        ack = 3
    };

    struct SendMsgProp {
        raw_msg_t data;
        MsgType msg_type;
        size_t dest;
    };

    struct RecvMsgProp {
        raw_msg_t data;
        MsgType type;

        RecvMsgProp(raw_msg_t data) :
            data(std::move(data)), type(MsgType::data) { }
        RecvMsgProp(MsgType type) : type(type) { }
    };

    /**
     * Increments a counter that counts the amount of nodes that have marked
     * the channel EOF. Once all nodes have marked EOF, the respective recv
     * queue is marked EOF.
     */
    void increment_eof_counter(size_t chan_num);

    void increment_sync_counter(size_t chan_num);

    void send_ack(size_t chan_num);

    void increment_ack_counter(size_t chan_num);

    void handle_sync_ack_messages(RecvMsgProp msg, size_t chan_num);

     /// Poll all the send queues once and send any given messages
    void poll_and_handle_send_queues();

     /// Poll the backend for any received messages and handle them
    void poll_and_handle_recv_backend();

     /// @return true if all channels are closed
    bool all_done() const;

     /// @return true if channel has been marked EOF and send queue empty
    bool send_done();

    using send_queue_t = squeue<SendMsgProp>;
    using recv_queue_t = squeue<RecvMsgProp>;

    std::unique_ptr<Backend> m_backend;
    std::array<send_queue_t, CHANS_AMOUNT> m_send_queues;
    std::array<recv_queue_t, CHANS_AMOUNT> m_recv_queues;
    std::array<size_t, CHANS_AMOUNT> m_global_eof_counters;
    std::array<size_t, CHANS_AMOUNT> m_sync_counters;
    std::array<size_t, CHANS_AMOUNT> m_ack_counters;

    mutable std::condition_variable m_recv_cond;
    mutable std::condition_variable m_sync_cond;
    mutable std::condition_variable m_extraction_cond;
    mutable std::mutex m_sync_mutex;

    bool m_synchronized = false;
    bool m_send_done = false;
    bool m_all_done = false;

    accurate_timer m_flush_timer;
    static constexpr double FLUSH_INTERVAL_SEC = 1e-2;
};

}

#include "communicator.inl"
