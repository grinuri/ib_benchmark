#pragma once

#include <type_traits>

namespace meta {

/**
 * Can be used as an empty type (like void).
 *
 * Useful when void is a valid type like any other, and a specific type is
 * needed to differentiate it from "no type".
 */
struct none {
    none() = delete;
    ~none() = delete;
    none(const none&) = delete;
    none& operator=(none) = delete;
};

/// `std::true_type` when `T` is `none`.
template <class T>
using is_none = std::is_same<T, none>;

/// Equivalent to `is_none<T>::value`.
template <class T>
constexpr bool is_none_v = is_none<T>::value;

/// `std::false_type` when `T` is `none`.
template <class T>
using is_not_none = std::integral_constant<bool, !is_none_v<T>>;

/// Equivalent to `is_not_none<T>::value`.
template <class T>
constexpr bool is_not_none_v = is_not_none<T>::value;

}
