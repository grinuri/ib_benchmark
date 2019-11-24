#pragma once

#include <type_traits>
#include <string>

namespace ib_bench {

/// @return A demangled C++ type name from the given mangled name.
std::string demangle(const char* name);

/// @return A demangled type name for type `T`.
template <class T>
std::string type_name() { return demangle(typeid(T).name()); }

/// @return A demangled type name for value `v`.
template <class T>
std::string type_name(const T& v) { return type_name<decltype(v)>(); }

/**
 * Returns a demangled type name for value `v` with const, volatile, and
 * reference qualifiers.
 */
template <class T>
std::string type_name_cvref() {
    std::string name = type_name<T>();
    using without_ref = std::remove_reference_t<T>;
    if (std::is_const_v<without_ref>)    { name += " const";    }
    if (std::is_volatile_v<without_ref>) { name += " volatile"; }
    if (std::is_lvalue_reference_v<T>)   { name += "&";         }
    if (std::is_rvalue_reference_v<T>)   { name += "&&";        }
    return name;
}

}
