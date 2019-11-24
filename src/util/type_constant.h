#pragma once

//@{
/// Stores the type `T` as a constexpr value.
template <class T> struct type_constant { using type = T; };
template <class T> constexpr type_constant<T> type_c{};
//@}

