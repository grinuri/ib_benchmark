#pragma once

#include <type_traits>

#include "apply.h"

namespace meta::list::detail {

template <template <class...> class List, bool CONDITION, class T, class... Ts>
struct conditional_push_back_helper;

template <template <class...> class List, class T, class... Ts>
struct conditional_push_back_helper<List, true, T, Ts...> {
    using type = List<Ts..., T>;
};

template <template <class...> class List, class T, class... Ts>
struct conditional_push_back_helper<List, false, T, Ts...> {
    using type = List<Ts...>;
};

template <class... Lists>
struct concat_helper {
    static_assert(
        sizeof...(Lists) > 0,
        "meta::list::concat must receive at least one list"
    );
};

template <class List>
struct concat_helper<List> { using type = List; };

template <class List1, class List2>
struct concat_helper<List1, List2> {
    static_assert(
        sizeof(List1) == 0,
        "meta::list::concat must receive two meta-lists of the same type"
    );
};

template <template <class...> class List, class... Ts, class... Us>
struct concat_helper<List<Ts...>, List<Us...>> {
    using type = List<Ts..., Us...>;
};

template <class List1, class List2, class... Lists>
struct concat_helper<List1, List2, Lists...> {
    using type = typename concat_helper<
        typename concat_helper<List1, List2>::type,
        Lists...
    >::type;
};

template <bool RETURN_NONE, size_t I, class... Ts>
struct get_helper {
    // will eventually reach this instantiation of get_helper only when I is
    // out of range

    static_assert(
        RETURN_NONE || (I == I + 1),
        "meta::list::get index out of range"
    );

    using type = none;
};

template <bool RETURN_NONE, class T, class... Ts>
struct get_helper<RETURN_NONE, 0, T, Ts...> { using type = T; };

template <bool RETURN_NONE, size_t I, class T, class... Ts>
struct get_helper<RETURN_NONE, I, T, Ts...> :
    get_helper<RETURN_NONE, I - 1, Ts...> {};

template <class T, size_t I, class... Ts>
struct index_helper {
    static_assert(
        sizeof(T) == 0,
        "Type `T` must exist in the list of types `Ts...`"
    );
};

template <class T, size_t I, class... Ts>
struct index_helper<T, I, T, Ts...> : std::integral_constant<size_t, I> {};

template <class T, size_t I, class NotT, class... Ts>
struct index_helper<T, I, NotT, Ts...> : index_helper<T, I + 1, Ts...> {};

template <class Result, class... Ts>
struct dedupe_helper;

template <class Result>
struct dedupe_helper<Result> { using type = Result; };

template <template <class...> class List, class... Rs, class T, class... Ts>
struct dedupe_helper<List<Rs...>, T, Ts...> : dedupe_helper<
    std::conditional_t<has<List<Rs...>, T>, List<Rs...>, List<Rs..., T>>, Ts...
> {};

/// Returned by detail::partition as either the low or high partition.
template <class...> struct partitioned {};

/// Returned by detail::partition as a pair of low and high partitions.
template <class Low, class High>
struct partitions {
    using low  = Low;
    using high = High;
};

template <
    class Low,
    class High,
    class Pivot,
    template <class, class> class Compare,
    class... Ts
>
struct partition_helper;

template <
    class... Lows,
    class... Highs,
    class Pivot,
    template <class, class> class Compare
>
struct partition_helper<
    partitioned<Lows...>,
    partitioned<Highs...>,
    Pivot,
    Compare
> {
    using type = partitions<partitioned<Lows...>, partitioned<Highs...>>;
};

template <
    class... Lows,
    class... Highs,
    class Pivot,
    template <class, class> class Compare,
    class T,
    class... Ts
>
struct partition_helper<
    partitioned<Lows...>,
    partitioned<Highs...>,
    Pivot,
    Compare,
    T,
    Ts...
> {
    static constexpr bool right = Compare<Pivot, T>::value;

    using low  = conditional_push_back<partitioned<Lows...>, !right, T>;
    using high = conditional_push_back<partitioned<Highs...>, right, T>;

    using type =
        typename partition_helper<low, high, Pivot, Compare, Ts...>::type;
};

template <class Pivot, template <class, class> class Compare, class... Ts>
using partition = typename partition_helper<
    partitioned<>, // low partition list
    partitioned<>, // high partition list
    Pivot,
    Compare,
    Ts...
>::type;

template <
    template <class...> class List,
    template <class, class> class Compare,
    class... Ts
>
struct sort_helper;

template <template <class...> class List, template <class, class> class Compare>
struct sort_helper<List, Compare> { using type = List<>; };

template <
    template <class...> class List,
    template <class, class> class Compare,
    class T,
    class... Ts
>
struct sort_helper<List, Compare, T, Ts...> {
    // quick-sort
    using partitions  = partition<T, Compare, Ts...>;
    using sorted_low  = sort<typename partitions::low,  Compare>;
    using sorted_high = sort<typename partitions::high, Compare>;
    using flattened = flatten<partitioned<sorted_low, T, sorted_high>>;
    using type = unpack<flattened, List>;
};

template <class Result, class... Ts>
struct flatten_helper;

template <class Result>
struct flatten_helper<Result> { using type = Result; };

template <template <class...> class List, class... Rs, class T, class... Ts>
struct flatten_helper<List<Rs...>, T, Ts...> :
    flatten_helper<List<Rs..., T>, Ts...> {};

template <template <class...> class List, class... Rs, class... Us, class... Ts>
struct flatten_helper<List<Rs...>, List<Us...>, Ts...> :
    flatten_helper<flatten<List<Rs..., Us...>>, Ts...> {};

template <template <class> class Predicate, class... Ts>
struct find_if_helper { using type = meta::none; };

template <template <class> class Predicate, class T, class... Ts>
struct find_if_helper<Predicate, T, Ts...> {
    using type = std::conditional_t<
        Predicate<T>::value,
        T,
        typename find_if_helper<Predicate, Ts...>::type
    >;
};

template <class T>
struct helper {
    static_assert(
        sizeof(T) == 0,
        "The given type is not a valid meta::list. Meta::list must be template "
        "classes of the form `Class<Ts...>`"
    );
};

template <template <class...> class List, class... Ts>
struct helper<List<Ts...>> {
    static constexpr size_t size = sizeof...(Ts);

    template <class T> using push_back  = List<Ts..., T>;
    template <class T> using push_front = List<T, Ts...>;

    template <bool CONDITION, class T>
    using conditional_push_back =
        typename conditional_push_back_helper<List, CONDITION, T, Ts...>::type;

    template <size_t I>
    using get = typename get_helper<false, I, Ts...>::type;

    template <size_t I>
    using get_or_none = typename get_helper<true, I, Ts...>::type;

    template <class T>
    static constexpr size_t index = index_helper<T, 0, Ts...>::value;

    template <template <class> class MF>
    using transform = List<MF<Ts>...>;

    template <template <class...> class Template>
    using unpack = Template<Ts...>;

    template <class T>
    static constexpr size_t count =
        (size_t(0) + ... + size_t(std::is_same_v<T, Ts>));

    template <class T>
    static constexpr bool has = (... || std::is_same_v<T, Ts>);

    template <class OtherList>
    using concat = typename concat_helper<List<Ts...>, OtherList>::type;

    using dedupe = typename dedupe_helper<List<>, Ts...>::type;

    template <template <class, class> class Compare>
    using sort = typename sort_helper<List, Compare, Ts...>::type;

    using flatten = typename flatten_helper<List<>, Ts...>::type;

    using unwrap = std::conditional_t<size == 1, get_or_none<0>, List<Ts...>>;

    template <class Apply>
    static constexpr void each(Apply&& apply) {
        meta::apply(apply, type_c<Ts>...);
    }

    template <template <class> class Predicate>
    using find_if = typename find_if_helper<Predicate, Ts...>::type;
};

}
