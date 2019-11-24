namespace ib_bench {

template <typename TimePolicy>
timer<TimePolicy>::timer() {
    reset();
}

template <typename TimePolicy>
double timer<TimePolicy>::elapsed() const {
    if (!m_is_paused) {
        return TimePolicy::to_seconds(TimePolicy::now() - m_start);
    } else {
        return TimePolicy::to_seconds(m_pause - m_start);
    }
}

template <typename TimePolicy>
void timer<TimePolicy>::reset() {
    m_start = TimePolicy::now();
    m_is_paused = false;
}

template <typename TimePolicy>
void timer<TimePolicy>::pause() {
    m_pause = TimePolicy::now();
    m_is_paused = true;
}

template <typename TimePolicy>
void timer<TimePolicy>::resume() {
    m_start += (TimePolicy::now() - m_pause);
    m_is_paused = false;
}

template <typename TimePolicy>
bool timer<TimePolicy>::is_paused() const {
    return m_is_paused;
}

template <typename TimePolicy>
inline double timer<TimePolicy>::time() {
    return TimePolicy::to_seconds(TimePolicy::now());
}

gettimeofday_policy::time_value_t gettimeofday_policy::now() {
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    return tv_now.tv_sec * static_cast<time_value_t>(1000000) +
            tv_now.tv_usec;
}

double gettimeofday_policy::to_seconds(const time_value_t &time_value) {
    return time_value / 1e6;
}

gettimeofday_policy::time_value_t gettimeofday_policy::from_seconds(double seconds) {
    return static_cast<time_value_t>(seconds * 1e6);
}

cpu_ticks_policy::time_value_t cpu_ticks_policy::now() {
    uint32_t a, d;
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return static_cast<uint64_t>(a) | (static_cast<uint64_t>(d) << 32);
}

double cpu_ticks_policy::to_seconds(const time_value_t &time_value) {
    return time_value / get_ticks_per_second();
}

cpu_ticks_policy::time_value_t cpu_ticks_policy::from_seconds(double seconds) {
    return static_cast<time_value_t>(seconds * get_ticks_per_second());
}

}
