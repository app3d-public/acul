#pragma once

#include "detail/raw_hash_table.hpp"
#include "detail/set_traits.hpp"

namespace acul
{
    template <typename K, typename Allocator = mem_allocator<std::byte>, typename H = std::hash<K>,
              typename Eq = std::equal_to<K>>
    using hashset = detail::raw_hashtable<Allocator, detail::set_traits<K, H, Eq>>;
}