#pragma once

#include <limits>

namespace acul
{
    template <typename T>
    struct range
    {
        T begin;
        T end;
    };

    template <typename T>
    struct minmax
    {
        T min = std::numeric_limits<T>::max();
        T max = std::numeric_limits<T>::min();
    };
} // namespace acul