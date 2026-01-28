#pragma once

#include "../internal/hash/raw_hash_table.hpp"
#include "../internal/hash/set_traits.hpp"

namespace acul
{
    template <typename K, typename Allocator = mem_allocator<std::byte>, typename H = std::hash<K>,
              typename Eq = std::equal_to<K>>
    using hashset = internal::raw_hashtable<Allocator, internal::set_traits<K, H, Eq>>;
}