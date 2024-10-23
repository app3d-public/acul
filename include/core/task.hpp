#ifndef APP_CORE_TASK_H
#define APP_CORE_TASK_H

#include <future>
#include <memory>
#include <oneapi/tbb/task.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>
#include "api.hpp"
#include "disposal_queue.hpp"
#include "event.hpp"

namespace task
{

    class ITask
    {
    public:
        virtual ~ITask() = default;

        virtual void run() = 0;

        virtual void await() = 0;
    };

    /**
     * @brief Class representing an asynchronous task.
     *
     * This class allows chaining of tasks and provides mechanisms to run and await the completion of tasks.
     *
     * @tparam T The return type of the task.
     */
    template <typename T>
    class Task final : public ITask, public std::enable_shared_from_this<Task<T>>
    {
    public:
        /**
         * @brief Constructs a Task with a callback.
         *
         * @tparam T The return type of the task.
         * @param handler The function to be executed by the task.
         * @param ctx The task group context.
         */
        explicit Task(std::function<T()> handler, oneapi::tbb::task_group_context *ctx = nullptr)
            : _ctx{ctx}, _handler(handler), _future(_promise.get_future())
        {
        }

        /**
         * Executes the task and recursively runs the next task in the chain.
         *
         * If the task return type is void, the handler function is called and the promise is set to a value.
         * Otherwise, the handler function is called and its return value is set as the value of the promise.
         *
         * @throws None
         */
        virtual void run() override
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
                auto nextTask = std::make_shared<Task<R>>(std::forward<F>(task), _ctx);
                _next = nextTask;
                return this->shared_from_this();
            }
            else
            {
                using R = std::invoke_result_t<F, T>;
                auto nextTask =
                    std::make_shared<Task<R>>([this, task = std::forward<F>(task)] { task(_future.get()); }, _ctx);
                _next = nextTask;
                return this->shared_from_this();
            }
        }

        /**
         * @brief Awaits the completion of the task.
         *
         * Waits for the promise to be set.
         */
        virtual void await() override { _future.wait(); }

        /**
         * @brief Gets the result of the task.
         *
         * @tparam R The type of the result (default is T).
         * @return The result of the task.
         */
        template <typename R = T, typename = std::enable_if_t<!std::is_void_v<R>, R>>
        R get()
        {
            if (_ctx && _ctx->is_group_execution_cancelled()) return R();
            return _future.get();
        }

    private:
        oneapi::tbb::task_group_context *_ctx;
        std::function<T()> _handler;
        std::promise<T> _promise;
        std::shared_future<T> _future;
        std::shared_ptr<ITask> _next;
    };

    template <typename F>
    auto addTask(F &&task)
    {
        if constexpr (std::is_invocable<F>::value)
        {
            using R = std::invoke_result_t<F>;
            auto ptr = std::make_shared<Task<R>>(std::forward<F>(task));
            return ptr;
        }
        else
        {
            auto ptr = std::forward<F>(task);
            return ptr;
        }
    }

    class MemCache : public ::MemCache
    {
    public:
        explicit MemCache(const std::shared_ptr<ITask> &task) : _task(task) {}

        virtual void free() override { _task->run(); }

    private:
        std::shared_ptr<ITask> _task;
    };

    /**
     * @brief Class for managing and running tasks.
     *
     * This class provides mechanisms to add tasks and await their completion.
     */
    class APPLIB_API ThreadDispatch
    {
    public:
        ThreadDispatch() : _ctx(oneapi::tbb::task_group_context::isolated), _group(_ctx) {}

        /// \brief Awaits the completion of all tasks in the task group.
        // \param force If true, all tasks in the group will be cancelled.
        void await(bool force = false)
        {
            if (force) _ctx.cancel_group_execution();
            _group.wait();
        }

        /**
         * @brief Adds a new task to the task group.
         *
         * @tparam F The type of the task function.
         * @param task The task function to be added.
         * @return A shared pointer to the added task.
         */
        template <typename F>
        inline auto dispatch(F &&task)
        {
            auto ptr = addTask(std::forward<F>(task));
            _group.run([ptr]() { ptr->run(); });
            return ptr;
        }

    private:
        oneapi::tbb::task_group_context _ctx;
        oneapi::tbb::task_group _group;
    };

    struct UpdateEvent final : public events::IEvent
    {
        void *ctx;
        std::string header;
        std::string message;
        f32 progress;

        UpdateEvent(const std::string &name, void *ctx = nullptr, const std::string &header = "",
                    const std::string &message = "", f32 progress = 0.0f)
            : IEvent(name), ctx(ctx), header(header), message(message), progress(progress)
        {
        }
    };
} // namespace task

#endif