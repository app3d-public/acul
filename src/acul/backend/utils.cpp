#include <acul/backend/utils.hpp>
#include <core/log.hpp>

#define MEM_DEDICATTED_ALLOC_MIN 536870912u

vk::CommandBuffer beginSingleTimeCommands(Device &device, QueueFamilyInfo &queue)
{
    vk::CommandBuffer commandBuffer;
    device.graphicsQueue.pool.primary.request(&commandBuffer, 1);
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo, device.vkLoader);
    return commandBuffer;
}

vk::Result endSingleTimeCommands(vk::CommandBuffer commandBuffer, QueueFamilyInfo &queue, Device &device)
{
    vk::Fence fence;
    device.fencePool.request(&fence, 1);
    device.vkDevice.resetFences(fence, device.vkLoader);
    commandBuffer.end(device.vkLoader);
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    queue.vkQueue.submit(submitInfo, fence, device.vkLoader);
    auto res = device.vkDevice.waitForFences(fence, true, UINT64_MAX, device.vkLoader);
    queue.pool.primary.release(commandBuffer);
    device.fencePool.release(fence);
    return res;
}

bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags vkUsage, VmaMemoryUsage vmaUsage, vk::Buffer &buffer,
                  vk::MemoryPropertyFlags requiredFlags, f32 priority, VmaAllocation &allocation, const Device &device)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.setSize(size).setUsage(vkUsage).setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.priority = priority;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(requiredFlags);
    if (size > MEM_DEDICATTED_ALLOC_MIN) allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    auto res = vmaCreateBuffer(device.allocator, reinterpret_cast<const VkBufferCreateInfo *>(&bufferInfo), &allocInfo,
                               reinterpret_cast<VkBuffer *>(&buffer), &allocation, nullptr);
    if (res == VK_SUCCESS) return true;
    logError("Failed to create buffer: %s", vk::to_string((vk::Result)res).c_str());
    return false;
}

void copyBuffer(Device &device, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(device, device.graphicsQueue);
    vk::BufferCopy copyRegion(0, 0, size);
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion, device.vkLoader);
    endSingleTimeCommands(commandBuffer, device.graphicsQueue, device);
}

void transitionImageLayout(Device &device, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                           u32 mipLevels)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(device, device.graphicsQueue);
    vk::ImageMemoryBarrier barrier{};
    barrier.setOldLayout(oldLayout).setNewLayout(newLayout).setImage(image).setSubresourceRange(
        {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1});
    vk::PipelineStageFlagBits srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlagBits dstStage = vk::PipelineStageFlagBits::eTopOfPipe;

    switch (oldLayout)
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
            throw std::invalid_argument("Unsupported src layout transition");
    }

    switch (newLayout)
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
            throw std::invalid_argument("Unsupported dst layout transition");
    }

    commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier,
                                  device.vkLoader);
    endSingleTimeCommands(commandBuffer, device.graphicsQueue, device);
}

void copyBufferToImage(Device &device, vk::Buffer buffer, vk::Image image, u32 width, u32 height, u32 layerCount,
                       int offsetX, int offsetY)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(device, device.graphicsQueue);
    vk::BufferImageCopy region{};
    region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, layerCount})
        .setImageOffset({offsetX, offsetY, 0})
        .setImageExtent({width, height, 1});

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region, device.vkLoader);
    endSingleTimeCommands(commandBuffer, device.graphicsQueue, device);
}

void copyImageToBuffer(Device &device, vk::Buffer buffer, vk::Image image, u32 width, u32 height, u32 layerCount,
                       int offsetX, int offsetY)
{
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(device, device.graphicsQueue);

    vk::BufferImageCopy region{};
    region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, layerCount})
        .setImageOffset({offsetX, offsetY, 0})
        .setImageExtent({width, height, 1});

    commandBuffer.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, 1, &region, device.vkLoader);
    endSingleTimeCommands(commandBuffer, device.graphicsQueue, device);
}

bool createImageWithInfo(const vk::ImageCreateInfo &imageInfo, vk::Image &image, VmaAllocation &allocation,
                         VmaAllocator &allocator, VmaMemoryUsage vmaUsageFlags, vk::MemoryPropertyFlags vkMemFlags,
                         f32 priority)
{
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsageFlags;
    allocInfo.priority = priority;
    allocInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(vkMemFlags);

    VkResult result = vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo *>(&imageInfo), &allocInfo,
                                     reinterpret_cast<VkImage *>(&image), &allocation, nullptr);
    if (result == VK_SUCCESS) return true;
    logError("Failed to create image #%d", (int)result);
    return false;
}