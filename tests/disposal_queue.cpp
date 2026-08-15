#include <acul/disposal_queue.hpp>
#include <cassert>

void test_disposal_queue()
{
    using namespace acul;
    disposal_queue queue;

    // Single
    bool b0 = false;
    queue.emplace([&]() { b0 = true; });

    bool b1 = false;
    disposal_queue::mem_data data;
    data.cache_list.push_back(alloc<mem_cache>([&]() { b1 = true; }));
    data.on_wait = [&]() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
    queue.push(std::move(data));

    // Shared
    shared_ptr<int> i0 = make_shared<int>(42);
    queue.push(make_unique<shared_mem_cache<int>>(i0));

    bool b2 = false;
    queue.emplace([&]() { b2 = true; });

    assert(!queue.is_main_queue_empty());
    queue.flush();

    assert(b0);
    assert(b1);
    assert(b2);

    // Regular commands remain reentrant, while forced commands are executed
    // only after the callback that enqueued them has returned.
    acul::vector<int> order;
    queue.emplace([&]() {
        order.push_back(1);
        queue.emplace([&]() { order.push_back(2); });
        queue.emplace(
            [&]() {
                order.push_back(4);
                queue.emplace([&]() { order.push_back(6); }, true);
            },
            true);
        order.push_back(3);
    });
    queue.emplace([&]() { order.push_back(5); });
    queue.flush();

    assert(order.size() == 6);
    assert(order[0] == 1);
    assert(order[1] == 2);
    assert(order[2] == 3);
    assert(order[3] == 5);
    assert(order[4] == 4);
    assert(order[5] == 6);
}
