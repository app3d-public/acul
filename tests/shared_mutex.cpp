#include <acul/shared_mutex.hpp>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

void test_shared_locking()
{
    acul::shared_mutex mutex;
    std::atomic<int> shared_counter{0};

    auto shared_task = [&]() {
        mutex.lock_shared();
        ++shared_counter;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        mutex.unlock_shared();
    };

    std::thread t1(shared_task);
    std::thread t2(shared_task);

    t1.join();
    t2.join();

    assert(shared_counter == 2);
    std::cout << "Shared lock test passed.\n";
}

void test_exclusive_blocks_shared()
{
    acul::shared_mutex m;
    std::atomic<bool> shared_entered{false};
    std::atomic<bool> exclusive_locked{false};

    std::condition_variable cv;
    std::mutex cv_mutex;

    auto exclusive = [&]() {
        acul::exclusive_lock lock(m);
        {
            std::lock_guard<std::mutex> lock(cv_mutex);
            exclusive_locked = true;
        }
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    };

    auto shared = [&]() {
        acul::shared_lock lock(m);
        shared_entered = true;
    };

    std::thread t1(exclusive);

    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        cv.wait(lock, [&] { return exclusive_locked.load(); });
    }

    std::thread t2(shared);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!shared_entered);

    t1.join();
    t2.join();

    std::cout << "Exclusive blocks shared test passed.\n";
}

void test_manual_lock()
{
    acul::shared_mutex m;
    std::atomic<bool> shared_entered1{false};
    std::atomic<bool> shared_entered2{false};
    std::atomic<bool> shared_ready{false};

    std::condition_variable cv;
    std::mutex cv_m;

    std::thread t1([&]() {
        acul::exclusive_lock lock(m);
        lock.unlock();

        {
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [&] { return shared_ready.load(); });
        }

        lock.lock();
    });

    std::thread t2([&]() {
        acul::shared_lock lock(m);
        lock.unlock();

        shared_ready = true;
        cv.notify_one();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lock.lock();
        shared_entered1 = true;
        lock.unlock();
    });

    std::thread t3([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        acul::shared_lock lock(m);
        shared_entered2 = true;
        lock.unlock();
    });

    t1.join();
    t2.join();
    t3.join();

    assert(shared_entered1);
    assert(shared_entered2);

    std::cout << "Manual exclusive and shared lock/unlock test passed.\n";
}

void test_shared_mutex()
{
    test_shared_locking();
    test_exclusive_blocks_shared();
    test_manual_lock();
    std::cout << "All shared_mutex tests passed.\n";
}
