#pragma once

#include <sstream>
#include <tuple>
#include <variant>
#include <boost/filesystem/path.hpp>

#include <boost/hana/for_each.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/variant/variant.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/boost_variant.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

namespace cereal {

    template <class Archive, class ...Ts>
    void serialize(Archive& ar, std::variant<Ts...>& variant) {
        constexpr bool is_saving = Archive::is_saving::value;
        constexpr bool is_loading = Archive::is_loading::value;
        static_assert(is_saving != is_loading);

        if constexpr (is_saving) {
            auto bvar = std::visit(
                [](auto&& val){ return boost::variant<Ts...>(val); },
                variant
            );
            ar << bvar;
        } else {
            boost::variant<Ts...> bvar;
            ar >> bvar;
            variant = boost::apply_visitor(
                [](auto&& val){ return std::variant<Ts...>(val); },
                bvar
            );
        }
    }


} // namespace cereal


namespace util {
    namespace fs = boost::filesystem;
    template <class T>
    void serialize(T&& value, std::ostream& os) {
        cereal::BinaryOutputArchive(os) << std::forward<T>(value);
    }

    template <class T>
    void dump(T&& value, const fs::path& path) {
        std::ofstream ofstream(path);
        serialize(std::forward<T>(value), ofstream);
    }

    template <class T>
    std::string serialize(T&& value) {
        std::ostringstream os;
        serialize(std::forward<T>(value), os);
        return os.str();
    }

    template <class T>
    T deserialize(std::istream& is) {
        T rv;
        cereal::BinaryInputArchive archive(is);
        archive >> rv;
        return rv;
    }

    template <class T>
    T load(const fs::path& path) {
        std::ifstream ifstream(path);
        return deserialize<T>(ifstream);
    }

    template <class T>
    T deserialize(const std::string& s) {
        std::istringstream is(s);
        return deserialize<T>(is);
    }

} // namespace town::util
