#include <core/backend/fence_pool.hpp>

void FencePool::requestFences(vk::Fence *pFences, size_t size, vk::Device &device, vk::DispatchLoaderDynamic &loader)
{
    int i = 0;
    // Use available fences
    for (; size > 0 && _pos < _size; ++i, ++_pos, --size) { pFences[i] = _fences[_pos]; }
    if (size == 0) return;

    // Use released buffers
    while (size > 0 && !_released.empty())
    {
        size_t id = _released.front();
        _released.pop();
        pFences[i++] = _fences[id];
        --size;
    }
    if (size == 0) return;

    // Allocate new fences
    for (int ti = 0; ti < size; ++ti)
    {
        _fences.push_back(device.createFence({vk::FenceCreateFlagBits::eSignaled}, nullptr, loader));
        pFences[i++] = _fences[_size + ti];
    }

    _size += size;
    _pos = _size;
}

void FencePool::releaseFence(vk::Fence fence)
{
    auto it = std::find(_fences.begin(), _fences.end(), fence);
    if (it != _fences.end())
    {
        size_t index = std::distance(_fences.begin(), it);
        _released.push(index);
    }
}