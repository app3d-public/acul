#include <core/task.hpp>

namespace task
{
    void ServiceDispatch::workerThread()
    {
        while (true)
        {

            auto next_time = std::chrono::steady_clock::time_point::max();
            for (auto service : _services)
            {
                auto service_time = service->dispatch();
                if (service_time < next_time) next_time = service_time;
            }

            std::unique_lock<std::mutex> lock(_mutex);
            if (!_running) break;
            if (next_time == std::chrono::steady_clock::time_point::max())
                _cv.wait(lock, [this] { return !_running; });
            else
                _cv.wait_until(lock, next_time, [this] { return !_running; });
        }
    }

    ServiceDispatch *g_ServiceDispatch{nullptr};

    std::chrono::steady_clock::time_point ScheduleService::dispatch()
    {
        auto now = std::chrono::steady_clock::now();
        Task task;

        while (!_tasks.empty())
        {
            _busy = true;
            if (!_tasks.try_pop(task))
            {
                _busy = false;
                break;
            }
            if (task.time <= now)
            {
                task.task->run();
                _busy = false;
            }
            else
            {
                _tasks.push(task);
                _busy = false;
                return task.time;
            }
        }
        return std::chrono::steady_clock::time_point::max();
    }
} // namespace task