#pragma once

#include <acul/compile_attributes.h>
#include <acul/symbol_export.h>
#include <stdio.h>
#include <stdlib.h>

namespace acul::detail
{
    [[noreturn]] ACUL_COLD ACUL_EXPORT void throw_requirement_failure(const char *condition, const char *file,
                                                                      int line);

    [[noreturn]] ACUL_COLD inline void report_assertion_failure(const char *message, const char *condition,
                                                                const char *file, int line)
    {
        fprintf(stderr, "%s:%d: %s: %s\n", file, line, message, condition);
        fflush(stderr);
        abort();
    }
} // namespace acul::detail

#ifdef NDEBUG
    #define ACUL_DETAIL_ASSERT_FAILURE(condition) ((void)0)
    #define ACUL_DETAIL_REQUIRE_FAILURE(condition) \
        ::acul::detail::throw_requirement_failure(condition, __FILE__, __LINE__)
#else
    #define ACUL_DETAIL_ASSERT_FAILURE(condition) \
        ::acul::detail::report_assertion_failure("Assertion failed", condition, __FILE__, __LINE__)
    #define ACUL_DETAIL_REQUIRE_FAILURE(condition) \
        ::acul::detail::report_assertion_failure("Required condition failed", condition, __FILE__, __LINE__)
#endif

// Assert in Debug and return the fallback in Release. Omit the fallback for void.
#define ACUL_ASSERT(condition, ...)                 \
    do                                              \
    {                                               \
        if (ACUL_UNLIKELY(!(condition)))            \
        {                                           \
            ACUL_DETAIL_ASSERT_FAILURE(#condition); \
            return __VA_ARGS__;                     \
        }                                           \
    } while (0)

// Assert in Debug and throw acul::runtime_error in Release.
#define ACUL_REQUIRE(condition)                                                   \
    do                                                                            \
    {                                                                             \
        if (ACUL_UNLIKELY(!(condition))) ACUL_DETAIL_REQUIRE_FAILURE(#condition); \
    } while (0)
