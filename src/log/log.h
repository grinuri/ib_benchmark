#pragma once

#include <string>
#include <iostream>

#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "log_layout.h"
#include "level.h"

namespace logging {

struct Logger {
    using log_type = boost::log::sources::severity_channel_logger_mt<
        level, std::string
    >;

    Logger(Logger&&) = default;
    Logger(const Logger&) = delete;
    Logger(
        const std::string& name,
        std::ostream& stream,
        const LogLayout& format = LogLayout{},
        level lvl = level::info
    );
    Logger(
        const std::string& name,
        const std::string& filename,
        const LogLayout& format = LogLayout{},
        level lvl = level::info,
        bool append = true
    );
    ~Logger();
    void set_level(level lev);
    level get_level() const;
    void add_file(const std::string& filename, bool append = true);
    log_type& log();
    const std::string& name() const;
    bool is_enabled_for(level lvl) const;

private:
    std::string m_name;
    LogLayout m_format;
    level m_level;
    log_type m_log;
    std::vector<boost::shared_ptr<boost::log::sinks::basic_sink_frontend>> m_sinks;

    Logger(const std::string& name, const LogLayout& format, level lvl);
    void add_file_sink(const std::string& filename, bool append);
    void add_console_sink(std::ostream& console);
};

std::string pretty_name(std::string name);

}

#define LOCATION boost::log::add_value("FilePath", __FILE__) << \
    boost::log::add_value("File", __builtin_strrchr(__FILE__, '/') ? \
    __builtin_strrchr(__FILE__, '/') + 1 : __FILE__) << \
    boost::log::add_value("Line", __LINE__)

#define LOG(logger, lvl, expr) \
    do { BOOST_LOG_SEV(logger.log(), lvl) << LOCATION << expr; } \
    while (false)

#define LOG_FATAL(logger, expr) \
    LOG(logger, logging::level::fatal, expr)

#define LOG_ERROR(logger, expr) \
    LOG(logger, logging::level::error, expr)

#define LOG_WARN(logger, expr) \
    LOG(logger, logging::level::warn, expr)

#define LOG_INFO(logger, expr) \
    LOG(logger, logging::level::info, expr)

#define LOG_DEBUG(logger, expr) \
    LOG(logger, logging::level::debug, expr)

#define LOG_DATA(logger, expr) \
    LOG(logger, logging::level::debug_data, expr)

#define LOG_TRACE(logger, expr) \
    LOG(logger, logging::level::trace, expr)


namespace logging {

    template <class Logger>
    struct ScopeTracer {
        ScopeTracer(Logger& logger, const std::string& name) :
            m_logger{logger}, m_name{name} {}

        ~ScopeTracer() {
            LOG(m_logger, logging::level::trace_func, "--- " << m_name);
        }

    private:
        Logger& m_logger;
        std::string m_name;
    };

}

