#include <acul/hash/hashmap.hpp>
#include "hashmap_common.hpp"

namespace
{
    struct CountingAllocator : acul::mem_allocator<std::byte>
    {
        static inline size_t live_blocks = 0;
        static pointer allocate(size_t size)
        {
            ++live_blocks;
            return acul::mem_allocator<std::byte>::allocate(size);
        }
        static void deallocate(pointer ptr)
        {
            if (ptr) --live_blocks;
            acul::mem_allocator<std::byte>::deallocate(ptr);
        }
    };

    void test_copy_assignment_releases_storage()
    {
        using map = acul::hashmap<int, int, CountingAllocator>;
        {
            map source, destination;
            source[7] = 42;
            destination[9] = 1;
            const auto live = CountingAllocator::live_blocks;
            for (int i = 0; i < 100; ++i)
            {
                destination = source;
                assert(destination.size() == 1 && destination[7] == 42);
                assert(CountingAllocator::live_blocks == live);
            }
            source = std::move(destination);
            source = destination; // Copy from a moved-from, empty table.
            assert(source.empty());
        }
        assert(CountingAllocator::live_blocks == 0);
    }
}

void test_hashmap()
{
    test_copy_assignment_releases_storage();
    using container_t = acul::hashmap<int, int>;
    test_hashmap_basic<container_t>();
    test_hashmap_many_inserts_and_reads<container_t>();
    test_hashmap_iteration<container_t>();
    test_hashmap_update_path<container_t>();
    test_hashmap_erase<container_t>();
}
