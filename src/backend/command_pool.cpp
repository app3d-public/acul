#include <core/backend/command_pool.hpp>

void CommandPool::allocate(vk::CommandPoolCreateInfo createInfo, vk::Device &device, vk::DispatchLoaderDynamic &loader,
                           size_t primary, size_t secondary)
{
    _vkPool = device.createCommandPool(createInfo, nullptr, loader);

    _primary.size = primary;
    if (_primary.size > 0)
    {
        _primary.buffers.resize(_primary.size);
        vk::CommandBufferAllocateInfo allocInfo(_vkPool, vk::CommandBufferLevel::ePrimary, _primary.size);
        vk::Result res = device.allocateCommandBuffers(&allocInfo, _primary.buffers.data(), loader);
        if (res != vk::Result::eSuccess) throw std::bad_alloc();
    }

    _secondary.size = secondary;
    if (_secondary.size > 0)
    {
        _secondary.buffers.resize(_secondary.size);
        vk::CommandBufferAllocateInfo allocInfo(_vkPool, vk::CommandBufferLevel::eSecondary, _secondary.size);
        vk::Result res = device.allocateCommandBuffers(&allocInfo, _secondary.buffers.data(), loader);
        if (res != vk::Result::eSuccess) throw std::bad_alloc();
    }
}

void CommandPool::requestBuffers(_CommandGroup &group, vk::CommandBuffer *pBuffers, size_t size,
                                 vk::CommandBufferLevel level, vk::Device &device, vk::DispatchLoaderDynamic &loader)
{
    int i = 0;

    // Use available buffers in the group
    for (; size > 0 && group.pos < group.size; ++i, ++group.pos, --size) { pBuffers[i] = group.buffers[group.pos]; }
    if (size == 0) return;

    // Use released buffers
    for (; size > 0 && !group.releasedBuffers.empty(); ++i, --size)
    {
        size_t id = group.releasedBuffers.front();
        group.releasedBuffers.pop();
        pBuffers[i] = group.buffers[id];
    }
    if (size == 0) return;

    // Allocate new buffers
    vk::CommandBufferAllocateInfo allocInfo(_vkPool, level, size);
    vk::CommandBuffer tmp[size];
    vk::Result res = device.allocateCommandBuffers(&allocInfo, tmp, loader);
    if (res != vk::Result::eSuccess) throw std::bad_alloc();
    for (int ti = 0; ti < size; ++ti)
    {
        pBuffers[i++] = tmp[ti];
        group.buffers.push_back(tmp[ti]);
    }
    group.size += size;
    group.pos = group.size;
}

void CommandPool::requestBuffers(vk::CommandBuffer *pBuffers, vk::CommandBufferLevel level, size_t size,
                                 vk::Device &device, vk::DispatchLoaderDynamic &loader)
{
    if (level == vk::CommandBufferLevel::ePrimary)
        requestBuffers(_primary, pBuffers, size, vk::CommandBufferLevel::ePrimary, device, loader);
    else
        requestBuffers(_secondary, pBuffers, size, vk::CommandBufferLevel::eSecondary, device, loader);
}

void CommandPool::releaseBuffer(_CommandGroup &group, vk::CommandBuffer buffer)
{
    auto it = std::find(group.buffers.begin(), group.buffers.end(), buffer);
    if (it != group.buffers.end())
    {
        size_t index = std::distance(group.buffers.begin(), it);
        group.releasedBuffers.push(index);
    }
}

void CommandPool::releaseBuffer(vk::CommandBuffer buffer, vk::CommandBufferLevel level)
{
    if (level == vk::CommandBufferLevel::ePrimary)
        releaseBuffer(_primary, buffer);
    else
        releaseBuffer(_secondary, buffer);
}

void CommandPool::releaseBuffers(vk::CommandBuffer *pBuffers, size_t size, vk::CommandBufferLevel level)
{
    if (level == vk::CommandBufferLevel::ePrimary)
        for (int i = 0; i < size; ++i) releaseBuffer(_primary, pBuffers[i]);
    else
        for (int i = 0; i < size; ++i) releaseBuffer(_secondary, pBuffers[i]);
}