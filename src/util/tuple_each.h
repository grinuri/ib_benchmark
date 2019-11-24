#pragma once

#include "expand.h"

namespace meta {

//@{
/// Applies the function `f` on each of the given std::tuple `t` items.
template <class... Ts, class F>
void tuple_each(std::tuple<Ts...>& t, F&& f) {
    EXPAND(Ts, f(std::get<index>(t)));
}
template <class... Ts, class F>
void tuple_each(const std::tuple<Ts...>& t, F&& f) {
    EXPAND(Ts, f(std::get<index>(t)));
}
template <class... Ts, class F>
void tuple_each(std::tuple<Ts...>&& t, F&& f) {
    EXPAND(Ts, f(std::get<index>(std::move(t))));
}
//@}

}
