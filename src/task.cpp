#include <acul/task.hpp>

namespace acul
{
    namespace task
    {
        void service_dispatch::worker_thread()
        {
            while (true)
            {
                auto next_time = std::chrono::steady_clock::time_point::max();
                {
                    for (auto service : _services)
                    {
                        auto st = service->dispatch();
                        if (st < next_time) next_time = st;
                    }
                }

                std::unique_lock<std::mutex> lock(_mutex);
                if (!_running) break;
                _cv.wait_until(lock, next_time);
            }
        }

        std::chrono::steady_clock::time_point shedule_service::dispatch()
        {
            auto now = std::chrono::steady_clock::now();
            timed_task task_entry;

            while (!_tasks.empty())
            {
                _busy = true;
                if (!_tasks.try_pop(task_entry))
                {
                    _busy = false;
                    break;
                }
                if (task_entry.time <= now)
                {
                    task_entry.task->run();
                    _busy = false;
                }
                else
                {
                    _tasks.push(task_entry);
                    _busy = false;
                    return task_entry.time;
                }
            }
            return std::chrono::steady_clock::time_point::max();
        }
    } // namespace task
} // namespace acul