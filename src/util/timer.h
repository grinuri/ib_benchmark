#pragma once

#include <chrono>


struct Timer {
using time_point_t = std::chrono::time_point<std::chrono::system_clock>;

    void start() {
        m_start_time = std::chrono::system_clock::now();
        m_running = true;
    }

    void stop() {
        m_end_time = std::chrono::system_clock::now();
        m_running = false;
    }

    double elapsed_millie_seconds() const {
        time_point_t end_time;

        if (m_running) {
            end_time = std::chrono::system_clock::now();
        } else {
            end_time = m_end_time;
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_start_time).count();
    }

    double elapsed_seconds() const {
        return elapsed_millie_seconds() / 1000.0 ;
    }

private:
    time_point_t m_start_time;
    time_point_t m_end_time;
    bool         m_running = false;
};
