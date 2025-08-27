#pragma once

#include <cassert>
#include <functional>
#include "../memory/alloc.hpp"
#include "internal/map_traits.hpp"
#include "internal/raw_hash_table.hpp"

namespace acul
{
    template <typename K, typename V, typename Allocator = mem_allocator<std::byte>, typename H = std::hash<K>,
              typename Eq = std::equal_to<K>>
    using hashmap = internal::raw_hashtable<Allocator, internal::map_traits<K, V, H, Eq>>;
} // namespace acul