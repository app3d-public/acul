#include <core/command_pool.hpp>

void CommandPool::destroy(vk::Device &device, vk::DispatchLoaderDynamic &loader)
{
    for (auto &pool : _pools)
        if (pool.vkPool) device.destroyCommandPool(pool.vkPool, nullptr, loader);
}

void CommandPool::allocate(vk::CommandPoolCreateInfo createInfo, vk::Device &device, vk::DispatchLoaderDynamic &loader,
                           size_t primary, size_t secondary)
{
    for (auto &pool : _pools)
    {
        pool.vkPool = device.createCommandPool(createInfo, nullptr, loader);

        pool.primary.size = primary;
        if (pool.primary.size > 0)
        {
            pool.primary.buffers.resize(pool.primary.size);
            vk::CommandBufferAllocateInfo allocInfo(pool.vkPool, vk::CommandBufferLevel::ePrimary, pool.primary.size);
            vk::Result res = device.allocateCommandBuffers(&allocInfo, pool.primary.buffers.data(), loader);
            if (res != vk::Result::eSuccess) throw std::bad_alloc();
        }

        pool.secondary.size = secondary;
        if (pool.secondary.size > 0)
        {
            pool.secondary.buffers.resize(pool.secondary.size);
            vk::CommandBufferAllocateInfo allocInfo(pool.vkPool, vk::CommandBufferLevel::eSecondary,
                                                    pool.secondary.size);
            vk::Result res = device.allocateCommandBuffers(&allocInfo, pool.secondary.buffers.data(), loader);
            if (res != vk::Result::eSuccess) throw std::bad_alloc();
        }
    }
}

size_t CommandPool::size(vk::CommandBufferLevel level) const
{
    auto &pool = _pools[getThreadID()];
    if (level == vk::CommandBufferLevel::ePrimary)
        return pool.primary.size - pool.primary.pos + pool.primary.releasedBuffers.size();
    return pool.secondary.size - pool.secondary.pos + pool.secondary.releasedBuffers.size();
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
    vk::CommandBufferAllocateInfo allocInfo(_pools[getThreadID()].vkPool, level, size);
    vk::CommandBuffer tmp[size];
    vk::Result res = device.allocateCommandBuffers(&allocInfo, tmp, loader);
    if (res != vk::Result::eSuccess) throw std::bad_alloc();
    for (int ti = 0; ti < size; ++ti)
    {
        pBuffers[i++] = tmp[ti];
        group.buffers.push_back(tmp[ti]);
    }
    group.size += size;
}

void CommandPool::requestBuffers(vk::CommandBuffer *pBuffers, vk::CommandBufferLevel level, size_t size,
                                 vk::Device &device, vk::DispatchLoaderDynamic &loader)
{
    auto &pool = _pools[getThreadID()];
    if (level == vk::CommandBufferLevel::ePrimary)
        requestBuffers(pool.primary, pBuffers, size, vk::CommandBufferLevel::ePrimary, device, loader);
    else
        requestBuffers(pool.secondary, pBuffers, size, vk::CommandBufferLevel::eSecondary, device, loader);
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
    auto &pool = _pools[getThreadID()];
    if (level == vk::CommandBufferLevel::ePrimary)
        releaseBuffer(pool.primary, buffer);
    else
        releaseBuffer(pool.secondary, buffer);
}

void CommandPool::releaseBuffers(vk::CommandBuffer *pBuffers, size_t size, vk::CommandBufferLevel level)
{
    auto &pool = _pools[getThreadID()];
    if (level == vk::CommandBufferLevel::ePrimary)
        for (int i = 0; i < size; ++i) releaseBuffer(pool.primary, pBuffers[i]);
    else
        for (int i = 0; i < size; ++i) releaseBuffer(pool.secondary, pBuffers[i]);
}