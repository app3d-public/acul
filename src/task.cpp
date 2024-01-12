#include "task.hpp"
#include <memory>
#include <mutex>

void ThreadPool::threadsCount(unsigned int count) { _threadsCount = count; }
unsigned int ThreadPool::threadsCount() const { return _threadsCount; }

void ThreadPool::prepareWorkerThreads(TaskQueue *taskQueue)
{
    for (unsigned int i = 0; i < _threadsCount; i++)
        _threads.push_back(std::thread(&TaskQueue::taskWorkerThread, taskQueue));
}

void ThreadPool::stopThreads()
{
    for (auto &thread : _threads)
        if (thread.joinable())
            thread.join();
}

void TaskManager::taskWorkerThread()
{
    while (_running)
    {
        std::shared_ptr<ITask> task;
        if (_tasks.try_pop(task))
        {
            task->onRun();
            task->onFetch();
        }
        else
        {
            std::unique_lock<std::mutex> lock(_taskMutex);
            _taskAvailable.wait(lock, [this]() { return !_tasks.empty() || !_running; });
        }
    }
}

TaskManager::~TaskManager()
{
    _running = false;
    notifyAll();
    _threadPool.stopThreads();
}

TaskManagerSync::TaskManagerSync()
{
    _executionThread = std::thread(&TaskManagerSync::taskWorkerThread, this);
    _fetchThread = std::thread(&TaskManagerSync::fetchTaskWorkerThread, this);
}

TaskManagerSync &TaskManagerSync::getSingleton()
{
    static TaskManagerSync singleton;
    return singleton;
}

void TaskManagerSync::taskWorkerThread()
{
    while (_running)
    {
        std::shared_ptr<ITask> task;
        if (_tasks.try_pop(task))
        {
            task->onRun();
            _fetchTasks.push(task); // Добавляем задачу в очередь обработки
        }
        else
        {
            {
                std::unique_lock<std::mutex> lock(_taskMutex);
                _tasksEmpty.notify_all();
            }

            {
                std::unique_lock<std::mutex> lock(_taskMutex);
                _taskAvailable.wait(lock, [this]() { return !_tasks.empty() || !_running; });
            }
        }
    }
}

void TaskManagerSync::fetchTaskWorkerThread()
{
    while (_fetchRunning)
    {
        std::shared_ptr<ITask> task;
        if (_fetchTasks.try_pop(task))
            task->onFetch();
        else
        {
            std::unique_lock<std::mutex> lock(_fetchTasksMutex);
            _fetchTasksEmpty.notify_all();
        }
    }
}

void TaskManagerSync::await()
{
    {
        std::unique_lock<std::mutex> lock(_taskMutex);
        _tasksEmpty.wait(lock, [this]() { return _tasks.empty(); });
    }

    {
        std::unique_lock<std::mutex> lock(_fetchTasksMutex);
        _fetchTasksEmpty.wait(lock, [this]() { return _fetchTasks.empty(); });
    }
}

TaskManagerSync::~TaskManagerSync()
{
    _running = false;
    if (_executionThread.joinable())
        _executionThread.join();
    _fetchRunning = false;
    if (_fetchThread.joinable())
        _fetchThread.join();
}