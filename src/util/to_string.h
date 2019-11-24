#pragma once

#include <sstream>

namespace ib_bench {

/// @return The result of ostream-ing the given value.
constexpr auto to_string = [](const auto& v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
};

}
