#pragma once

#include "scalars.hpp"

namespace astl
{
    template <typename T = i32>
    struct point2D
    {
        T x;
        T y;
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
} // namespace astl
