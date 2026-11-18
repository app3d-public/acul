#pragma once

#include <queue>
#include <oneapi/tbb/concurrent_priority_queue.h>
#include "deque.hpp"

namespace acul
{
    template <typename T>
    using queue = std::queue<T, deque<T>>;

    template <typename T, typename Compare = std::less<T>>
    using priority_queue = oneapi::tbb::concurrent_priority_queue<T, Compare>;
}