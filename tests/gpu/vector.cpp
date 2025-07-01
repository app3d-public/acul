#include <acul/gpu/vector.hpp>

using namespace acul::gpu;
using namespace acul;

void test_gpu_vector_basic(device &d)
{
    gpu::vector<int> v;
    assert(v.empty());
    assert(v.size() == 0);

    buffer b0;
    b0.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b0.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b0.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    v.init(d, b0);

    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    assert(!v.empty());
    assert(v.size() == 3);
    assert(v[0] == 1);
    assert(v[1] == 2);
    assert(v[2] == 3);

    v.clear();
    assert(v.empty());
    assert(v.size() == 0);

    buffer b1;
    b1.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b1.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b1.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    b1.instance_count = 10;
    gpu::vector<u32> v2(d, b1, 7);
    assert(v2.size() == 10);
    assert(v2[0] == 7);
}

void test_gpu_vector_resize_reserve(device &d)
{
    gpu::vector<int> v;
    buffer b0;
    b0.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b0.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b0.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    v.init(d, b0);

    v.reserve(10);
    assert(v.capacity() >= 10);
    assert(v.size() == 0);

    v.resize(5);
    assert(v.size() == 5);

    v[2] = 42;
    assert(v[2] == 42);

    v.resize(2);
    assert(v.size() == 2);
}

void test_gpu_vector_move(device &d)
{
    buffer b;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    gpu::vector<int> v1{d, b};
    v1.push_back(100);
    v1.push_back(200);

    // Move
    gpu::vector<int> v4 = std::move(v1);
    assert(v4.size() == 2);
    assert(v4[0] == 100);
    assert(v4[1] == 200);
    assert(v1.empty() || v1.size() == 0);
}

void test_gpu_vector_front_back(device &d)
{
    buffer b;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    gpu::vector<int> v{d, b};
    v.push_back(5);
    v.push_back(10);
    v.push_back(15);

    assert(v.front() == 5);
    assert(v.back() == 15);

    v.front() = 1;
    v.back() = 99;

    assert(v.front() == 1);
    assert(v.back() == 99);
}

void test_gpu_vector_insert_erase(device &d)
{
    buffer b;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    gpu::vector<int> v{{1, 2, 4}, d, b};

    v.insert(v.begin() + 2, 3);
    assert(v.size() == 4);
    assert(v[2] == 3);
    assert(v[3] == 4);

    v.erase(v.begin() + 1);
    assert(v.size() == 3);
    assert(v[0] == 1);
    assert(v[1] == 3);
    assert(v[2] == 4);
}

void test_gpu_vector_assign(device &d)
{
    buffer b;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    gpu::vector<int> v{d, b};
    v.assign(5, 42);
    assert(v.size() == 5);
    for (auto &e : v) assert(e == 42);
}

void test_gpu_vector_iterators(device &d)
{
    buffer b;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eStorageBuffer;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    b.instance_count = 5;
    gpu::vector<int> v(d, b);
    assert(v.data().mapped);
    std::iota(v.begin(), v.end(), 10);

    assert(std::find(v.begin(), v.end(), 12) != v.end());
    assert(std::find(v.begin(), v.end(), 999) == v.end());

    auto it = std::find_if(v.begin(), v.end(), [](int x) { return x > 12; });
    assert(it != v.end());
    assert(*it == 13);
}

void test_gpu_vector(device &d)
{
    test_gpu_vector_basic(d);
    test_gpu_vector_resize_reserve(d);
    test_gpu_vector_move(d);
    test_gpu_vector_front_back(d);
    test_gpu_vector_insert_erase(d);
    test_gpu_vector_assign(d);
    test_gpu_vector_iterators(d);
}