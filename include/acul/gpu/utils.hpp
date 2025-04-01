#pragma once

/****************************************************
 *  Helper device functions
 *****************************************************/

#include "../point2d.hpp"
#include "device.hpp"

namespace acul
{
    namespace gpu
    {
        inline size_t align_up(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

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
        APPLIB_API vk::CommandBuffer begin_single_time_commands(device &device, queue_family_info &queue);

        /**
         * Ends a single-time command buffer and submits it to the queue.
         *
         * @param commandBuffer The command buffer to end and submit.
         * @param queue The queue family to submit the command buffer to.
         * @param device The device object.
         * @return The result of the submit operation.
         * @throws vk::Error if there is an error submitting or waiting for the command buffer.
         */
        APPLIB_API vk::Result end_single_time_commands(vk::CommandBuffer command_buffer, queue_family_info &queue,
                                                       device &device);

        /// @brief Create a buffer with the given size, usage flags, and memory usage
        /// @param size Size of the buffer in bytes
        /// @param vk_usage Usage flags for the Vulkan buffer
        /// @param vma_usage Memory usage for the buffer
        /// @param buffer Destination Vulkan buffer
        /// @param required_flags: Required memory property flags for the buffer
        /// @param priority: Priority of the buffer
        /// @param allocation: Allocation for the buffer
        /// @param device Device
        /// @return True on success, false on failure
        APPLIB_API bool create_buffer(vk::DeviceSize size, vk::BufferUsageFlags vk_usage, VmaMemoryUsage vma_usage,
                                      vk::Buffer &buffer, vk::MemoryPropertyFlags required_flags, f32 priority,
                                      VmaAllocation &allocation, const device &device);

        /// @brief Copy buffer from source to destination
        /// @param device Device
        /// @param src_buffer Source buffer
        /// @param dst_buffer Destination buffer
        /// @param size Size of the buffer
        inline void copy_buffer(device &device, vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size)
        {
            vk::CommandBuffer command_buffer = begin_single_time_commands(device, device.graphics_queue);
            vk::BufferCopy copy_region(0, 0, size);
            command_buffer.copyBuffer(src_buffer, dst_buffer, 1, &copy_region, device.loader);
            end_single_time_commands(command_buffer, device.graphics_queue, device);
        }

        /// @brief Transit image layout from one layout to another one using pipeline memory barrier
        /// @param device Device
        /// @param image Image
        /// @param old_layout Current layout
        /// @param new_layout New layout
        /// @param mipLevels Image mip level
        APPLIB_API void transition_image_layout(device &device, vk::Image image, vk::ImageLayout old_layout,
                                                vk::ImageLayout new_layout, u32 mipLevels);

        /// @brief Copy buffer to VK image
        /// @param device Device
        /// @param buffer Source buffer
        /// @param image Destination image
        /// @param dimensions Image dimensions
        /// @param layer_count Layer count
        /// @param offset Offset
        inline void copy_buffer_to_image(device &device, vk::Buffer buffer, vk::Image image, point2D<u32> dimensions,
                                         u32 layer_count, point2D<int> offset = {0, 0})
        {
            vk::CommandBuffer command_buffer = begin_single_time_commands(device, device.graphics_queue);
            vk::BufferImageCopy region{};
            region.setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, layer_count})
                .setImageOffset({offset.x, offset.y, 0})
                .setImageExtent({dimensions.x, dimensions.y, 1});
            command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region,
                                             device.loader);
            end_single_time_commands(command_buffer, device.graphics_queue, device);
        }

        /// @brief Copy image from VK Buffer
        /// @param device Device
        /// @param buffer Source buffer
        /// @param image Destination image
        /// @param dimensions Image dimensions
        /// @param layerCount Layer count
        /// @param offset Offset
        inline void copy_image_to_buffer(device &device, vk::Buffer buffer, vk::Image image, point2D<u32> dimensions,
                                         u32 layerCount, point2D<int> offset = {0, 0})
        {
            vk::CommandBuffer commandBuffer = begin_single_time_commands(device, device.graphics_queue);
            vk::BufferImageCopy region{};
            region.setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, layerCount})
                .setImageOffset({offset.x, offset.y, 0})
                .setImageExtent({dimensions.x, dimensions.y, 1});
            commandBuffer.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, 1, &region,
                                            device.loader);
            end_single_time_commands(commandBuffer, device.graphics_queue, device);
        }

        /// @brief Create VK image by ImageCreeateInfo structure
        /// @param imageInfo ImageCreateInfo structure
        /// @param image Destination VK image
        /// @param allocation VMA allocation
        /// @param allocator VMA allocator
        /// @param vma_usage_flags Usage flags for vma allocation
        /// @param vk_mem_flags Memory properties
        /// @param priority Mem priority
        /// @return True on success, false on failure
        APPLIB_API bool create_image(const vk::ImageCreateInfo &image_info, vk::Image &image, VmaAllocation &allocation,
                                     VmaAllocator &allocator,
                                     VmaMemoryUsage vma_usage_flags = VMA_MEMORY_USAGE_GPU_ONLY,
                                     vk::MemoryPropertyFlags vk_mem_flags = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                     f32 priority = 0.5f);
    } // namespace gpu
} // namespace acul