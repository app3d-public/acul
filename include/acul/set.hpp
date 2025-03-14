#pragma once

#include <oneapi/tbb/scalable_allocator.h>
#include <set>

namespace acul
{
    template <typename T>
    using set = std::set<T, std::less<T>, oneapi::tbb::scalable_allocator<T>>;
}