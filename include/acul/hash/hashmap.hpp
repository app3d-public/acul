#pragma once

#include "detail/map_traits.hpp"
#include "detail/raw_hash_table.hpp"

namespace acul
{
    template <typename K, typename V, typename Allocator = mem_allocator<std::byte>, typename H = std::hash<K>,
              typename Eq = std::equal_to<K>>
    using hashmap = detail::raw_hashtable<Allocator, detail::map_traits<K, V, H, Eq>>;
} // namespace acul