#pragma once

#include <map>
#include <oneapi/tbb/scalable_allocator.h>

namespace astl
{
    template <typename K, typename V, typename C = std::less<K>>
    using map = std::map<K, V, C, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;

    template <typename K, typename V, typename C = std::less<K>>
    using multimap = std::multimap<K, V, C, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;
} // namespace astl