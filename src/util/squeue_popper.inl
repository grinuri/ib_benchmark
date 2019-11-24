
namespace ib_bench {

template <typename T, typename Mutex, typename ConditionVar>
squeue_popper<T, Mutex, ConditionVar>::iterator::iterator() : m_q(nullptr) { }

template <typename T, typename Mutex, typename ConditionVar>
squeue_popper<T, Mutex, ConditionVar>::iterator::
iterator(squeue<T, Mutex, ConditionVar>* q) : m_q(q) {
    // initializing m_popped with value, or making the iterator end
    increment();
}

template <typename T, typename Mutex, typename ConditionVar>
void squeue_popper<T, Mutex, ConditionVar>::iterator::increment() {
    try {
        m_popped = m_q->pop();
    } catch(squeue_eof& e) {
        m_popped.reset();
    }
}

template <typename T, typename Mutex, typename ConditionVar>
T& squeue_popper<T, Mutex, ConditionVar>::iterator::dereference() const {
    return *m_popped;
}

template <typename T, typename Mutex, typename ConditionVar>
bool squeue_popper<T, Mutex, ConditionVar>::iterator::
equal(const iterator& other) const {
    return !m_popped && !other.m_popped;
}

template <typename T, typename Mutex, typename ConditionVar>
squeue_popper<T, Mutex, ConditionVar>::squeue_popper(squeue<T, Mutex, ConditionVar>* q) : m_q(q) { }

template <typename T, typename Mutex, typename ConditionVar>
typename squeue_popper<T, Mutex, ConditionVar>::iterator
squeue_popper<T, Mutex, ConditionVar>::begin() const {
    return iterator(m_q);
}

template <typename T, typename Mutex, typename ConditionVar>
typename squeue_popper<T, Mutex, ConditionVar>::iterator
squeue_popper<T, Mutex, ConditionVar>::end() const {
    return iterator();
}

template <typename T, typename Mutex, typename ConditionVar>
squeue_popper<T, Mutex, ConditionVar>
make_squeue_popper(squeue<T, Mutex, ConditionVar>* q) {
    return squeue_popper<T, Mutex, ConditionVar>(q);
}

}
