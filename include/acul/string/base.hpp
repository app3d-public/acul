#pragma once

#include <cstddef>
#include <utility>

template <typename T>
constexpr static inline size_t null_terminated_length(const T *s) noexcept
{
    const T *p = s;
    while (*p) ++p;
    return p - s;
}

template <typename T, typename U, typename = void>
struct is_string_like : std::false_type
{
};

template <typename T, typename U>
struct is_string_like<
    T, U, std::void_t<decltype(std::declval<const U &>().data()), decltype(std::declval<const U &>().size())>>
    : std::bool_constant<std::is_convertible_v<decltype(std::declval<const U &>().data()), const T *> &&
                         std::is_convertible_v<decltype(std::declval<const U &>().size()), size_t>>
{
};

template <typename T, typename U>
constexpr bool is_string_like_v = is_string_like<T, U>::value;

template <typename T>
constexpr int compare_string(T *lhs, size_t lhs_size, T *rhs, size_t rhs_size) noexcept
{
    size_t min_size = lhs_size < rhs_size ? lhs_size : rhs_size;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_memcmp(lhs, rhs, min_size);
#else
    return memcmp(lhs, rhs, min_size);
#endif
}