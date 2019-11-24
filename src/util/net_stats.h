#pragma once

#include <util/timer.h>

struct NetStats {
    NetStats() {
        m_timer.start();
    }

    size_t bytes_sent()     const { return m_bytes_sent; }

    size_t bytes_received() const { return m_bytes_received;}

    double seconds_passed() const { return m_timer.elapsed_seconds(); }

    void finish() {
        m_timer.stop();
        m_running = false;
    }

    void update_received(size_t num_bytes) {
        m_bytes_received += num_bytes;
    }

    void update_sent(size_t num_bytes) {
        m_bytes_sent += num_bytes;
    }

    double upstream_bandwidth() const {
        return (double)(m_bytes_sent) / m_timer.elapsed_seconds();
    }

    double downstream_bandwidth() const {
        return (double)(m_bytes_received) / m_timer.elapsed_seconds();
    }

private:
    bool   m_running        = true;
    Timer  m_timer;
    size_t m_bytes_sent     = 0;
    size_t m_bytes_received = 0;
};