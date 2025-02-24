#pragma once

#include <queue>
#include "deque.hpp"

namespace astl
{
    template <typename T>
    using queue = std::queue<T, deque<T>>;
}