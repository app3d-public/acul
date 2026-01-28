#pragma once

#include <functional>
#include "../internal/hash/map_traits.hpp"
#include "../internal/hash/raw_hl_hashtable.hpp"
#include "../memory/alloc.hpp"

namespace acul
{
    template <typename K, typename V, typename H = std::hash<K>, typename Eq = std::equal_to<K>>
    using hl_hashmap = internal::raw_hl_hashtable<mem_allocator<std::byte>, internal::map_traits<K, V, H, Eq>>;
} // namespace acul