#pragma once

#include <ostream>

namespace logging {

enum class level {
    off = 0x7fffffff,
    fatal = 50000,
    error = 40000,
    warn = 30000,
    info = 20000,
    debug = 10000,
    debug_data = 9999,
    trace = 5000,
    trace_comm = 4999,
    trace_func = 4998
};

template<typename CharT, typename TraitsT>
std::basic_ostream<CharT, TraitsT>& operator<<(
    std::basic_ostream<CharT, TraitsT> &out, level lvl
) {
    switch (lvl)
    {
        case level::trace_func:     return out << "TRACE_FUNC";
        case level::trace_comm:     return out << "TRACE_COMM";
        case level::trace:          return out << "TRACE";
        case level::debug_data:     return out << "DEBUG_DATA";
        case level::debug:          return out << "DEBUG";
        case level::info:           return out << "INFO";
        case level::warn:           return out << "WARN";
        case level::error:          return out << "ERROR";
        case level::fatal:          return out << "FATAL";
        case level::off:            return out << "OFF";
        default:                    return out << static_cast<int>(lvl);
    }
}

}

