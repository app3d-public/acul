#pragma once

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
} // namespace acul
