#pragma once

#include <ranges>
#include "std/darray.hpp"

template <typename... R>
inline auto join_range(R &...ranges)
{
    return std::ranges::views::join(DArray{std::views::all(ranges)...});
}