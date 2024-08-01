#include <core/task.hpp>

std::atomic<bool> cancelFlag{false};

TaskManager &TaskManager::global()
{
    static TaskManager singleton;
    return singleton;
}