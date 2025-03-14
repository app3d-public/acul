#pragma once

#include <oneapi/tbb/scalable_allocator.h>
#include <unordered_set>

namespace acul
{
    template <typename T>
    using hashset = std::unordered_set<T, std::hash<T>, std::equal_to<T>, oneapi::tbb::scalable_allocator<T>>;
}