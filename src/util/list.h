#pragma once

#include <cstddef>

#include "none.h"
#include "type_constant.h"

/**
 * Provides tools for working with lists of types.
 *
 * A meta list is any template name `Template` with types `Ts...`
 * ~~~{.cpp}
 * meta::list::size<std::tuple<int, bool, char>>;
 * meta::list::size<std::variant<int, bool, char>>;
 * meta::list::size<std::pair<int, bool>>;
 *
 * // meta::list::of can be used as a simple list of types without any added
 * // functionality
 * meta::list::size<meta::list::of<int, bool, char>>;
 * ~~~
 */
namespace meta::list {

namespace detail {

template <class>    struct helper;
template <class...> struct concat_helper;

}

/// Can be used when a simple meta::list is needed.
template <class...> struct of {};

/// The size of the meta::list.
template <class List>
constexpr size_t size = detail::helper<List>::size;

/// `true` if the the meta::list is empty.
template <class List>
constexpr bool empty = (size<List> == 0);

/// Appends `T` to the meta::list.
template <class List, class T>
using push_back = typename detail::helper<List>::template push_back<T>;

/// Prepends `T` to the meta::list.
template <class List, class T>
using push_front = typename detail::helper<List>::template push_front<T>;

/// Appends `T` to the meta::list if `CONDITION` is `true`.
template <class List, bool CONDITION, class T>
using conditional_push_back =
    typename detail::helper<List>::template conditional_push_back<CONDITION, T>;

/// The `I`th type of the meta::list.
template <class List, size_t I>
using get = typename detail::helper<List>::template get<I>;

/// The `I`th type of the meta::list or meta::none if `I` is out of range.
template <class List, size_t I>
using get_or_none = typename detail::helper<List>::template get_or_none<I>;

/// The index of the first appearance of `T` in the meta::list.
template <class List, class T>
constexpr size_t index = detail::helper<List>::template index<T>;

/// The first type of the meta::list.
template <class List>
using front = get<List, 0>;

/// The last type of the meta::list.
template <class List>
using back = get<List, size<List> - 1>;

/// The meta::list with each type transformed using the meta-function `MF`.
template <class List, template <class> class MF>
using transform = typename detail::helper<List>::template transform<MF>;

/// Unpacks the types of the meta::list into `Template`.
template <class List, template <class...> class Template>
using unpack = typename detail::helper<List>::template unpack<Template>;

/// The number of times `T` appears in the meta::list.
template <class List, class T>
constexpr size_t count = detail::helper<List>::template count<T>;

/// `true` if `T` appears in the meta::list.
template <class List, class T>
constexpr bool has = detail::helper<List>::template has<T>;

/**
 * Returns a meta::list that is a concatenation of all the types in `Lists`.
 * @note The types of the containers of each list in `Lists` must be the same.
 * The resulting container is of that type.
 */
template <class... Lists>
using concat = typename detail::concat_helper<Lists...>::type;

/// The meta::list without duplicate types.
template <class List>
using dedupe = typename detail::helper<List>::dedupe;

/// `true` if the meta::list contains duplicate types
template <class List>
constexpr bool has_duplicates = !std::is_same_v<List, dedupe<List>>;

/**
 * Sorts the meta::list using the given `Compare` meta-function.
 *
 * `Compare<A, B>::value` should result in `true` if `A` is less than `B` (i.e.
 * `A` is ordered before `B`).
 */
template <class List, template <class, class> class Compare>
using sort = typename detail::helper<List>::template sort<Compare>;

/**
 * Flatten the meta::list.
 *
 * Only lists of the same type as the root list are flattened:
 * ~~~{.cpp}
 * template <class... Ts> struct MyList {};
 *
 * using my_list = MyList<
 *     MyList<int>,
 *     std::tuple<double, std::string>, // not flattened, since not `MyList`
 *     bool,
 *     MyList<float, MyList<int, int>>
 *     //            ^~~~~~~~~~~~~~~~
 *     //            flattens recursively
 * >;
 *
 * using flattened = meta::list::flatten<my_list>;
 * // results in MyList<int, std::tuple<double, std::string>, bool, int, int>
 * ~~~
 */
template <class List>
using flatten = typename detail::helper<List>::flatten;

/// Unwrap the meta::list if it contains only one element.
template <class List>
using unwrap = typename detail::helper<List>::unwrap;

/**
 * Apply the given `apply` function on each type in the meta::list.
 *
 * `apply` is called on each type `T` as type_constant<T>.
 */
template <class List, class Apply>
constexpr void each(Apply&& apply) { detail::helper<List>::each(apply); }

/**
 * Returns the first element `T` in `List` for which `Predicate<T>::value`
 * evaluates to `true`. If none-such element exists, returns meta::none.
 */
template <class List, template <class> class Predicate>
using find_if = typename detail::helper<List>::template find_if<Predicate>;

}

#include "list.inl"
