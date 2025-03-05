#ifndef APP_CORE_TASK_H
#define APP_CORE_TASK_H

#include <future>
#include <oneapi/tbb/concurrent_priority_queue.h>
#include <oneapi/tbb/task.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>
#include "api.hpp"
#include "disposal_queue.hpp"
#include "event.hpp"

#define TASK_EVENT_UPDATE_SIGN 0x3E916882EBB697C3
#define TASK_EVENT_DONE_SIGN   0x2B56484DF4085AA6

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
    class Task final : public ITask, public astl::enable_shared_from_this<Task<T>>
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
    };

    template <typename F>
    auto addTask(F &&task)
    {
        if constexpr (std::is_invocable<F>::value)
        {
            using R = std::invoke_result_t<F>;
            auto ptr = astl::make_shared<Task<R>>(std::forward<F>(task));
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
        explicit MemCache(const astl::shared_ptr<ITask> &task) : _task(task) {}

        virtual void free() override { _task->run(); }

    private:
        astl::shared_ptr<ITask> _task;
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

        UpdateEvent(void *ctx = nullptr, const std::string &header = "", const std::string &message = "",
                    f32 progress = 0.0f)
            : IEvent(TASK_EVENT_UPDATE_SIGN), ctx(ctx), header(header), message(message), progress(progress)
        {
        }
    };

    class ServiceDispatch;

    class IService
    {
        friend class ServiceDispatch;

    public:
        virtual ~IService() = default;

        virtual std::chrono::steady_clock::time_point dispatch() = 0;

        virtual void await(bool force = false) = 0;

        void notify();

    protected:
        ServiceDispatch *_sd{nullptr};
    };

    class ServiceDispatch
    {
    public:
        ServiceDispatch() : _running(true), _thread(&ServiceDispatch::workerThread, this) {}

        ~ServiceDispatch()
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _running = false;
                _cv.notify_all();
            }
            if (_thread.joinable()) _thread.join();
            for (auto service : _services) astl::release(service);
        }

        void registerService(IService *service)
        {
            _services.push_back(service);
            service->_sd = this;
        }

    private:
        bool _running{true};
        std::thread _thread;
        std::mutex _mutex;
        std::condition_variable _cv;
        astl::vector<IService *> _services;

        APPLIB_API void workerThread();

        friend class IService;
    };

    extern APPLIB_API ServiceDispatch *g_ServiceDispatch;

    inline void IService::notify() { _sd->_cv.notify_one(); }

    class APPLIB_API ScheduleService final : public IService
    {
    public:
        virtual std::chrono::steady_clock::time_point dispatch() override;

        template <typename F>
        void addTask(F &&task, std::chrono::steady_clock::time_point time)
        {
            _tasks.emplace(addTask(std::forward<F>(task)), time);
            notify();
        }

        virtual void await(bool force = false) override
        {
            if (force) _tasks.clear();
            while (!_tasks.empty() || _busy.load(std::memory_order_acquire)) std::this_thread::yield();
        }

    private:
        struct Task
        {
            astl::shared_ptr<ITask> task;
            std::chrono::steady_clock::time_point time;

            bool operator<(const Task &other) const { return time > other.time; }
        };

        oneapi::tbb::concurrent_priority_queue<Task> _tasks;
        std::atomic<bool> _busy{false};
    };
} // namespace task

#endif