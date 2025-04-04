#pragma once
#include "../disposal_queue.hpp"
#include "device.hpp"
#include "utils.hpp"

namespace acul
{
    namespace gpu
    {
        /// @brief Vulkan buffer abstraction wrapper
        class APPLIB_API buffer
        {
        public:
            class mem_cache;

            /// @brief Create a new buffer
            /// @param device Current device
            /// @param instance_size Size of the type to allocate for this buffer
            /// @param instance_count Instances count
            /// @param vk_usage_flags Vulkan usage flags
            /// @param vma_usage_flags VMA usage flags
            /// @param memory_property_flags Memory property flags
            /// @param priority Memory priority
            /// @param min_offset_alignment Minimum offset alignment
            buffer(device &device, vk::DeviceSize instance_size, u32 instance_count,
                   vk::BufferUsageFlags vk_usage_flags, VmaMemoryUsage vma_usage_flags,
                   vk::MemoryPropertyFlags memory_property_flags, f32 priority = 0.5f,
                   vk::DeviceSize min_offset_alignment = 1)
                : _device{device},
                  _instance_count{instance_count},
                  _instance_size{instance_size},
                  _vk_usage_flags{vk_usage_flags},
                  _vma_usage_flags{vma_usage_flags},
                  _memory_property_flags{memory_property_flags},
                  _priority{priority}
            {
                logDebug("Allocating new buffer %p, purpose: %s", this, vk::to_string(_vk_usage_flags).c_str());
                _alignment_size = get_alignment(instance_size, min_offset_alignment);
                _buffer_size = _alignment_size * instance_count;
            }

            static acul::unique_ptr<buffer> UBO(device &device, vk::DeviceSize instance_size, u32 instance_count,
                                                VmaMemoryUsage vma_usage_flags,
                                                vk::MemoryPropertyFlags memory_property_flags)
            {
                return acul::make_unique<buffer>(
                    device, instance_size, instance_count, vk::BufferUsageFlagBits::eUniformBuffer, vma_usage_flags,
                    memory_property_flags, 0.5f, device.properties.properties.limits.minUniformBufferOffsetAlignment);
            }

            ~buffer()
            {
                logDebug("Deallocating buffer %p vk: %p purpose: %s", this, (void *)_vk_buffer,
                         vk::to_string(_vk_usage_flags).c_str());
                unmap();
                vmaDestroyBuffer(_device.allocator, _vk_buffer, _allocation);
            }

            buffer(const buffer &) = delete;
            buffer &operator=(const buffer &) = delete;

            bool allocate()
            {
                assert(_buffer_size > 0);
                return create_buffer(_buffer_size, _vk_usage_flags, _vma_usage_flags, _vk_buffer,
                                     _memory_property_flags, _priority, _allocation, _device);
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
            void write_to_buffer(void *data, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
            {
                assert(_mapped);
                if (size == VK_WHOLE_SIZE)
                    memcpy(_mapped, data, _buffer_size);
                else
                {
                    char *mem_offset = static_cast<char *>(_mapped);
                    mem_offset += offset;
                    memcpy(mem_offset, data, size);
                }
            }

            /**
             * Moves the specified data to the mapped buffer
             * @param data Pointer to the data to move
             * @param size Size of the data to move
             * @param offset Byte offset from beginning of mapped region
             *
             **/
            void move_to_buffer(void *data, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
            {
                assert(_mapped);
                if (size == VK_WHOLE_SIZE)
                    memmove(_mapped, data, _buffer_size);
                else
                {
                    char *mem_offset = static_cast<char *>(_mapped);
                    mem_offset += offset;
                    memmove(mem_offset, data, size);
                }
            }

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
            vk::DescriptorBufferInfo descriptor_info(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
            {
                return vk::DescriptorBufferInfo{_vk_buffer, offset, size};
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
             * Copies "instance_size" bytes of data to the mapped buffer at an offset of index * alignment_size
             *
             * @param data Pointer to the data to copy
             * @param index Used in offset calculation
             *
             */
            void write_to_index(void *data, int index)
            {
                write_to_buffer(data, _instance_size, index * _alignment_size);
            }

            /**
             *  Flush the memory range at index * alignment_size of the buffer to make it visible to the device
             *
             * @param index Used in offset calculation
             *
             */
            vk::Result flush_index(int index) { return flush(_alignment_size, index * _alignment_size); }

            /**
             * Create a buffer info descriptor
             *
             * @param index Specifies the region given by index * alignment_size
             *
             * @return vk::DescriptorBufferInfo for instance at index
             */
            vk::DescriptorBufferInfo descriptor_info_for_index(int index)
            {
                return descriptor_info(_alignment_size, index * _alignment_size);
            }

            /**
             * Invalidate a memory range of the buffer to make it visible to the host
             *
             * @note Only required for non-coherent memory
             *
             * @param index Specifies the region to invalidate: index * alignment_size
             *
             * @return vk::Result of the invalidate call
             */
            vk::Result invalidate_index(int index) { return invalidate(_alignment_size, index * _alignment_size); }

            /// @brief Get VK buffer
            vk::Buffer vk_buffer() const { return _vk_buffer; }

            /// @brief Get raw data of mapped memory range
            void *mapped() const { return _mapped; }

            /// @brief Get instance count
            u32 instance_count() const { return _instance_count; }

            /// @brief Get instance size in bytes
            vk::DeviceSize instance_size() const { return _instance_size; }

            /// @brief Get alignment size in bytes
            vk::DeviceSize getAlignmentSize() const { return _alignment_size; }

            /// @brief Get Vulkan usage flags
            vk::BufferUsageFlags vk_usage_flags() const { return _vk_usage_flags; }

            /// @brief Get VMA usage flags
            VmaMemoryUsage vma_usage_flags() const { return _vma_usage_flags; }

            /// @brief Get memory flags
            vk::MemoryPropertyFlags memory_property_flags() const { return _memory_property_flags; }

            /// @brief Get buffer size
            vk::DeviceSize size() const { return _buffer_size; }

            /// @brief Get priority
            f32 priority() const { return _priority; }

        private:
            static vk::DeviceSize get_alignment(vk::DeviceSize instance_size, vk::DeviceSize min_offset_alignment)
            {
                if (min_offset_alignment > 0)
                    return (instance_size + min_offset_alignment - 1) & ~(min_offset_alignment - 1);
                return instance_size;
            }

            device &_device;
            u32 _instance_count;
            void *_mapped = nullptr;
            vk::Buffer _vk_buffer = VK_NULL_HANDLE;
            VmaAllocation _allocation = VK_NULL_HANDLE;
            vk::DeviceSize _buffer_size;
            vk::DeviceSize _instance_size;
            vk::DeviceSize _alignment_size;
            vk::BufferUsageFlags _vk_usage_flags;
            VmaMemoryUsage _vma_usage_flags;
            vk::MemoryPropertyFlags _memory_property_flags;
            f32 _priority;
        };

        class buffer::mem_cache : public acul::mem_cache
        {
        public:
            explicit mem_cache(buffer *&buffer) : _buffer(buffer) { buffer = nullptr; }

            virtual void free() override { acul::release(_buffer); }

        private:
            buffer *_buffer;
        };
    } // namespace gpu
} // namespace acul