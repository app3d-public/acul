#pragma once

#include <vulkan/vulkan.hpp>
#include "../api.hpp"
#include "../std/basic_types.hpp"
#include "../std/darray.hpp"

/**
 * @class CommandPool
 * @brief Manages a pool of Vulkan command buffers.
 */
class APPLIB_API CommandPool
{
public:
    /**
     * @brief Destroys the command pool.
     * @param device The Vulkan device.
     * @param loader The Vulkan dispatch loader.
     */
    void destroy(vk::Device &device, vk::DispatchLoaderDynamic &loader)
    {
        if (_vkPool) device.destroyCommandPool(_vkPool, nullptr, loader);
    }

    /**
     * @brief Gets the Vulkan command pool.
     * @return The Vulkan command pool.
     */
    vk::CommandPool vkPool() const { return _vkPool; }

    /**
     * @brief Gets the size of the specified command buffer level.
     * @param level The command buffer level (primary or secondary).
     * @return The size of the command buffer group.
     */
    size_t size(vk::CommandBufferLevel level) const
    {
        if (level == vk::CommandBufferLevel::ePrimary)
            return _primary.size - _primary.pos + _primary.releasedBuffers.size();
        return _secondary.size - _secondary.pos + _secondary.releasedBuffers.size();
    }

    /**
     * @brief Allocates the command pool with the specified creation info.
     * @param createInfo The command pool creation info.
     * @param device The Vulkan device.
     * @param loader The Vulkan dispatch loader.
     * @param primary The number of primary command buffers to allocate.
     * @param secondary The number of secondary command buffers to allocate.
     */
    void allocate(vk::CommandPoolCreateInfo createInfo, vk::Device &device, vk::DispatchLoaderDynamic &loader,
                  size_t primary = 0, size_t secondary = 0);

    /**
     * @brief Requests command buffers.
     * @param pBuffers Pointer to the array of command buffers to be filled.
     * @param level The command buffer level (primary or secondary).
     * @param size The number of command buffers to request.
     * @param device The Vulkan device.
     * @param loader The Vulkan dispatch loader.
     */
    void requestBuffers(vk::CommandBuffer *pBuffers, vk::CommandBufferLevel level, size_t size, vk::Device &device,
                        vk::DispatchLoaderDynamic &loader);

    /**
     * @brief Releases a command buffer back to the pool.
     * @param buffer The command buffer to release.
     * @param level The command buffer level (primary or secondary).
     */
    void releaseBuffer(vk::CommandBuffer buffer, vk::CommandBufferLevel level);

    /**
     * @brief Releases multiple command buffers back to the pool.
     * @param pBuffers Pointer to the array of command buffers to release.
     * @param size The number of command buffers to release.
     * @param level The command buffer level (primary or secondary).
     */
    void releaseBuffers(vk::CommandBuffer *pBuffers, size_t size, vk::CommandBufferLevel level);

private:
    struct _CommandGroup
    {
        size_t size;
        size_t pos;
        DArray<vk::CommandBuffer> buffers;
        Queue<size_t> releasedBuffers;
    };

    vk::CommandPool _vkPool;
    _CommandGroup _primary;
    _CommandGroup _secondary;

    void requestBuffers(_CommandGroup &group, vk::CommandBuffer *pBuffers, size_t size, vk::CommandBufferLevel level,
                        vk::Device &device, vk::DispatchLoaderDynamic &loader);

    void releaseBuffer(_CommandGroup &group, vk::CommandBuffer buffer);
};