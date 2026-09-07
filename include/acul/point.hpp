#pragma once

#include <type_traits>
#include "scalars.hpp"

namespace acul
{
    template <typename T>
    struct point
    {
        T x;
        T y;

        point() = default;

        point(T x, T y) : x(x), y(y) {}

        template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
        point(const point<U> &other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y))
        {
        }
    };

    template <typename T>
    inline bool operator==(const point<T> &a, const point<T> &b)
    {
        return a.x == b.x && a.y == b.y;
    }

    template <typename T>
    inline bool operator!=(const point<T> &a, const point<T> &b)
    {
        return !(a == b);
    }

    template <typename T>
    inline bool operator<(const point<T> &a, const point<T> &b)
    {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    }

    template <typename T>
    inline bool operator>(const point<T> &a, const point<T> &b)
    {
        return b < a;
    }

    template <typename T>
    inline point<T> &operator+(const point<T> &a, const point<T> &b)
    {
        return {a.x + b.x, a.y + b.y};
    }

    template <typename T>
    inline point<T> &operator+=(point<T> &a, const point<T> &b)
    {
        a.x += b.x;
        a.y += b.y;
        return a;
    }

    template <typename T>
    inline point<T> &operator-(const point<T> &a, const point<T> &b)
    {
        return {a.x - b.x, a.y - b.y};
    }

    template <typename T>
    inline point<T> &operator-=(point<T> &a, const point<T> &b)
    {
        a.x -= b.x;
        a.y -= b.y;
        return a;
    }
    template <typename T>
    inline point<T> &operator-(const point<T> &a)
    {
        return {-a.x, -a.y};
    }
    template <typename T>
    inline point<T> &operator*(const point<T> &a, i32 b)
    {
        return {a.x * b, a.y * b};
    }
    template <typename T>
    inline point<T> &operator/(const point<T> &a, i32 b)
    {
        return {a.x / b, a.y / b};
    }

    using ipoint = point<int>;
    using ipoint32 = point<i32>;
    using upoint32 = point<u32>;
    using upoint64 = point<u64>;
    using fpoint32 = point<f32>;
    using fpoint64 = point<f64>;
} // namespace acul