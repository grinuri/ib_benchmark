#pragma once

#include <boost/operators.hpp>

#include <sys/time.h>
#include <stdint.h>

#include <cstddef> // for NULL


namespace ib_bench {

struct gettimeofday_policy {
    typedef uint64_t time_value_t;
    static inline time_value_t now();
    static inline double to_seconds(const time_value_t &time_value);
    static inline time_value_t from_seconds(double seconds);
} ;

struct cpu_ticks_policy {
    typedef uint64_t time_value_t;
    static inline time_value_t now();
    static inline double to_seconds(const time_value_t &time_value);
    static inline time_value_t from_seconds(double seconds);

private:
    static double get_ticks_per_second();
} ;


/**
 * Class to measure accurate system time.
 */
template <typename TimePolicy>
class timer {
public:
    
    inline timer();

    /**
     * @return Elapsed time since construction or last reset.
     */
    inline double elapsed() const;

    /**
     * Reset the timer.
     */
    inline void reset();
    
    /**
     * Sets timer in a paused state, i.e. elapsed() will return constant value
     * until resume() is called.
     */
    inline void pause();
    
    /**
     * Resumes timer from a paused state.
     */
    inline void resume();

    /**
     * Returns the current time.
     */
    static inline double time();

    /**
     * Return true iff the timer is paused
     */
    inline bool is_paused() const;

private:
    
    // inline static struct timeval elapsed_since(const struct timeval& start);
    
    typename TimePolicy::time_value_t m_start;
    typename TimePolicy::time_value_t m_pause;
    bool                              m_is_paused;
};

typedef timer<gettimeofday_policy> accurate_timer;

}

#include "accurate_timer.inl"
