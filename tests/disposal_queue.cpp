#include <acul/disposal_queue.hpp>
#include <cassert>

void test_disposal_queue()
{
    using namespace acul;
    disposal_queue queue;

    // Single
    bool b0 = false;
    queue.push(alloc<mem_cache>([&]() { b0 = true; }));

    bool b1 = false;
    disposal_queue::mem_data data;
    data.cache_list.push_back(alloc<mem_cache>([&]() { b1 = true; }));
    data.on_wait = [&]() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
    queue.push(data);

    // Shared
    shared_ptr<int> i0 = make_shared<int>(42);
    queue.push(alloc<shared_mem_cache<int>>(i0));

    assert(!queue.empty());
    queue.flush();

    assert(b0);
    assert(b1);
}