#pragma once

#include <type_traits>
#include "scalars.hpp"

namespace acul
{
    template <typename F, typename S>
    struct pair
    {
        F first;
        S second;

        bool operator==(const pair<F, S> &other) const { return first == other.first && second == other.second; }

        bool operator!=(const pair<F, S> &other) const { return !(*this == other); }
    };

    template <typename T = i32>
    struct point2D
    {
        T x;
        T y;

        point2D() = default;

        point2D(T x, T y) : x(x), y(y) {}

        template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
        point2D(const point2D<U> &other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y))
        {
        }
    };

    template <typename T>
    inline bool operator==(const point2D<T> &a, const point2D<T> &b)
    {
        return a.x == b.x && a.y == b.y;
    }

    template <typename T>
    inline bool operator!=(const point2D<T> &a, const point2D<T> &b)
    {
        return !(a == b);
    }

    template <typename T>
    inline bool operator<(const point2D<T> &a, const point2D<T> &b)
    {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    }

    template <typename T>
    inline bool operator>(const point2D<T> &a, const point2D<T> &b)
    {
        return b < a;
    }

    template <typename T>
    inline point2D<T> operator+(const point2D<T> &a, const point2D<T> &b)
    {
        return {a.x + b.x, a.y + b.y};
    }

    template <typename T>
    inline point2D<T> &operator+=(point2D<T> &a, const point2D<T> &b)
    {
        a.x += b.x;
        a.y += b.y;
        return a;
    }

    template <typename T>
    inline point2D<T> operator-(const point2D<T> &a, const point2D<T> &b)
    {
        return {a.x - b.x, a.y - b.y};
    }

    template <typename T>
    inline point2D<T> &operator-=(point2D<T> &a, const point2D<T> &b)
    {
        a.x -= b.x;
        a.y -= b.y;
        return a;
    }
    template <typename T>
    inline point2D<T> operator-(const point2D<T> &a)
    {
        return {-a.x, -a.y};
    }
    template <typename T>
    inline point2D<T> operator*(const point2D<T> &a, i32 b)
    {
        return {a.x * b, a.y * b};
    }
    template <typename T>
    inline point2D<T> operator/(const point2D<T> &a, i32 b)
    {
        return {a.x / b, a.y / b};
    }

    template <typename T>
    struct range
    {
        T begin;
        T end;
    };
} // namespace acul
