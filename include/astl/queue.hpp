#pragma once

#include <oneapi/tbb/scalable_allocator.h>
#include <queue>
#include "deque.hpp"

namespace astl
{
    template <typename T>
    using queue = std::queue<T, deque<T>>;
}