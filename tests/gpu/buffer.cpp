#include <acul/gpu/buffer.hpp>

using namespace acul;
using namespace acul::gpu;

void check_buffer_construct(device &d)
{
    buffer b;
    b.instance_count = 1;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eTransferSrc;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    b.priority = 0.1f;
    construct_buffer(b, sizeof(int));
    assert(acul::gpu::allocate_buffer(b, d));
    assert(acul::gpu::map_buffer(b, d));

    int value = 7;
    write_to_buffer(b, &value);
    flush_buffer(b, d);
    assert(invalidate_buffer(b, d) == vk::Result::eSuccess);
    int *mapped = (int *)b.mapped;
    assert(mapped);
    assert(*mapped == value);

    unmap_buffer(b, d);
    destroy_buffer(b, d);
}

void check_buffer_ubo(device &d)
{
    buffer b;
    b.instance_count = 3;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eTransferSrc;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    b.priority = 0.1f;

    construct_ubo_buffer(b, sizeof(u32), d.rd);
    assert(acul::gpu::allocate_buffer(b, d));
    assert(acul::gpu::map_buffer(b, d));

    u32 values[3] = {1, 2, 3};
    for (int i = 0; i < 3; ++i)
    {
        write_to_buffer_index(b, sizeof(u32), &values[i], i);
        flush_buffer_index(b, i, d);
        assert(invalidate_buffer_index(b, i, d) == vk::Result::eSuccess);
    }

    unmap_buffer(b, d);
    destroy_buffer(b, d);
}

void check_move_to_buffer(device &d)
{
    buffer b;
    b.instance_count = 1;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eTransferSrc;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    b.priority = 0.1f;

    construct_buffer(b, sizeof(int) * 3);
    assert(allocate_buffer(b, d));
    assert(map_buffer(b, d));

    int values[] = {10, 20, 30};
    move_to_buffer(b, values, sizeof(values));
    flush_buffer(b, d);

    int *mapped = static_cast<int *>(b.mapped);
    assert(mapped[0] == 10);
    assert(mapped[1] == 20);
    assert(mapped[2] == 30);

    int subvalue = 99;
    move_to_buffer(b, &subvalue, sizeof(subvalue), sizeof(int));
    flush_buffer_index(b, 1, d);
    assert(mapped[1] == 99);

    unmap_buffer(b, d);

    // Defered release
    buffer::mem_cache cache(b, d);
    cache.on_free();
    assert(b.vk_buffer == VK_NULL_HANDLE);
}

void test_gpu_buffer(device &d)
{
    check_buffer_construct(d);
    check_buffer_ubo(d);
    check_move_to_buffer(d);
}