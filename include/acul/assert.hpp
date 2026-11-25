#pragma once

#include <acul/compile_attributes.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

namespace acul::detail
{
    [[noreturn]] ACUL_COLD inline void require_failed(const char *condition, const char *file, int line)
    {
        fprintf(stderr, "%s:%d: Required condition failed: %s\n", file, line, condition);
        abort();
    }
}

// Assert in Debug and return the fallback in Release. Omit the fallback for void.
#define ACUL_ASSERT(condition, ...)                                                               \
    do                                                                                            \
    {                                                                                             \
        if (ACUL_UNLIKELY(!(condition)))                                                           \
        {                                                                                         \
            assert(0 && #condition);                                                              \
            return __VA_ARGS__;                                                                   \
        }                                                                                         \
    } while (0)

// Use when continuing after a runtime failure would leave the program in an unsafe state.
#define ACUL_REQUIRE(condition)                                                                   \
    do                                                                                            \
    {                                                                                             \
        if (ACUL_UNLIKELY(!(condition)))                                                           \
        {                                                                                         \
            assert(0 && #condition);                                                              \
            ::acul::detail::require_failed(#condition, __FILE__, __LINE__);                         \
        }                                                                                         \
    } while (0)
