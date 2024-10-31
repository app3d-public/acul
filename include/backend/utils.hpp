#pragma once

/****************************************************
 *  Helper device functions
 *****************************************************/

#include "device.hpp"

inline size_t alignUp(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

/**
 * Allocates a single-time command buffer and begins recording commands.
 *
 * @param device The device object.
 * @param queue The queue family.
 *
 * @return The allocated and begun command buffer.
 *
 * @throws vk::Error if the command buffer allocation fails.
 */
APPLIB_API vk::CommandBuffer beginSingleTimeCommands(Device &device, QueueFamilyInfo &queue);

/**
 * Ends a single-time command buffer and submits it to the queue.
 *
 * @param commandBuffer The command buffer to end and submit.
 * @param queue The queue family to submit the command buffer to.
 * @param device The device object.
 * @return The result of the submit operation.
 * @throws vk::Error if there is an error submitting or waiting for the command buffer.
 */
APPLIB_API vk::Result endSingleTimeCommands(vk::CommandBuffer commandBuffer, QueueFamilyInfo &queue, Device &device);

/// @brief Create a buffer with the given size, usage flags, and memory usage
/// @param size Size of the buffer in bytes
/// @param vkUsage Usage flags for the Vulkan buffer
/// @param vmaUsage Memory usage for the buffer
/// @param buffer Destination Vulkan buffer
/// @param requiredFlags: Required memory property flags for the buffer
/// @param priority: Priority of the buffer
/// @param allocation: Allocation for the buffer
/// @param device Device
/// @return True on success, false on failure
APPLIB_API bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags vkUsage, VmaMemoryUsage vmaUsage,
                             vk::Buffer &buffer, vk::MemoryPropertyFlags requiredFlags, f32 priority,
                             VmaAllocation &allocation, const Device &device);

/// @brief Copy buffer from source to destination
/// @param device Device
/// @param srcBuffer Source buffer
/// @param dstBuffer Destination buffer
/// @param size Size of the buffer
APPLIB_API void copyBuffer(Device &device, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

/// @brief Transit image layout from one layout to another one using pipeline memory barrier
/// @param device Device
/// @param image Image
/// @param oldLayout Current layout
/// @param newLayout New layout
/// @param mipLevels Image mip level
APPLIB_API void transitionImageLayout(Device &device, vk::Image image, vk::ImageLayout oldLayout,
                                      vk::ImageLayout newLayout, u32 mipLevels);

/// @brief Copy buffer to VK image
/// @param device Device
/// @param buffer Source buffer
/// @param image Destination image
/// @param width Image width
/// @param height Image height
/// @param layerCount Layer count
/// @param offsetX Offset X
/// @param offsetY Offset Y
APPLIB_API void copyBufferToImage(Device &device, vk::Buffer buffer, vk::Image image, u32 width, u32 height,
                                  u32 layerCount, int offsetX = 0, int offsetY = 0);

/// @brief Copy image from VK Buffer
/// @param device Device
/// @param buffer Source buffer
/// @param image Destination image
/// @param width Image width
/// @param height Image height
/// @param layerCount Layer count
/// @param offsetX Offset X
/// @param offsetY Offset Y
APPLIB_API void copyImageToBuffer(Device &device, vk::Buffer buffer, vk::Image image, u32 width, u32 height,
                                  u32 layerCount, int offsetX, int offsetY);

/// @brief Create VK image by ImageCreeateInfo structure
/// @param imageInfo ImageCreateInfo structure
/// @param image Destination VK image
/// @param allocation VMA allocation
/// @param allocator VMA allocator
/// @param vmaUsageFlags Usage flags for vma allocation
/// @param vkMemFlags Memory properties
/// @param priority Mem priority
/// @return True on success, false on failure
APPLIB_API bool createImageWithInfo(const vk::ImageCreateInfo &imageInfo, vk::Image &image, VmaAllocation &allocation,
                                    VmaAllocator &allocator, VmaMemoryUsage vmaUsageFlags = VMA_MEMORY_USAGE_GPU_ONLY,
                                    vk::MemoryPropertyFlags vkMemFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                    f32 priority = 0.5f);