#pragma once

#include <string>

#include <boost/log/utility/setup/formatter_parser.hpp>

namespace logging {

struct LogLayout {
    LogLayout(
        const std::string& format = "%Severity% %Message% - %File%:%Line%",
        const std::string& prefix = "[%TimeStamp%][%ThreadNum%] "
    ) :
        m_format{prefix + format}
    { }

    auto to_format() const {
        return boost::log::parse_formatter(m_format);
    }

    auto& str() const {
        return m_format;
    }

private:
    std::string m_format;
};


}

