#pragma once

#include "log/log.h"

namespace ib_bench {
    logging::Logger& logger();

    void set_logging(int level);

    int get_default_log_level();
}

#define BENCH_LOG_FATAL(MSG) \
        LOG_FATAL(MSG)

#define BENCH_LOG_ERROR(MSG) \
        LOG_ERROR(ib_bench::logger(), MSG)

#define BENCH_LOG_WARN(MSG) \
        LOG_WARN(ib_bench::logger(), MSG)

#define BENCH_LOG_INFO(MSG) \
        LOG_INFO(ib_bench::logger(), MSG)

#define BENCH_LOG_DEBUG(MSG) \
        LOG_DEBUG(ib_bench::logger(), MSG)

#define BENCH_LOG_DATA(MSG) \
        LOG_DATA(ib_bench::logger(), MSG)

#define BENCH_LOG_TRACE(MSG) \
        LOG_TRACE(ib_bench::logger(), MSG)
