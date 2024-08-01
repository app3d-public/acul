#include <cassert>
#include <core/log.hpp>
#include <core/mem/buffer.hpp>


Buffer::Buffer(Device &device, vk::DeviceSize instanceSize, u32 instanceCount, vk::BufferUsageFlags vkUsageFlags,
               VmaMemoryUsage vmaUsageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, f32 priority,
               vk::DeviceSize minOffsetAlignment)
    : _device{device},
      _instanceCount{instanceCount},
      _instanceSize{instanceSize},
      _vkUsageFlags{vkUsageFlags},
      _vmaUsageFlags{vmaUsageFlags},
      _memoryPropertyFlags{memoryPropertyFlags},
      _priority{priority}
{
    logDebug("Allocating new buffer %p, purpose: %s", this, vk::to_string(_vkUsageFlags).c_str());
    _alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
    _bufferSize = _alignmentSize * instanceCount;
}

Buffer::~Buffer()
{
    logDebug("Deallocating buffer %p vk: %p purpose: %s", this, (void *)_vkBuffer,
             vk::to_string(_vkUsageFlags).c_str());
    unmap();
    vmaDestroyBuffer(_device.allocator, _vkBuffer, _allocation);
}

void Buffer::writeToBuffer(void *data, vk::DeviceSize size, vk::DeviceSize offset)
{
    assert(_mapped);
    if (size == VK_WHOLE_SIZE)
        memcpy(_mapped, data, _bufferSize);
    else
    {
        char *memOffset = static_cast<char *>(_mapped);
        memOffset += offset;
        memcpy(memOffset, data, size);
    }
}

void Buffer::moveToBuffer(void *data, vk::DeviceSize size, vk::DeviceSize offset)
{
    assert(_mapped);
    if (size == VK_WHOLE_SIZE)
        memmove(_mapped, data, _bufferSize);
    else
    {
        char *memOffset = static_cast<char *>(_mapped);
        memOffset += offset;
        memmove(memOffset, data, size);
    }
}