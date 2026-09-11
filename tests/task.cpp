#include <acul/task.hpp>
#include <cassert>
#include <future>

namespace
{
    bool wait_count(const std::atomic<int> &count, int expected)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (count.load() < expected && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        return count.load() >= expected;
    }

    struct CountingService : acul::task::service_base
    {
        std::atomic<int> &count;
        bool called = false;
        explicit CountingService(std::atomic<int> &count) : count(count) {}
        std::chrono::steady_clock::time_point dispatch() override
        {
            if (!called) { called = true; ++count; }
            return std::chrono::steady_clock::time_point::max();
        }
        void await(bool = false) override {}
    };

    struct BlockingService : CountingService
    {
        std::promise<void> &entered;
        std::shared_future<void> resume;
        BlockingService(std::atomic<int> &count, std::promise<void> &entered, std::shared_future<void> resume)
            : CountingService(count), entered(entered), resume(resume) {}
        std::chrono::steady_clock::time_point dispatch() override
        {
            if (!called) { entered.set_value(); resume.wait(); }
            return CountingService::dispatch();
        }
    };

    struct NotifyingService : CountingService
    {
        using CountingService::CountingService;
        std::chrono::steady_clock::time_point dispatch() override
        {
            if (++count == 1) notify();
            return std::chrono::steady_clock::time_point::max();
        }
    };
}

void test_service_registration_during_dispatch()
{
    std::atomic<int> count{0};
    std::promise<void> entered, resume;
    acul::task::service_dispatch sd;
    sd.register_service(acul::alloc<BlockingService>(count, entered, resume.get_future().share()));
    sd.register_service(acul::alloc<CountingService>(count));
    sd.run();
    entered.get_future().wait();
    // Force registry reallocations while the worker is inside its first service.
    for (int i = 0; i < 128; ++i) sd.register_service(acul::alloc<CountingService>(count));
    resume.set_value();
    assert(wait_count(count, 130));
}

void test_service_notification_during_dispatch()
{
    std::atomic<int> count{0};
    acul::task::service_dispatch sd;
    sd.register_service(acul::alloc<NotifyingService>(count));
    sd.run();
    assert(wait_count(count, 2));
}

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
    service_dispatch  sd;
    sd.run();
    shedule_service *scheduler = acul::alloc<shedule_service>();
    sd.register_service(scheduler);

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
    service_dispatch sd;
    sd.run();
    shedule_service *scheduler = acul::alloc<shedule_service>();
    sd.register_service(scheduler);

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
    test_service_registration_during_dispatch();
    test_service_notification_during_dispatch();
    test_task_simple();
    test_task_void();
    test_thread_dispatch_simple();
    test_thread_dispatch_void();
    test_shedule_service();
    test_shedule_service_order();
}
