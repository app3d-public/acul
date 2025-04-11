/***************
 * Abstract wrappers for interacting with Vulkan descriptor sets
 * @author Wusiki Jeronii
 * @date 12.2022
 */
#pragma once

#include "device.hpp"

namespace acul
{

    namespace gpu
    {
        /// @brief Wrapper for creating a descriptor set layout
        class APPLIB_API descriptor_set_layout
        {
        public:
            /// @brief Descriptor set layout builder
            class builder
            {
            public:
                /// @brief Initialize layout builder
                /// @param device Current device
                explicit builder() {}

                /// @brief Add binding for creating a descriptor set layout
                /// @param binding  A binding number
                /// @param descriptor_type Descriptor set type. It must be unique
                /// @param stage_flags Pipeline stage flags that have a access to this binding
                /// @param count Count of bindings
                /// @return Self
                builder &add_binding(u32 binding, vk::DescriptorType descriptor_type, vk::ShaderStageFlags stage_flags,
                                     u32 count = 1)
                {
                    assert(_bindings.count(binding) == 0 && "Binding already in use");
                    vk::DescriptorSetLayoutBinding layoutBinding(binding, descriptor_type, count, stage_flags);
                    _bindings[binding] = layoutBinding;
                    return *this;
                }

                /// @brief Create a descriptor set layout by added bindings
                /// @return Shared pointer to the descriptor set layout wrapper
                shared_ptr<descriptor_set_layout> build(device &device) const
                {
                    return acul::make_shared<descriptor_set_layout>(device, _bindings);
                }

            private:
                hashmap<u32, vk::DescriptorSetLayoutBinding> _bindings{};
            };

            /// @brief Create a descriptor set layout
            /// @param device Current device
            /// @param bindings Map of descriptor bindings
            descriptor_set_layout(device &device, const acul::hashmap<u32, vk::DescriptorSetLayoutBinding> &bindings);

            ~descriptor_set_layout()
            {
                _device.vk_device.destroyDescriptorSetLayout(_descriptor_set_layout, nullptr, _device.loader);
            }

            descriptor_set_layout(const descriptor_set_layout &) = delete;

            descriptor_set_layout &operator=(const descriptor_set_layout &) = delete;

            /// @brief Get the created descriptor set layout
            vk::DescriptorSetLayout layout() const { return _descriptor_set_layout; }

        private:
            device &_device;
            vk::DescriptorSetLayout _descriptor_set_layout;
            acul::hashmap<u32, vk::DescriptorSetLayoutBinding> _bindings;

            friend class descriptor_writer;
        };

        /// @brief A wrapper for creating descriptor pools
        class APPLIB_API descriptor_pool
        {
        public:
            /// @brief Descriptor pool builder
            class builder
            {
            public:
                /// @brief Initialize pool builder
                /// @param device Current device
                builder() = default;

                /// @brief Set max pool size for specified descriptor type
                /// @param descriptor_type Descriptor type
                /// @param count Maximum pool size
                /// @return Self
                builder &add_pool_size(vk::DescriptorType descriptor_type, u32 count)
                {
                    _pool_sizes.push_back({descriptor_type, count});
                    return *this;
                }

                /// @brief Set flags for the creating pool
                /// @param flags Descriptor pool flags
                /// @return Self
                builder &set_pool_flags(vk::DescriptorPoolCreateFlags flags)
                {
                    _pool_flags = flags;
                    return *this;
                }

                /// @brief Set maximum set count for all pool sizes
                /// @param count Maximum set count
                /// @return Self
                builder &set_max_sets(u32 count)
                {
                    _max_sets = count;
                    return *this;
                }

                /// @brief Build descriptor pool
                /// @return Shared descriptor pool wrapper
                acul::shared_ptr<descriptor_pool> build(device &device) const
                {
                    return acul::make_shared<descriptor_pool>(device, _max_sets, _pool_flags, _pool_sizes);
                }

            private:
                acul::vector<vk::DescriptorPoolSize> _pool_sizes;
                u32 _max_sets = 1000;
                vk::DescriptorPoolCreateFlags _pool_flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
            };

            /// @brief Create a descriptor pool
            /// @param device Current device
            /// @param max_sets Maximum set count for all pool sizes
            /// @param pool_flags Descriptor pool flags
            /// @param pool_sizes Descriptor pool sizes
            descriptor_pool(device &device, u32 max_sets, vk::DescriptorPoolCreateFlags pool_flags,
                            const acul::vector<vk::DescriptorPoolSize> &pool_sizes)
                : _device(device)
            {
                vk::DescriptorPoolCreateInfo pool_info(pool_flags, max_sets, pool_sizes.size(), pool_sizes.data());
                if (_device.vk_device.createDescriptorPool(&pool_info, nullptr, &_descriptor_pool, _device.loader) !=
                    vk::Result::eSuccess)
                    throw runtime_error("Failed to create descriptor pool");
            }

            ~descriptor_pool() { _device.vk_device.destroyDescriptorPool(_descriptor_pool, nullptr, _device.loader); }

            descriptor_pool(const descriptor_pool &) = delete;

            descriptor_pool &operator=(const descriptor_pool &) = delete;

            /// @brief Allocate specified descriptor set
            /// @param descriptor_set_layout Descriptor set layout
            /// @param descriptor Destination descriptor set
            /// @return true if success else false
            bool allocate_descriptor(const vk::DescriptorSetLayout descriptor_set_layout,
                                     vk::DescriptorSet &descriptor) const;

            /// @brief Free allocate descriptor sets by specified vector of descriptors
            /// @param descriptors Vector of descriptors
            void free_descriptors(acul::vector<vk::DescriptorSet> &descriptors) const
            {
                _device.vk_device.freeDescriptorSets(_descriptor_pool, static_cast<u32>(descriptors.size()),
                                                     descriptors.data(), _device.loader);
            }

            /// @brief Reset descriptor pool
            void reset_pool() { _device.vk_device.resetDescriptorPool(_descriptor_pool, {}, _device.loader); }

            /// @brief Get the created descriptor pool
            vk::DescriptorPool vk_pool() const { return _descriptor_pool; }

        private:
            device &_device;
            vk::DescriptorPool _descriptor_pool;

            friend class descriptor_writer;
        };

        /// @brief A wrapper for writing descriptor set to destinations
        class APPLIB_API descriptor_writer
        {
        public:
            /// @brief Initialize a descriptor writer
            /// @param set_layout Target descriptor set layout
            /// @param pool Target descriptor pool
            descriptor_writer(descriptor_set_layout &set_layout, descriptor_pool &pool)
                : _set_layout{set_layout}, _pool{pool}
            {
            }

            /// @brief Write buffer to destination descriptor set
            /// @param binding Destination buffer binding
            /// @param buffer_info BufferInfo structure
            /// @return Self
            descriptor_writer &write_buffer(u32 binding, vk::DescriptorBufferInfo *buffer_info);

            /// @brief Write image to destination descriptor set
            /// @param binding Destination image binding
            /// @param image_info ImageInfo structure
            /// @return Self
            descriptor_writer &write_image(u32 binding, vk::DescriptorImageInfo *image_info);

            /// @brief Allocate descriptor set by applied write bindings
            /// @param set Destination descriptor set
            /// @return true if successful otherwise false
            bool build(vk::DescriptorSet &set)
            {
                if (!set && !_pool.allocate_descriptor(_set_layout._descriptor_set_layout, set)) return false;
                overwrite(set);
                return true;
            }

            /// @brief Update descriptor set
            /// @param set Destroy descriptor set
            void overwrite(const vk::DescriptorSet &set)
            {
                for (auto &write : _writes) write.dstSet = set;
                _pool._device.vk_device.updateDescriptorSets(_writes.size(), _writes.data(), 0, nullptr,
                                                             _pool._device.loader);
            }

        private:
            descriptor_set_layout &_set_layout;
            descriptor_pool &_pool;
            acul::vector<vk::WriteDescriptorSet> _writes;
        };
    } // namespace gpu
} // namespace acul