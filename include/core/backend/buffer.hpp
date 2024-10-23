#pragma once
#include "../disposal_queue.hpp"
#include "device.hpp"
#include "utils.hpp"


/// @brief VK buffer abstraction wrapper
class APPLIB_API Buffer
{
public:
    /// @brief Create a new buffer
    /// @param device Current device
    /// @param instanceSize Size of the type to allocate for this buffer
    /// @param instanceCount Instances count
    /// @param vkUsageFlags Vulkan usage flags
    /// @param vmaUsageFlags VMA usage flags
    /// @param memoryPropertyFlags Memory property flags
    /// @param priority Memory priority
    /// @param minOffsetAlignment Minimum offset alignment
    Buffer(Device &device, vk::DeviceSize instanceSize, u32 instanceCount, vk::BufferUsageFlags vkUsageFlags,
           VmaMemoryUsage vmaUsageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, f32 priority = 0.5f,
           vk::DeviceSize minOffsetAlignment = 1);
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    bool allocateBuffer()
    {
        assert(_bufferSize > 0);
        return createBuffer(_bufferSize, _vkUsageFlags, _vmaUsageFlags, _vkBuffer, _memoryPropertyFlags, _priority,
                            _allocation, _device);
    }

    /// @brief  Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
    /// @return vk::Result of the buffer mapping call
    vk::Result map() { return (vk::Result)vmaMapMemory(_device.allocator, _allocation, &_mapped); }

    /// @brief Unmap a mapped memory range
    /// @note Does not return a result as vkUnmapMemory can't fail
    void unmap()
    {
        if (!_mapped) return;
        vmaUnmapMemory(_device.allocator, _allocation);
        _mapped = nullptr;
    }

    /**
     * Copies the specified data to the mapped buffer. Default value writes whole buffer range
     *
     * @param data Pointer to the data to copy
     * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
     * range.
     * @param offset (Optional) Byte offset from beginning of mapped region
     *
     */
    void writeToBuffer(void *data, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

    /**
     * Moves the specified data to the mapped buffer
     * @param data Pointer to the data to move
     * @param size Size of the data to move
     * @param offset Byte offset from beginning of mapped region
     *
     **/
    void moveToBuffer(void *data, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

    /**
     * Flush a memory range of the buffer to make it visible to the device
     *
     * @note Only required for non-coherent memory
     *
     * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the
     * complete buffer range.
     * @param offset (Optional) Byte offset from beginning
     *
     * @return vk::Result of the flush call
     */
    vk::Result flush(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
    {
        return (vk::Result)vmaFlushAllocation(_device.allocator, _allocation, offset, size);
    }

    /**
     * Create a buffer info descriptor
     *
     * @param size (Optional) Size of the memory range of the descriptor
     * @param offset (Optional) Byte offset from beginning
     *
     * @return vk::DescriptorBufferInfo of specified offset and range
     */
    vk::DescriptorBufferInfo descriptorInfo(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
    {
        return vk::DescriptorBufferInfo{_vkBuffer, offset, size};
    }

    /**
     * Invalidate a memory range of the buffer to make it visible to the host
     *
     * @note Only required for non-coherent memory
     *
     * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
     * the complete buffer range.
     * @param offset (Optional) Byte offset from beginning
     *
     * @return vk::Result of the invalidate call
     */
    vk::Result invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
    {
        return (vk::Result)vmaInvalidateAllocation(_device.allocator, _allocation, offset, size);
    }

    /**
     * Copies "instanceSize" bytes of data to the mapped buffer at an offset of index * alignmentSize
     *
     * @param data Pointer to the data to copy
     * @param index Used in offset calculation
     *
     */
    void writeToIndex(void *data, int index) { writeToBuffer(data, _instanceSize, index * _alignmentSize); }

    /**
     *  Flush the memory range at index * alignmentSize of the buffer to make it visible to the device
     *
     * @param index Used in offset calculation
     *
     */
    vk::Result flushIndex(int index) { return flush(_alignmentSize, index * _alignmentSize); }

    /**
     * Create a buffer info descriptor
     *
     * @param index Specifies the region given by index * alignmentSize
     *
     * @return vk::DescriptorBufferInfo for instance at index
     */
    vk::DescriptorBufferInfo descriptorInfoForIndex(int index)
    {
        return descriptorInfo(_alignmentSize, index * _alignmentSize);
    }

    /**
     * Invalidate a memory range of the buffer to make it visible to the host
     *
     * @note Only required for non-coherent memory
     *
     * @param index Specifies the region to invalidate: index * alignmentSize
     *
     * @return vk::Result of the invalidate call
     */
    vk::Result invalidateIndex(int index) { return invalidate(_alignmentSize, index * _alignmentSize); }

    /// @brief Get VK buffer
    vk::Buffer getBuffer() const { return _vkBuffer; }

    /// @brief Get raw data of mapped memory range
    void *getMappedMemory() const { return _mapped; }

    /// @brief Get instance count
    u32 getInstanceCount() const { return _instanceCount; }

    /// @brief Get instance size in bytes
    vk::DeviceSize getInstanceSize() const { return _instanceSize; }

    /// @brief Get alignment size in bytes
    vk::DeviceSize getAlignmentSize() const { return _instanceSize; }

    /// @brief Get Vulkan usage flags
    vk::BufferUsageFlags getVkUsageFlags() const { return _vkUsageFlags; }

    /// @brief Get VMA usage flags
    VmaMemoryUsage getVmaUsageFlags() const { return _vmaUsageFlags; }

    /// @brief Get memory flags
    vk::MemoryPropertyFlags getMemoryPropertyFlags() const { return _memoryPropertyFlags; }

    /// @brief Get buffer size
    vk::DeviceSize getBufferSize() const { return _bufferSize; }

    /// @brief Get priority
    f32 getPriority() const { return _priority; }

private:
    static vk::DeviceSize getAlignment(vk::DeviceSize instanceSize, vk::DeviceSize minOffsetAlignment)
    {
        if (minOffsetAlignment > 0) return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        return instanceSize;
    }

    Device &_device;
    u32 _instanceCount;
    void *_mapped = nullptr;
    vk::Buffer _vkBuffer = VK_NULL_HANDLE;
    VmaAllocation _allocation = VK_NULL_HANDLE;
    vk::DeviceSize _bufferSize;
    vk::DeviceSize _instanceSize;
    vk::DeviceSize _alignmentSize;
    vk::BufferUsageFlags _vkUsageFlags;
    VmaMemoryUsage _vmaUsageFlags;
    vk::MemoryPropertyFlags _memoryPropertyFlags;
    f32 _priority;
};

class BufferMemCache : public MemCache
{
public:
    explicit BufferMemCache(Buffer *&buffer) : _buffer(buffer) { buffer = nullptr; }

    virtual void free() override { astl::release(_buffer); }

private:
    Buffer *_buffer;
};