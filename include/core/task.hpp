#ifndef APP_CORE_TASK_H
#define APP_CORE_TASK_H

#include <future>
#include <oneapi/tbb/task_group.h>
#include "api.hpp"

/**
 * @brief Class representing an asynchronous task.
 *
 * This class allows chaining of tasks and provides mechanisms to run and await the completion of tasks.
 *
 * @tparam T The return type of the task.
 */
template <typename T>
class Task final : public std::enable_shared_from_this<Task<T>>
{
public:
    /**
     * @brief Constructs a Task with a callback.
     *
     * @param handler The function to be executed by the task.
     */
    explicit Task(std::function<T()> handler) : _handler(handler), _future(_promise.get_future()) {}

    /**
     * Executes the task and recursively runs the next task in the chain.
     *
     * If the task return type is void, the handler function is called and the promise is set to a value.
     * Otherwise, the handler function is called and its return value is set as the value of the promise.
     *
     * @throws None
     */
    void run()
    {
        if constexpr (std::is_void<T>::value)
        {
            _handler();
            _promise.set_value();
        }
        else
            _promise.set_value(_handler());
        if (_next) _next->run();
    }

    /**
     * @brief Chains another task to be executed after the current task.
     *
     * @tparam F The type of the next task function.
     * @param task The next task function to be executed.
     * @return A shared pointer to the next task.
     */
    template <typename F>
    auto next(F &&task)
    {
        if constexpr (std::is_void_v<T>)
        {
            using R = std::invoke_result_t<F>;
            auto nextTask = std::make_shared<Task<R>>(std::forward<F>(task));
            _next = nextTask;
            return _next;
        }
        else
        {
            using R = std::invoke_result_t<F, T>;
            auto nextTask = std::make_shared<Task<R>>([this, task = std::forward<F>(task)] { task(_future.get()); });
            _next = nextTask;
            return _next;
        }
    }

    /**
     * @brief Awaits the completion of the task.
     *
     * Waits for the promise to be set.
     */
    void await() { _future.wait(); }

    /**
     * @brief Gets the result of the task.
     *
     * @tparam R The type of the result (default is T).
     * @return The result of the task.
     */
    template <typename R = T, typename = std::enable_if_t<!std::is_void_v<R>, R>>
    R get()
    {
        return _future.get();
    }

private:
    std::function<T()> _handler;
    std::promise<T> _promise;
    std::shared_future<T> _future;
    std::shared_ptr<Task<void>> _next;
};

/**
 * @brief Class for managing and running tasks.
 *
 * This class provides mechanisms to add tasks and await their completion.
 */
class TaskManager
{
public:
    // @brief Awaits the completion of all tasks in the task group.
    void await() { _group.wait(); }

    /**
     * @brief Returns the global instance of the TaskManager.
     *
     * @return The global TaskManager instance.
     */
    static APPLIB_API TaskManager &global();

    /**
     * @brief Adds a new task to the task group.
     *
     * @tparam F The type of the task function.
     * @param task The task function to be added.
     * @return A shared pointer to the added task.
     */
    template <typename F>
    auto addTask(F &&task)
    {
        if constexpr (std::is_invocable<F>::value)
        {
            using R = std::invoke_result_t<F>;
            auto ptr = std::make_shared<Task<R>>(std::forward<F>(task));
            _group.run([ptr]() { ptr->run(); });
            return ptr;
        }
        else
        {
            auto ptr = std::forward<F>(task);
            _group.run([ptr]() { ptr->run(); });
            return ptr;
        }
    }

private:
    oneapi::tbb::task_group _group;
};
#endif