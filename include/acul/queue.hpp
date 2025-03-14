#pragma once

#include <queue>
#include "deque.hpp"

namespace acul
{
    template <typename T>
    using queue = std::queue<T, deque<T>>;
}