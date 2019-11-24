#ifndef __WITHIN_SQUEUE_H__
#error "Do not directly include this file."
#endif

namespace ib_bench {

template <typename T, typename Mutex, typename ConditionVar>
squeue<T, Mutex, ConditionVar>::squeue() : m_eof(false) {
}

template <typename T, typename Mutex, typename ConditionVar>
void squeue<T, Mutex, ConditionVar>::push(T obj) {
    {
        std::lock_guard<mutex_t> lock(m_mutex);
        if (m_eof) {
            throw squeue_eof();
        }
        m_queue.push(std::move(obj));
    }
    m_cond.notify_one();
}

template <typename T, typename Mutex, typename ConditionVar>
T squeue<T, Mutex, ConditionVar>::pop() {
    std::unique_lock<mutex_t> lock(m_mutex);
    while (m_queue.empty()) {
        if (m_eof) {
            throw squeue_eof();
        }
        m_cond.wait(lock);
    }
    return inner_pop();
}

template <typename T, typename Mutex, typename ConditionVar>
boost::optional<T> squeue<T, Mutex, ConditionVar>::try_pop() {
    std::unique_lock<mutex_t> lock(m_mutex);
    if (m_queue.empty())
        return boost::none;
    return inner_pop();
}

template <typename T, typename Mutex, typename ConditionVar>
boost::optional<T> squeue<T, Mutex, ConditionVar>::timed_pop(double timeout) {
    using std::chrono::microseconds;
    microseconds dur(static_cast<microseconds::rep>(timeout * 1e6));
    std::unique_lock<mutex_t> lock(m_mutex);
    while (m_queue.empty()) {
        if (m_cond.wait_for(lock, dur) == std::cv_status::timeout) {
            // timed-out
            return boost::none;
        }
    }
    return inner_pop();
}

template <typename T, typename Mutex, typename ConditionVar>
boost::optional<T> squeue<T, Mutex, ConditionVar>::top() {
    std::unique_lock<mutex_t> lock(m_mutex);
    if (m_queue.empty())
        return boost::none;
    return m_queue.front();
}

template <typename T, typename Mutex, typename ConditionVar>
size_t squeue<T, Mutex, ConditionVar>::size() const {
    return m_queue.size();
}

template <typename T, typename Mutex, typename ConditionVar>
bool squeue<T, Mutex, ConditionVar>::empty() const {
    return m_queue.empty();
}

template <typename T, typename Mutex, typename ConditionVar>
void squeue<T, Mutex, ConditionVar>::mark_eof() {
    {
        std::unique_lock<mutex_t> lock(m_mutex);
        m_eof = true;
    }
    m_cond.notify_all();
}

template <typename T, typename Mutex, typename ConditionVar>
bool squeue<T, Mutex, ConditionVar>::eof() const {
    return m_eof;
}

template <typename T, typename Mutex, typename ConditionVar>
void squeue<T, Mutex, ConditionVar>::reset() {
    if (!m_queue.empty()) {
        throw error();
    }
    m_eof = false;
}

template <typename T, typename Mutex, typename ConditionVar>
T squeue<T, Mutex, ConditionVar>::inner_pop() {
    T ret = std::move(m_queue.front());
    m_queue.pop();
    return ret;
}

}

