#include <boost/thread/shared_mutex.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>

#include "log.h"
#include "log_layout.h"

namespace logging {

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", level)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(file, "File", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(file_path, "FilePath", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(line, "Line", unsigned)

namespace keywords = boost::log::keywords;
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;

std::string pretty_name(std::string name) {
    size_t end = name.find('(');
    if (end == std::string::npos)
        end = name.length();
    size_t start = name.rfind(' ', end);
    if (start == std::string::npos)
        start = 0;
    else
        start = start + 1;
    return name.substr(start, end - start);
}

namespace detail {

size_t generate_thread_number() {
    static std::atomic<size_t> next_number = 0;
    return ++next_number;
}

} // namespace detail

size_t thread_number() {
    thread_local size_t number = detail::generate_thread_number();
    return number;
}

Logger::Logger(
    const std::string& name,
    std::ostream& stream,
    const LogLayout& format,
    level lvl
) :
    Logger(name, format, lvl)
{
    add_console_sink(stream);
}

Logger::Logger(
    const std::string& name,
    const std::string& filename,
    const LogLayout& format,
    level lvl,
    bool append
) :
    Logger(name, format, lvl)
{
    add_file_sink(filename, append);
}

void Logger::set_level(level lev) {
    m_level = lev;
    for (auto& sink : m_sinks) {
        sink->reset_filter();
        sink->set_filter(severity >= m_level && channel == m_name);
    }
}

level Logger::get_level() const {
    return m_level;
}

void Logger::add_file(const std::string& filename, bool append) {
    add_file_sink(filename, append);
}

Logger::log_type& Logger::log() {
    return m_log;
}

Logger::Logger(
    const std::string& name,
    const LogLayout& format,
    level lvl
) :
    m_name{name},
    m_format{format},
    m_level{lvl},
    m_log{keywords::channel = name}
{
    logging::add_common_attributes();
    logging::register_simple_formatter_factory<level, char>("Severity");
    logging::core::get()->add_global_attribute(
        "ThreadNum", boost::log::attributes::make_function(&thread_number)
    );
}

Logger::~Logger() {
    try {
        for (auto& sink : m_sinks) {
            logging::core::get()->remove_sink(sink);
        }
    } catch(...) {
        std::cerr << "Exception in Logger::~Logger()\n";
    }
}

void Logger::add_file_sink(const std::string& filename, bool append) {
    auto sink = logging::add_file_log(
        keywords::file_name = filename,
        keywords::open_mode = (append ? std::ios::app : std::ios::trunc),
        keywords::auto_flush = true,
        keywords::filter = severity >= m_level && channel == m_name
    );
    sink->set_formatter(m_format.to_format());
    m_sinks.push_back(sink);
}

void Logger::add_console_sink(std::ostream& console) {
    auto sink = logging::add_console_log(
        console,
        keywords::filter = severity >= m_level && channel == m_name
    );
    sink->set_formatter(m_format.to_format());
    m_sinks.push_back(sink);
}

const std::string& Logger::name() const {
    return m_name;
}

bool Logger::is_enabled_for(level lvl) const {
    return lvl >= m_level;
}

}

