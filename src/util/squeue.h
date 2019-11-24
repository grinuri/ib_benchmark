#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

#include <boost/optional.hpp>

namespace ib_bench {

struct error {};

struct squeue_eof : error {
};

/**
 * An implementation of a shared (or synchronized) queue.
 * Based on an std::queue, and various concurrency primitives.
 *
 * @tparam T             The type to hold in the queue.
 * @tparam Mutex         A mutex type.
 * @tparam ConditionVar  A condition variable type.
 *
 * @note Setting the mutex and condition variable allows to work with different
 * kinds of thread type, such as boost::fibers.
 */
template <typename T,
          typename Mutex = std::mutex,
          typename ConditionVar = std::condition_variable>
struct squeue {
    using mutex_t = Mutex;
    using condition_variable_t = ConditionVar;

    /// Held elements' type. 
    using value_type = T;

    squeue();
    squeue(const squeue&) = delete;
    squeue(squeue&&) = delete;
    squeue& operator=(const squeue&) = delete;
    squeue& operator=(squeue&&) = delete;

    /**
     * Push an object to the queue.
     * The queue is of infinite size, and will resize accordingly.
     *
     * @param obj The object to insert.
     */
    void push(T obj);

    /**
     * Pops an object from the queue.
     * The call will always return with a valid object; Will
     * sleep as long as needed if the queue is empty.
     *
     * @return The popped element.
     */
    T pop();

    /**
     * Tries to pop an object from the queue atomically.
     * If the queue is empty, empty boost::optional is returned.
     * In any case, the function will not block.
     *
     * @return The popped element.
     */
    boost::optional<T> try_pop();

    boost::optional<T> timed_pop(double timeout);

    /**
     * @return The top element of the queue, if exists.
     */
    boost::optional<T> top();

    /**
     * Returns the number of elements that exist in the queue at the moment.
     */
    size_t size() const;

    /**
     * @returns True if the queue is empty, False otherwise.
     */
    bool empty() const;

    /**
     * Notifies the queue that eof has been reached.
     * Implications: all pushes and all pops (once the queue becomes empty)
     * will result in queue_eof exception throw, including pops that are
     * currently blocked waiting.
     */
    void mark_eof();

    /**
     * @returns True if the queue was marked as eof, and False otherwise.
     */
    bool eof() const;

    /**
     * Resets the queue.
     * DO NOT call while the queue is used by another thread
     * will raise error if queue not empty
     */
    void reset();

protected:

    /**
     * Performs recurrent pop routine from underlining queue.
     *
     * @return The popped element
     */
    T inner_pop();

    /** Our magnificent queue. */
    std::queue<T> m_queue;
    /** Used to synchronize queue access. */
    mutex_t m_mutex;
    /** Used to to sleep on when queue is empty. */
    condition_variable_t m_cond;
    /** Used to mark that the queue reached eof. */
    bool m_eof;

};

}

#define __WITHIN_SQUEUE_H__
#include "squeue.inl"
#undef __WITHIN_SQUEUE_H__

