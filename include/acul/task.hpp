#ifndef APP_ACUL_TASK_H
#define APP_ACUL_TASK_H

#include <future>
#include <oneapi/tbb/concurrent_priority_queue.h>
#include <oneapi/tbb/task.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/task_group.h>
#include "string/string.hpp"

#ifdef _WIN32
    #include <processthreadsapi.h>
#endif
#include "api.hpp"
#include "disposal_queue.hpp"
#include "event.hpp"

#define TASK_EVENT_UPDATE_SIGN 0x3E916882EBB697C3
#define TASK_EVENT_DONE_SIGN   0x2B56484DF4085AA6

namespace acul
{
    namespace task
    {
        class task_base
        {
        public:
            virtual ~task_base() = default;

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
        class task final : public task_base, public acul::enable_shared_from_this<task<T>>
        {
        public:
            /**
             * @brief Constructs a Task with a callback.
             *
             * @tparam T The return type of the task.
             * @param handler The function to be executed by the task.
             * @param ctx The task group context.
             */
            explicit task(std::function<T()> handler, oneapi::tbb::task_group_context *ctx = nullptr)
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
        auto add_task(F &&task)
        {
            if constexpr (std::is_invocable<F>::value)
            {
                using R = std::invoke_result_t<F>;
                auto ptr = acul::make_shared<acul::task::task<R>>(std::forward<F>(task));
                return ptr;
            }
            else
            {
                auto ptr = std::forward<F>(task);
                return ptr;
            }
        }

        class mem_cache : public acul::mem_cache
        {
        public:
            explicit mem_cache(const shared_ptr<task_base> &task) : _task(task) {}

            virtual void free() override { _task->run(); }

        private:
            shared_ptr<task_base> _task;
        };

        /**
         * @brief Class for managing and running tasks.
         *
         * This class provides mechanisms to add tasks and await their completion.
         */
        class APPLIB_API thread_dispatch
        {
        public:
            thread_dispatch() : _ctx(oneapi::tbb::task_group_context::isolated), _group(_ctx) {}

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
                auto ptr = add_task(std::forward<F>(task));
                _group.run([ptr]() { ptr->run(); });
                return ptr;
            }

        private:
            oneapi::tbb::task_group_context _ctx;
            oneapi::tbb::task_group _group;
        };

        struct update_event final : public events::event
        {
            void *ctx;
            string header;
            string message;
            f32 progress;

            update_event(void *ctx = nullptr, const string &header = {}, const string &message = {},
                         f32 progress = 0.0f)
                : event(TASK_EVENT_UPDATE_SIGN), ctx(ctx), header(header), message(message), progress(progress)
            {
            }
        };

        class service_dispatch;

        class service_base
        {
            friend class service_dispatch;

        public:
            virtual ~service_base() = default;

            virtual std::chrono::steady_clock::time_point dispatch() = 0;

            virtual void await(bool force = false) = 0;

            void notify();

        protected:
            service_dispatch *_sd{nullptr};
        };

        class service_dispatch
        {
        public:
            service_dispatch() : _running(true), _thread(&service_dispatch::worker_thread, this) {}

            ~service_dispatch()
            {
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _running = false;
                    _cv.notify_all();
                }
                if (_thread.joinable()) _thread.join();
                for (auto service : _services) release(service);
            }

            void register_service(service_base *service)
            {
                _services.push_back(service);
                service->_sd = this;
            }

        private:
            bool _running{true};
            std::thread _thread;
            std::mutex _mutex;
            std::condition_variable _cv;
            vector<service_base *> _services;

            APPLIB_API void worker_thread();

            friend class service_base;
        };

        extern APPLIB_API service_dispatch *g_Service_dispatch;

        inline void service_base::notify() { _sd->_cv.notify_one(); }

        class APPLIB_API shedule_service final : public service_base
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
            struct timed_task
            {
                shared_ptr<task_base> task;
                std::chrono::steady_clock::time_point time;

                bool operator<(const timed_task &other) const { return time > other.time; }
            };

            oneapi::tbb::concurrent_priority_queue<timed_task> _tasks;
            std::atomic<bool> _busy{false};
        };

        inline int get_thread_id()
        {
#ifdef _WIN32
            return GetCurrentThreadId();
#else
            return syscall(SYS_gettid);
#endif
        }
    } // namespace task
} // namespace acul

#endif