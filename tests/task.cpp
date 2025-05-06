#include <acul/task.hpp>
#include <cassert>

void test_task_simple()
{
    using namespace acul::task;

    auto t = acul::make_shared<task<int>>([] { return 42; });
    t->run();
    t->await();
    assert(t->get() == 42);
}

void test_task_void()
{
    using namespace acul::task;

    bool flag = false;
    auto t = acul::make_shared<task<void>>([&flag] { flag = true; });
    t->run();
    t->await();
    assert(flag);
}

template <typename U, typename... Args>
static inline void construct(U *p, Args &&...args)
{
    ::new ((void *)p) U(std::forward<Args>(args)...);
}

void test_thread_dispatch_simple()
{

    using namespace acul::task;

    thread_dispatch dispatcher;

    auto t1 = dispatcher.dispatch([] { return 123; });
    auto t2 = dispatcher.dispatch([] { return 456; });
    assert(t1->get() == 123);
    assert(t2->get() == 456);
    dispatcher.await(true);
}

void test_thread_dispatch_void()
{
    using namespace acul::task;

    thread_dispatch dispatcher;

    bool done = false;
    auto t = dispatcher.dispatch([&done] { done = true; });
    dispatcher.await(false);

    assert(done);
}

void test_shedule_service()
{
    using namespace acul::task;
    service_dispatch dispatcher;
    shedule_service *scheduler = acul::alloc<shedule_service>();
    dispatcher.register_service(scheduler);

    bool first = false;
    bool second = false;

    auto now = std::chrono::steady_clock::now();
    scheduler->add_task([&] { first = true; }, now);
    scheduler->add_task([&] { second = true; }, now + std::chrono::milliseconds(10));

    scheduler->await(false);

    assert(first);
    assert(second);
}

void test_shedule_service_order()
{
    using namespace acul::task;
    service_dispatch dispatcher;
    shedule_service *scheduler = acul::alloc<shedule_service>();
    dispatcher.register_service(scheduler);

    acul::vector<int> result;

    auto now = std::chrono::steady_clock::now();

    scheduler->add_task([&] { result.push_back(1); }, now + std::chrono::milliseconds(10));
    scheduler->add_task([&] { result.push_back(2); }, now);

    scheduler->await(false);

    assert(result.size() == 2);
    assert(result[0] == 2);
    assert(result[1] == 1);
}

void test_task()
{
    test_task_simple();
    test_task_void();
    test_thread_dispatch_simple();
    test_thread_dispatch_void();
    test_shedule_service();
    test_shedule_service_order();
}