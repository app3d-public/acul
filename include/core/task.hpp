#ifndef APP_CORE_TASK_H
#define APP_CORE_TASK_H

#include <condition_variable>
#include <functional>
#include <future>
#include <oneapi/tbb/concurrent_queue.h>
#include <thread>
#include "api.hpp"
#include "std/darray.hpp"

// Interface for a generic task. It provides a structure for task execution and fetching results.
class ITask
{
public:
    virtual void onRun() = 0;  // Method to define the task's execution logic.
    virtual void onFetch() {}; // Optional method to define post-execution logic (e.g., processing results).
    virtual ~ITask() = default;
};

// Class for a specific task type, inheriting from ITask
template <typename T>
class Task : public ITask
{
public:
    explicit Task(std::function<T()> handler) : _onRun(handler) {}

    // Method to set a callback function for post-execution logic and return a shared pointer to the task.
    Task<T> *onFetch(std::function<void(T)> fetch)
    {
        _onFetch = fetch;
        return this;
    }

    // Method to execute the task's logic and set the promise value.
    virtual void onRun() override
    {
        _result = _onRun();
        _runPromise.set_value(_result);
    }

    // Method to execute post-task logic with the result.
    virtual void onFetch() override
    {
        if (_onFetch) _onFetch(_result);
        _fetchPromise.set_value(_result);
    }

    // Returns a future to the task's execution result.
    std::future<T> runFuture() { return _runPromise.get_future(); }

    // Returns a future to the result of the onFetch method.
    std::future<T> fetchFuture() { return _fetchPromise.get_future(); }

private:
    T _result;
    std::function<T()> _onRun;
    std::function<void(const T &)> _onFetch;
    std::promise<T> _runPromise;
    std::promise<T> _fetchPromise;
};

// Class for a specific task type, inheriting from ITask
template <>
class Task<void> : public ITask
{
public:
    explicit Task(std::function<void()> handler) : _onRun(handler) {}

    virtual ~Task() = default;

    // Method to set a callback function for post-execution logic and return a shared pointer to the task.
    Task<void> *onFetch(std::function<void()> fetch)
    {
        _onFetch = fetch;
        return this;
    }

    // Method to execute the task's logic and set the promise value.
    virtual void onRun() override
    {
        _onRun();
        _runPromise.set_value();
    }

    // Method to execute post-task logic with the result.
    virtual void onFetch() override
    {
        if (_onFetch)
        {
            _onFetch();
            _fetchPromise.set_value();
        }
    }

    // Returns a future to the task's execution result.
    std::future<void> runFuture() { return _runPromise.get_future(); }

    // Returns a future to the result of the onFetch method.
    std::future<void> fetchFuture() { return _fetchPromise.get_future(); }

private:
    std::function<void()> _onRun;
    std::function<void()> _onFetch;
    std::promise<void> _runPromise;
    std::promise<void> _fetchPromise;
};

/// \brief Represents a task queue that holds tasks to be processed.
///
/// This class provides methods to add tasks to the queue and perform task processing in a worker thread.
class TaskQueue
{
public:
    virtual ~TaskQueue() = default;

    /// \brief Add a task to the task queue.
    ///
    /// \param task The task to be added.

    template <typename F>
    auto addTask(F &&task, bool notify = true)
    {
        if constexpr (std::is_invocable<F>::value && !std::is_convertible<F, ITask *>::value)
        {
            using R = std::invoke_result_t<F>;
            auto ptr = new Task<R>(std::forward<F>(task));
            _tasks.push(ptr);
            if (notify) notifyAll();
            return ptr;
        }
        else
        {
            auto ptr = std::forward<F>(task);
            _tasks.push(ptr);
            if (notify) notifyAll();
            return ptr;
        }
    }

    /// \brief Perform the task processing in the worker thread.
    virtual void taskWorkerThread() = 0;

    void notifyAll()
    {
        {
            std::unique_lock<std::mutex> lock(_taskMutex);
            _taskAvailable.notify_all();
        }
    }

    void await()
    {
        while (!_tasks.empty()) std::this_thread::yield();
    }

protected:
    oneapi::tbb::concurrent_queue<ITask *> _tasks;
    std::mutex _taskMutex;
    std::condition_variable _taskAvailable;
};

/// @brief Represents a thread pool manager class
class APPLIB_API ThreadPool
{
public:
    ThreadPool() : _threadsCount(std::thread::hardware_concurrency()) {}
    explicit ThreadPool(unsigned int count) : _threadsCount(count) {}

    /// @brief Stop all worker threads
    void stopThreads();

    /// @brief Get the number of worker threads
    void threadsCount(unsigned int count);

    /// @brief Set the number of worker threads
    unsigned int threadsCount() const;

    /// @brief Prepare worker threads
    void prepareWorkerThreads(TaskQueue *taskQueue);

private:
    unsigned int _threadsCount;
    DArray<std::thread> _threads;
};

/// \brief Represents a task manager that handles task processing.
///
/// It provides methods to perform task processing in worker threads and handle tasks with conditions.
class APPLIB_API TaskManager : public TaskQueue
{
public:
    void destroy();

    void threadPool(ThreadPool *pool) { _threadPool = pool; }

    static TaskManager &global();

    /// \brief Perform the task processing in the worker thread.
    void taskWorkerThread() override;

private:
    ThreadPool *_threadPool;
    std::atomic<bool> _running;

    TaskManager() : _threadPool(nullptr), _running(true) {}
};

class TaskManagerSync : public TaskQueue
{
public:
    /// \brief Get the singleton instance of the TaskManagerSync.
    ///
    /// \return The singleton instance of the TaskManagerSync.
    static TaskManagerSync &global();

    /// \brief Perform the task processing in the main worker thread.
    void taskWorkerThread() override;

    /// \brief Perform the task processing in the fetch worker thread.
    void fetchTaskWorkerThread();

    /// \brief Stop the worker threads for task processing.
    void stopWorkerThreads();

    /// \brief Wait for the all tasks to finish.
    void await();

private:
    std::atomic<bool> _running{true};
    std::atomic<bool> _fetchRunning{true};
    std::thread _executionThread;
    std::thread _fetchThread;
    oneapi::tbb::concurrent_queue<ITask *> _fetchTasks;
    std::condition_variable _tasksEmpty;
    std::condition_variable _fetchTasksEmpty;
    std::mutex _fetchTasksMutex;

    TaskManagerSync();
    ~TaskManagerSync();
    TaskManagerSync(const TaskManagerSync &) = delete;
    TaskManagerSync &operator=(const TaskManagerSync &) = delete;
};

#endif