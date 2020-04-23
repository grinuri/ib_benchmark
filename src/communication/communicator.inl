#include <chrono>
#include <iomanip>
#include <boost/format.hpp>

#include <util/validate.h>
#include <util/tuple_each.h>
#include <util/squeue_popper.h>

#include <util/log.h>
#include <util/serialization.h>

#include "communicator.h"

namespace ib_bench {

template <class Backend, class ...ChannelTypes>
SRCommunicator<Backend, ChannelTypes...>::SRCommunicator(
    std::unique_ptr<Backend>&& backend_ptr) :
    m_backend(std::move(backend_ptr))
    {
        VALIDATE(m_backend, "Backend is empty! :(");
        m_backend-> template
            validate_frontend_type<SRCommunicator<Backend, ChannelTypes...>>();
        std::fill(m_global_eof_counters.begin(), m_global_eof_counters.end(), 0);
        std::fill(m_sync_counters.begin(), m_sync_counters.end(), 0);
        std::fill(m_ack_counters.begin(), m_ack_counters.end(), 0);
    }

template <class Backend, class ...ChannelTypes>
template <class ...BackendArgs>
SRCommunicator<Backend, ChannelTypes...>::SRCommunicator(BackendArgs&& ...args) :
    SRCommunicator(std::make_unique<Backend>(std::forward<BackendArgs>(args)...))
    { }

template <typename Backend, typename ...ChannelTypes>
SRCommunicator<Backend, ChannelTypes...>::~SRCommunicator() {
    for (size_t i = 0; i < CHANS_AMOUNT; ++i) {
        if (!m_recv_queues[i].empty()) {
            BENCH_LOG_WARN(boost::format("WARNING! receive queue %d not empty at"
                                            " Communicator destruction!") % i);
        }
        if (!m_send_queues[i].empty()) {
            BENCH_LOG_WARN(boost::format("WARNING! send queue %d not empty at "
                                            "Communicator destruction!") % i);
        }
    }
}

template <class Backend, class ...ChannelTypes>
template <size_t CHAN_NUM>
void SRCommunicator<Backend, ChannelTypes...>::send(
    const meta::list::get<chan_types_list, CHAN_NUM>& obj,
    size_t dest
) {
    DEBUG_VALIDATE(
        !m_send_queues[CHAN_NUM].eof(),
       "Cannot send on channel #" << CHAN_NUM << " after EOF has been marked."
    );
    m_send_queues[CHAN_NUM].push({util::serialize(obj), MsgType::data, dest});
}

template <class Backend, class ...ChannelTypes>
template <size_t CHAN_NUM>
auto SRCommunicator<Backend, ChannelTypes...>::receive() {
    using chan_type = meta::list::get<chan_types_list, CHAN_NUM>;
    try {
        RecvMsgProp rmsg = m_recv_queues[CHAN_NUM].pop();
        while (rmsg.type == MsgType::sync || rmsg.type == MsgType::ack) {
            handle_sync_ack_messages(rmsg, CHAN_NUM);
            rmsg = m_recv_queues[CHAN_NUM].pop();
        }
        return std::optional(util::deserialize<chan_type>(rmsg.data));
    } catch (squeue_eof& e) {
        return std::optional<chan_type>();
    }
}

template <class Backend, class ...ChannelTypes>
template <size_t CHAN_NUM>
auto SRCommunicator<Backend, ChannelTypes...>::try_receive() {
    using chan_type = meta::list::get<chan_types_list, CHAN_NUM>;
    auto opt_rmsg = m_recv_queues[CHAN_NUM].try_pop();
    while (opt_rmsg && (opt_rmsg->type == MsgType::sync
                     || opt_rmsg->type == MsgType::ack)) {
        handle_sync_ack_messages(*opt_rmsg, CHAN_NUM);
        opt_rmsg = m_recv_queues[CHAN_NUM].try_pop();
    }
    if (opt_rmsg) {
        return std::make_optional(util::deserialize<chan_type>(opt_rmsg->data));
    } else {
        return std::optional<chan_type>();
    }
}

template <class Backend, class ...ChannelTypes>
bool SRCommunicator<Backend, ChannelTypes...>::is_closed(size_t ch_num) const {
    // The channel is fully closed iff its recv queue is EOF and empty
    return m_recv_queues[ch_num].eof() && m_recv_queues[ch_num].empty();
}

template <class Backend, class ...ChannelTypes>
template <size_t CHAN_NUM>
void SRCommunicator<Backend, ChannelTypes...>::synchronize() {
    BENCH_LOG_DEBUG(
        boost::format("[%d] Synchronizing channel %d") % rank() % CHAN_NUM);
    SendMsgProp SYNC_SIGNAL = {std::to_string(rank()), MsgType::sync, 0};
    m_send_queues[CHAN_NUM].push(SYNC_SIGNAL);

    std::unique_lock l(m_sync_mutex);
    while (!m_synchronized) {
        m_sync_cond.wait(l);
    }
    m_synchronized = false;
    BENCH_LOG_DEBUG(boost::format("[%d] Synchronization of channel "
                                 "%d complete.") % rank() % CHAN_NUM);
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::mark_eof(size_t ch_num) {
    BENCH_LOG_DEBUG(boost::format(
                       "[%d] Marking EOF on channel %d")
                       % rank() % ch_num);

    SendMsgProp EOF_SIGNAL = {{}, MsgType::eof, 0};
    m_send_queues[ch_num].push(EOF_SIGNAL);
    m_send_queues[ch_num].mark_eof();
}

template <class Backend, class ...ChannelTypes>
size_t SRCommunicator<Backend, ChannelTypes...>::rank() const{
    return m_backend->rank();
}

template <class Backend, class ...ChannelTypes>
size_t SRCommunicator<Backend, ChannelTypes...>::size() const {
    return m_backend->size();
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::increment_eof_counter(
    size_t chan_num
) {
    m_global_eof_counters[chan_num]++;
    BENCH_LOG_DEBUG(boost::format(
                       "[%d] Got EOF on channel %d. Current counter: %d")
                       % rank() % static_cast<int>(chan_num) %
                       m_global_eof_counters[chan_num]);
    if (m_global_eof_counters[chan_num] == size()) {
        m_recv_queues[chan_num].mark_eof();
        BENCH_LOG_DEBUG(boost::format(
                           "[%d] COMM channel %d is now closed!")
                           % rank() % chan_num);
        m_recv_cond.notify_all();
    }
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::increment_sync_counter(
    size_t chan_num
) {
    if (++m_sync_counters[chan_num] == size()) {
        send_ack(chan_num);
    }
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::send_ack(size_t chan_num) {
    SendMsgProp ACK_SIGNAL = {std::to_string(rank()), MsgType::ack, 0};
    m_send_queues[chan_num].push(ACK_SIGNAL);
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::increment_ack_counter(
    size_t chan_num
) {
    if (++m_ack_counters[chan_num] == size()) {
        std::unique_lock l(m_sync_mutex);
        m_synchronized = true;
        m_sync_cond.notify_all();
        VALIDATE(m_sync_counters[chan_num] >= size(),
            "Internal error during comm synchronization");
        m_sync_counters[chan_num] -= size();
        m_ack_counters[chan_num] -= size();
    }
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::handle_sync_ack_messages(
    RecvMsgProp msg, size_t chan_num
) {
    if (msg.type == MsgType::sync) {
        increment_sync_counter(chan_num);
    } else if (msg.type == MsgType::ack){
        increment_ack_counter(chan_num);
    } else {
        throw std::invalid_argument("Message type is not sync or ack!");
    }
}

std::string to_hex(const std::string& str) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    std::cout << getpid() << " converting " << str.size() << std::endl;
    for (uint8_t c : str) {
        ss << std::setw(2) << (int)c << '.';
    }
    return ss.str();
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::poll_and_handle_send_queues() {
    size_t chan_id = 0;
    for (auto& sq : m_send_queues) {
        boost::optional<SendMsgProp> msg = sq.try_pop();
        while (msg) {
            // Packet structure: {data, msg_type, channel_id}
            raw_msg_t backend_msg = std::move(msg->data);
            backend_msg.push_back(static_cast<uint8_t>(msg->msg_type));
            backend_msg.push_back(chan_id);

            if (msg->msg_type == MsgType::data) {
                m_backend->send(std::move(backend_msg), msg->dest);
            } else { // EOF, sync or ACK messages
                m_backend->broadcast(std::move(backend_msg));
                // We empty the backend's buffers when sending these messages
                m_backend->flush_send_buffers();
            }
            if (msg->msg_type == MsgType::eof) {
                BENCH_LOG_DEBUG(boost::format(
                                   "[%d] Sending EOF on channel %s")
                                   % rank() % chan_id);
            }
            msg = sq.try_pop();
        }
        chan_id++;
    }
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::poll_and_handle_recv_backend() {
    auto opt_recv_vector = m_backend->try_receive();
    if (opt_recv_vector) {
        for (auto&& backend_msg : *opt_recv_vector) {
            // Packet structure: {data, msg_type, channel_id}
            uint8_t chan_id = backend_msg.back();
            backend_msg.pop_back();
            MsgType msg_type = static_cast<MsgType>(backend_msg.back());
            backend_msg.pop_back();
            if (msg_type == MsgType::eof) {
                increment_eof_counter(chan_id);
            } else if (msg_type == MsgType::sync || msg_type == MsgType::ack) {
                m_recv_queues[chan_id].push(msg_type);
            } else {
                VALIDATE(msg_type == MsgType::data, "Fatal error in communicator!");
                m_recv_queues[chan_id].push(std::move(backend_msg));
            }
        }
        m_recv_cond.notify_all();
    }
}

template <class Backend, class ...ChannelTypes>
bool SRCommunicator<Backend, ChannelTypes...>::all_done() const {
    for (size_t i = 0; i < CHANS_AMOUNT; ++i) {
        if (!is_closed(i)) { return false; }
    }
    return true;
}

template <class Backend, class ...ChannelTypes>
bool SRCommunicator<Backend, ChannelTypes...>::send_done() {
    if (m_send_done) {
        return true;
    } else if (std::all_of(m_send_queues.begin(), m_send_queues.end(),
                       [](auto& q) {return q.eof() && q.empty();}) &&
         m_backend->done_sending()) {
        m_send_done = true;
        BENCH_LOG_DEBUG(boost::format("[%d] All send queues closed") % rank());
        return true;
    }
    return false;
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::run() {
    VALIDATE(!m_all_done, "Cannot run communicator, "
                                 "all channels already closed");
    while (!all_done()) {
        if (!send_done()) {
            poll_and_handle_send_queues();
        }
        poll_and_handle_recv_backend();
        if (m_flush_timer.elapsed() > FLUSH_INTERVAL_SEC) {
            m_backend->flush_send_buffers();
            m_flush_timer.reset();
        }
    }
    BENCH_LOG_DEBUG(boost::format("[%d] COMM all done") % rank());
    m_all_done = true;
    m_extraction_cond.notify_all();
}

template <class Backend, class ...ChannelTypes>
void SRCommunicator<Backend, ChannelTypes...>::wait_recv() {
    const size_t WAIT_TIME = 500;
    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    // Heuristic. Avoids busy wait without actually synchronizing
    m_recv_cond.wait_for(lk, std::chrono::milliseconds(WAIT_TIME));
}

template <class Backend, class ...ChannelTypes>
std::unique_ptr<Backend>
    SRCommunicator<Backend, ChannelTypes...>::extract_backend() {
    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    while (!m_all_done) {
        m_extraction_cond.wait(lk);
    }
    return std::move(m_backend);
}

}

