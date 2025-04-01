#include <acul/gpu/utils.hpp>

#define MEM_DEDICATTED_ALLOC_MIN 536870912u

namespace acul
{
    namespace gpu
    {
        vk::CommandBuffer begin_single_time_commands(device &device, queue_family_info &queue)
        {
            vk::CommandBuffer command_buffer;
            device.graphics_queue.pool.primary.request(&command_buffer, 1);
            vk::CommandBufferBeginInfo begin_info;
            begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
            command_buffer.begin(begin_info, device.loader);
            return command_buffer;
        }

        vk::Result end_single_time_commands(vk::CommandBuffer command_buffer, queue_family_info &queue, device &device)
        {
            vk::Fence fence;
            device.fence_pool.request(&fence, 1);
            device.vk_device.resetFences(fence, device.loader);
            command_buffer.end(device.loader);
            vk::SubmitInfo submitInfo;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &command_buffer;
            queue.vk_queue.submit(submitInfo, fence, device.loader);
            auto res = device.vk_device.waitForFences(fence, true, UINT64_MAX, device.loader);
            queue.pool.primary.release(command_buffer);
            device.fence_pool.release(fence);
            return res;
        }

        bool create_buffer(vk::DeviceSize size, vk::BufferUsageFlags vk_usage, VmaMemoryUsage vma_usage,
                           vk::Buffer &buffer, vk::MemoryPropertyFlags required_flags, f32 priority,
                           VmaAllocation &allocation, const device &device)
        {
            vk::BufferCreateInfo buffer_info;
            buffer_info.setSize(size).setUsage(vk_usage).setSharingMode(vk::SharingMode::eExclusive);

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = vma_usage;
            allocInfo.priority = priority;
            allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(required_flags);
            if (size > MEM_DEDICATTED_ALLOC_MIN) allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            auto res = vmaCreateBuffer(device.allocator, reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info),
                                       &allocInfo, reinterpret_cast<VkBuffer *>(&buffer), &allocation, nullptr);
            if (res == VK_SUCCESS) return true;
            logError("Failed to create buffer: %s", vk::to_string((vk::Result)res).c_str());
            return false;
        }

        void transition_image_layout(device &device, vk::Image image, vk::ImageLayout old_layout,
                                     vk::ImageLayout new_layout, u32 mipLevels)
        {
            vk::CommandBuffer command_buffer = begin_single_time_commands(device, device.graphics_queue);
            vk::ImageMemoryBarrier barrier{};
            barrier.setOldLayout(old_layout)
                .setNewLayout(new_layout)
                .setImage(image)
                .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1});
            vk::PipelineStageFlagBits srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            vk::PipelineStageFlagBits dstStage = vk::PipelineStageFlagBits::eTopOfPipe;

            switch (old_layout)
            {
                case vk::ImageLayout::eUndefined:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
                    break;
                case vk::ImageLayout::eTransferDstOptimal:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
                    srcStage = vk::PipelineStageFlagBits::eTransfer;
                    break;
                case vk::ImageLayout::eTransferSrcOptimal:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
                    srcStage = vk::PipelineStageFlagBits::eTransfer;
                    break;
                case vk::ImageLayout::eReadOnlyOptimal:
                case vk::ImageLayout::eShaderReadOnlyOptimal:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
                    srcStage = vk::PipelineStageFlagBits::eFragmentShader;
                    break;
                default:
                    throw acul::runtime_error("Unsupported src layout transition");
            }

            switch (new_layout)
            {
                case vk::ImageLayout::eTransferDstOptimal:
                    barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
                    dstStage = vk::PipelineStageFlagBits::eTransfer;
                    break;
                case vk::ImageLayout::eTransferSrcOptimal:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferRead);
                    srcStage = vk::PipelineStageFlagBits::eTransfer;
                    dstStage = vk::PipelineStageFlagBits::eHost;
                    break;
                case vk::ImageLayout::eColorAttachmentOptimal:
                    barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
                    dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
                    break;
                case vk::ImageLayout::eReadOnlyOptimal:
                case vk::ImageLayout::eShaderReadOnlyOptimal:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
                    barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
                    dstStage = vk::PipelineStageFlagBits::eFragmentShader;
                    break;
                case vk::ImageLayout::eGeneral:
                    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
                    barrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
                    srcStage = vk::PipelineStageFlagBits::eTransfer;
                    dstStage = vk::PipelineStageFlagBits::eTransfer;
                    break;
                default:
                    throw acul::runtime_error("Unsupported dst layout transition");
            }

            command_buffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1,
                                           &barrier, device.loader);
            end_single_time_commands(command_buffer, device.graphics_queue, device);
        }

        bool create_image(const vk::ImageCreateInfo &image_info, vk::Image &image, VmaAllocation &allocation,
                          VmaAllocator &allocator, VmaMemoryUsage vma_usage_flags, vk::MemoryPropertyFlags vk_mem_flags,
                          f32 priority)
        {
            VmaAllocationCreateInfo alloc_info = {};
            alloc_info.usage = vma_usage_flags;
            alloc_info.priority = priority;
            alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(vk_mem_flags);
            alloc_info.priority = priority;

            VkResult result = vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo *>(&image_info),
                                             &alloc_info, reinterpret_cast<VkImage *>(&image), &allocation, nullptr);
            if (result == VK_SUCCESS) return true;
            logError("Failed to create image #%d", (int)result);
            return false;
        }
    } // namespace gpu
} // namespace acul