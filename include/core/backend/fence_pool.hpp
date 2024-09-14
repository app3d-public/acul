#pragma once

#include <vulkan/vulkan.hpp>
#include "../api.hpp"
#include "../std/basic_types.hpp"
#include "../std/vector.hpp"

class APPLIB_API FencePool
{
public:
    FencePool() : _size(0), _pos(0) {}

    /**
     * @brief Destroys the fence pool.
     * @param device The Vulkan device.
     * @param loader The Vulkan dispatch loader.
     */
    void destroy(vk::Device &device, vk::DispatchLoaderDynamic &loader)
    {
        for (auto &fence : _fences) device.destroyFence(fence, nullptr, loader);
        _fences.clear();
        _size = 0;
        _pos = 0;
    }

    void allocate(size_t size, vk::Device &device, vk::DispatchLoaderDynamic &loader)
    {
        _size = size;
        _fences.resize(size);
        for (auto &fence : _fences) fence = device.createFence({vk::FenceCreateFlagBits::eSignaled}, nullptr, loader);
    }

    void requestFences(vk::Fence *pFences, size_t size, vk::Device &device, vk::DispatchLoaderDynamic &loader);

    void releaseFence(vk::Fence fence);

    void releaseFences(vk::Fence *pFences, size_t size)
    {
        for (int i = 0; i < size; ++i) releaseFence(pFences[i]);
    }

    size_t size() const { return _size - _pos + _released.size(); }

private:
    size_t _size = 0;
    size_t _pos = 0;
    astl::vector<vk::Fence> _fences;
    astl::queue<size_t> _released;
};