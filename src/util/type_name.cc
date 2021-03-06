#include <memory>
#include <cxxabi.h>

#include "type_name.h"

namespace ib_bench {

std::string demangle(const char* name) {
    int status;
    std::unique_ptr<char, decltype(&std::free)> result(
        abi::__cxa_demangle(name, nullptr, nullptr, &status),
        std::free
    );
    switch (status) {
        case 0:
            return result.get();
        case -1:
            throw std::bad_alloc();
        case -2:
            throw std::invalid_argument(
                '"' + std::string(name) + "\" is not a valid mangled name"
            );
        default:
            throw std::runtime_error("Should not reach here");
    }
}

}
