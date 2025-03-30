#pragma once

#include <functional>
#include <oneapi/tbb/scalable_allocator.h>

namespace acul
{
    template <typename K, typename V, typename H = std::hash<K>>
    using hashmap =
        std::unordered_map<K, V, H, std::equal_to<K>, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;

    template <typename K, typename V, typename H = std::hash<K>>
    using multi_hashmap =
        std::unordered_multimap<K, V, H, std::equal_to<K>, oneapi::tbb::scalable_allocator<std::pair<const K, V>>>;
} // namespace acul