#pragma once

#ifndef NDEBUG
#define BENCH_DEBUG
#endif

/**
 * A macro for code that's discarded if NDEBUG is defined, and is otherwise
 * evaluated.
 */
#ifdef BENCH_DEBUG_DEBUG
#define DEBUG_CODE(code...) code
#else
#define DEBUG_CODE(code...)
#endif
