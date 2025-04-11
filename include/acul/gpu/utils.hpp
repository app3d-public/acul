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
        inline size_t get_alignment(size_t size, size_t min_offset)
        {
            return min_offset > 0 ? (size + min_offset - 1) & ~(min_offset - 1) : size;
        }

        struct single_time_exec
        {
            vk::CommandBuffer command_buffer;
            vk::Device &vk_device;
            queue_family_info &queue;
            resource_pool<vk::Fence, fence_pool_alloc> &fence_pool;
            vk::DispatchLoaderDynamic &loader;

            single_time_exec(device &device)
                : vk_device(device.vk_device),
                  queue(device.details->queues.graphics),
                  fence_pool(device.details->fence_pool),
                  loader(device.loader)
            {
                queue.pool.primary.request(&command_buffer, 1);
                vk::CommandBufferBeginInfo begin_info;
                begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
                command_buffer.begin(begin_info, loader);
            }

            APPLIB_API vk::Result end();
        };

        inline void copy_buffer_to_image(single_time_exec &exec, vk::Buffer buffer, vk::Image image,
                                         point2D<u32> dimensions, u32 layer_count, point2D<int> offset = {0, 0})
        {
            vk::BufferImageCopy region{};
            region.setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, layer_count})
                .setImageOffset({offset.x, offset.y, 0})
                .setImageExtent({dimensions.x, dimensions.y, 1});
            exec.command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region,
                                                  exec.loader);
        }

        inline vk::Result copy_buffer_to_image(device &device, vk::Buffer buffer, vk::Image image,
                                               point2D<u32> dimensions, u32 layer_count, point2D<int> offset = {0, 0})
        {
            single_time_exec exec{device};
            copy_buffer_to_image(exec, buffer, image, dimensions, layer_count, offset);
            return exec.end();
        }

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
            single_time_exec exec{device};
            vk::BufferCopy copy_region(0, 0, size);
            exec.command_buffer.copyBuffer(src_buffer, dst_buffer, 1, &copy_region, exec.loader);
            exec.end();
        }

        /// @brief Transit image layout from one layout to another one using pipeline memory barrier
        /// @param device Device
        /// @param image Image
        /// @param old_layout Current layout
        /// @param new_layout New layout
        /// @param mipLevels Image mip level
        APPLIB_API void transition_image_layout(single_time_exec &exec, vk::Image image, vk::ImageLayout old_layout,
                                                vk::ImageLayout new_layout, u32 mipLevels);

        inline vk::Result transition_image_layout(device &device, vk::Image image, vk::ImageLayout old_layout,
                                                  vk::ImageLayout new_layout, u32 mipLevels)
        {
            single_time_exec exec{device};
            transition_image_layout(exec, image, old_layout, new_layout, mipLevels);
            return exec.end();
        }

        /// @brief Copy buffer to VK image
        /// @param device Device
        /// @param buffer Source buffer
        /// @param image Destination image
        /// @param dimensions Image dimensions
        /// @param layer_count Layer count
        /// @param offset Offset

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
            single_time_exec info{device};
            vk::BufferImageCopy region{};
            region.setBufferOffset(0)
                .setBufferRowLength(0)
                .setBufferImageHeight(0)
                .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, layerCount})
                .setImageOffset({offset.x, offset.y, 0})
                .setImageExtent({dimensions.x, dimensions.y, 1});
            info.command_buffer.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, 1, &region,
                                                  info.loader);
            info.end();
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