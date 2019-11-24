#pragma once

#include <utility>

namespace meta {

/// Applies the given function on each given argument.
template <class Apply, class... Args>
void apply(Apply&& apply, Args&&... args) {
    (apply(std::forward<Args>(args)), ...);
}

}
