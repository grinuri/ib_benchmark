#include "log.h"

#include <boost/format.hpp>

#include "log/log_layout.h"

namespace ib_bench {

static logging::level get_log_level(int level) {
    switch (level) {
    case 0: return logging::level::off;
    case 1: return logging::level::fatal;
    case 2: return logging::level::error;
    case 3: return logging::level::warn;
    case 4: return logging::level::info;
    case 5: return logging::level::debug;
    case 6: return logging::level::debug_data;
    case 7: return logging::level::trace;
    case 8: return logging::level::trace_comm;
    case 9: return logging::level::trace_func;
    default:
        throw std::exception();
    }
}

logging::Logger& logger() {
    static logging::Logger logger(
                "ibex",
                std::cout,
                logging::LogLayout(),
                get_log_level(get_default_log_level())
            );
    return logger;
}

int get_default_log_level() {
    // default level is info.
    return 9;
}

void set_logging(int level) {
    logger().set_level(get_log_level(level));
}

}
