#pragma once

#include <boost/iterator/iterator_facade.hpp>
#include <boost/optional/optional.hpp>

namespace ib_bench {

/**
 * a container stub for popping and iterating conveniently over an squeue
 * the squeue must be kept alive while the container is used
 * using several 'containers' on the same squeue will extract different
 * elements
 */
template <typename T, typename Mutex, typename ConditionVar>
class squeue_popper {
private:
    class iterator : public boost::iterator_facade<
                                iterator,
                                T,
                                boost::forward_traversal_tag> {
    public:
        /// Creates iterator pointing to end
        iterator();
        /// Creates iterator that pops from the squeue
        iterator(squeue<T, Mutex, ConditionVar>* q);

        void increment();
        T& dereference() const;

        /// Always different unless both are end
        bool equal(const iterator& other) const;
    private:
        squeue<T, Mutex, ConditionVar>* m_q;
        mutable boost::optional<T> m_popped;
    };

public:
    squeue_popper(squeue<T, Mutex, ConditionVar>* q);
    iterator begin() const;
    iterator end() const;

private:
    squeue<T, Mutex, ConditionVar>* m_q;
};

template <typename T,
          typename Mutex = std::mutex,
          typename ConditionVar = std::condition_variable>
squeue_popper<T, Mutex, ConditionVar> make_squeue_popper(squeue<T, Mutex, ConditionVar>* q);

}

#include "squeue_popper.inl"

