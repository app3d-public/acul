#pragma once
#include "../disposal_queue.hpp"
#include "device.hpp"
#include "utils.hpp"

namespace acul
{
    namespace gpu
    {
        struct buffer
        {
            u32 instance_count = 0;
            void *mapped = nullptr;
            vk::Buffer vk_buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;
            vk::DeviceSize alignment_size = 0;
            vk::DeviceSize buffer_size = 0;
            vk::BufferUsageFlags vk_usage_flags = {};
            VmaMemoryUsage vma_usage_flags = VMA_MEMORY_USAGE_UNKNOWN;
            vk::MemoryPropertyFlags memory_property_flags = {};
            f32 priority = 0.5f;

            struct mem_cache;
        };

        inline void construct_buffer(buffer &buffer, size_t instance_size, size_t min_offset_alignment = 1)
        {
            buffer.alignment_size = get_alignment(instance_size, min_offset_alignment);
            buffer.buffer_size = buffer.alignment_size * buffer.instance_count;
            LOG_DEBUG("Created new buffer %p, purpose: %s", &buffer, vk::to_string(buffer.vk_usage_flags).c_str());
        }

        inline void construct_ubo_buffer(buffer &buffer, size_t instance_size, device &device)
        {
            buffer.alignment_size = device.details->get_aligned_ubo_size(instance_size);
            buffer.buffer_size = buffer.alignment_size * buffer.instance_count;
            LOG_DEBUG("Created new buffer %p, purpose: %s", &buffer, vk::to_string(buffer.vk_usage_flags).c_str());
        }

        inline bool map_buffer(buffer &buffer, device &device)
        {
            return vmaMapMemory(device.allocator, buffer.allocation, &buffer.mapped) == VK_SUCCESS;
        }

        inline void unmap_buffer(buffer &buffer, device &device)
        {
            if (!buffer.mapped) return;
            vmaUnmapMemory(device.allocator, buffer.allocation);
            buffer.mapped = nullptr;
        }

        inline void destroy_buffer(buffer &buffer, device &device)
        {
            LOG_DEBUG("Deallocating buffer %p vk: %p purpose: %s", &buffer, (void *)buffer.vk_buffer,
                     vk::to_string(buffer.vk_usage_flags).c_str());
            unmap_buffer(buffer, device);
            vmaDestroyBuffer(device.allocator, buffer.vk_buffer, buffer.allocation);
            buffer = {};
        }

        inline bool allocate_buffer(buffer &buffer, device &device)
        {
            assert(buffer.buffer_size > 0);
            return create_buffer(buffer.buffer_size, buffer.vk_usage_flags, buffer.vma_usage_flags, buffer.vk_buffer,
                                 buffer.memory_property_flags, buffer.priority, buffer.allocation, device);
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
        inline void write_to_buffer(buffer &buffer, void *data, vk::DeviceSize size = VK_WHOLE_SIZE,
                                    vk::DeviceSize offset = 0)
        {
            assert(buffer.mapped);
            if (size == VK_WHOLE_SIZE)
                memcpy(buffer.mapped, data, buffer.buffer_size);
            else
            {
                char *mem_offset = static_cast<char *>(buffer.mapped);
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
        inline void move_to_buffer(buffer &buffer, void *data, vk::DeviceSize size = VK_WHOLE_SIZE,
                                   vk::DeviceSize offset = 0)
        {
            assert(buffer.mapped);
            if (size == VK_WHOLE_SIZE)
                memmove(buffer.mapped, data, buffer.buffer_size);
            else
            {
                char *mem_offset = static_cast<char *>(buffer.mapped);
                mem_offset += offset;
                memmove(mem_offset, data, size);
            }
        }

        /**
         * Copies "instance_size" bytes of data to the mapped buffer at an offset of index * alignment_size
         *
         * @param data Pointer to the data to copy
         * @param index Used in offset calculation
         *
         */
        inline void write_to_buffer_index(buffer &buffer, size_t instance_size, void *data, int index)
        {
            write_to_buffer(buffer, data, instance_size, index * buffer.alignment_size);
        }

        inline vk::Result flush_buffer(buffer &buffer, device &device, vk::DeviceSize size = VK_WHOLE_SIZE,
                                       vk::DeviceSize offset = 0)
        {
            return (vk::Result)vmaFlushAllocation(device.allocator, buffer.allocation, offset, size);
        }
        /**
         *  Flush the memory range at index * alignment_size of the buffer to make it visible to the device
         *
         * @param index Used in offset calculation
         *
         */
        inline vk::Result flush_buffer_index(buffer &buffer, int index, device &device)
        {
            return flush_buffer(buffer, device, buffer.alignment_size, index * buffer.alignment_size);
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
        inline vk::Result invalidate_buffer(buffer &buffer, device &device, vk::DeviceSize size = VK_WHOLE_SIZE,
                                            vk::DeviceSize offset = 0)
        {
            return (vk::Result)vmaInvalidateAllocation(device.allocator, buffer.allocation, offset, size);
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
        inline vk::Result invalidate_buffer_index(buffer &buffer, int index, device &device)
        {
            return invalidate_buffer(buffer, device, buffer.alignment_size, index * buffer.alignment_size);
        }

        struct buffer::mem_cache : public acul::mem_cache
        {
            explicit mem_cache(buffer &buffer, device &device)
                : acul::mem_cache{[this, &device]() { destroy_buffer(this->_buffer, device); }},
                  _buffer{.mapped = buffer.mapped, .vk_buffer = buffer.vk_buffer, .allocation = buffer.allocation}
            {
                buffer = {};
            }

        private:
            buffer _buffer;
        };
    } // namespace gpu
} // namespace acul