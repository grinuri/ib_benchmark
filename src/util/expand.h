#pragma once

#include <utility>

#include "apply.h"

namespace meta::detail {

template <class T, size_t INDEX>
struct ExpandedItem {
    using type = T;
    static constexpr size_t index = INDEX;
};

template <class... Ts, size_t... I, class Apply>
void expand_with_indices(const Apply& apply, std::index_sequence<I...>) {
    (apply(ExpandedItem<Ts, I>{}), ...);
};

template <class... Ts, class Apply>
void expand(const Apply& apply) {
    expand_with_indices<Ts...>(apply, std::index_sequence_for<Ts...>{});
}

}

/// Defines `type` as `item::type` and `index` as `item::index` in scope.
#define EXPAND_ITEM(item)\
    using type [[maybe_unused]] = typename decltype(item)::type;\
    [[maybe_unused]] constexpr size_t index = decltype(item)::index;\

/**
 * Expand the given type parameter pack, applying `code_block` to each type.
 *
 * The symbols `type` and `index` are available inside the `code_block`. `type`
 * is the expanded type parameter, and `index` is its zero-based index.
 */
#define EXPAND(type_pack, code_block...)\
    meta::detail::expand<type_pack...>([&](auto item) {\
        EXPAND_ITEM(item);\
        code_block;\
    })
