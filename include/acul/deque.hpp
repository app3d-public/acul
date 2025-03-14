#pragma once

#include <deque>
#include <oneapi/tbb/scalable_allocator.h>

namespace acul
{
    template <typename T>
    using deque = std::deque<T, oneapi::tbb::scalable_allocator<T>>;
}