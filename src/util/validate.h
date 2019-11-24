#pragma once

#include <sstream>
#include "code.h"

struct error {};

/// Validate an expression at runtime and throw an exception if false.
#define VALIDATE(expr, msg...) \
    if(!(expr)) { \
        std::stringstream error_msg; \
        error_msg << "Failed on assertion: (" << #expr << ")" << std::endl; \
        error_msg << "At " << __FILE__ << ":" << __LINE__ << std::endl; \
        error_msg << "Error: " << msg << std::endl; \
        throw error(); \
    }

/**
 * Validate an expression at runtime and throw an exception if false.
 * Does not evaluate if NDEBUG is defined.
 */
#define DEBUG_VALIDATE(expr, msg)\
    DEBUG_CODE(VALIDATE(expr, msg))
