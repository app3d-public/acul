#include <core/task.hpp>

TaskManager &TaskManager::global()
{
    static TaskManager singleton;
    return singleton;
}