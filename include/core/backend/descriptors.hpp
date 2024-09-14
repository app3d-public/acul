/***************
 * Abstract wrappers for interacting with Vulkan descriptor sets
 * @author Wusiki Jeronii
 * @date 12.2022
 */
#ifndef APP_BACKEND_DESCRIPTORS
#define APP_BACKEND_DESCRIPTORS

#include "device.hpp"

namespace app
{

    /// @brief Wrapper for creating a descriptor set layout
    class APPLIB_API DescriptorSetLayout
    {
    public:
        /// @brief Descriptor set layout builder
        class APPLIB_API Builder
        {
        public:
            /// @brief Initialize layout builder
            /// @param device Current device
            explicit Builder(Device &device) : _device{device} {}

            /// @brief Add binding for creating a descriptor set layout
            /// @param binding  A binding number
            /// @param descriptorType Descriptor set type. It must be unique
            /// @param stageFlags Pipeline stage flags that have a access to this binding
            /// @param count Count of bindings
            /// @return Self
            Builder &addBinding(u32 binding, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags,
                                u32 count = 1);

            /// @brief Create a descriptor set layout by added bindings
            /// @return Shared pointer to the descriptor set layout wrapper
            std::shared_ptr<DescriptorSetLayout> build() const
            {
                return std::make_shared<DescriptorSetLayout>(_device, _bindings);
            }

        private:
            Device &_device;
            astl::hashmap<u32, vk::DescriptorSetLayoutBinding> _bindings{};
        };

        /// @brief Create a descriptor set layout
        /// @param device Current device
        /// @param bindings Map of descriptor bindings
        DescriptorSetLayout(Device &device, const astl::hashmap<u32, vk::DescriptorSetLayoutBinding> &bindings);

        ~DescriptorSetLayout()
        {
            _device.vkDevice.destroyDescriptorSetLayout(_descriptorSetLayout, nullptr, _device.vkLoader);
        }

        DescriptorSetLayout(const DescriptorSetLayout &) = delete;

        DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

        /// @brief Get the created descriptor set layout
        vk::DescriptorSetLayout getDescriptorSetLayout() const { return _descriptorSetLayout; }

    private:
        Device &_device;
        vk::DescriptorSetLayout _descriptorSetLayout;
        astl::hashmap<u32, vk::DescriptorSetLayoutBinding> _bindings;

        friend class DescriptorWriter;
    };

    /// @brief A wrapper for creating descriptor pools
    class APPLIB_API DescriptorPool
    {
    public:
        /// @brief Descriptor pool builder
        class APPLIB_API Builder
        {
        public:
            /// @brief Initialize pool builder
            /// @param device Current device
            explicit Builder(Device &device) : _device{device} {}

            /// @brief Set max pool size for specified descriptor type
            /// @param descriptorType Descriptor type
            /// @param count Maximum pool size
            /// @return Self
            Builder &addPoolSize(vk::DescriptorType descriptorType, u32 count)
            {
                _poolSizes.push_back({descriptorType, count});
                return *this;
            }

            /// @brief Set flags for the creating pool
            /// @param flags Descriptor pool flags
            /// @return Self
            Builder &setPoolFlags(vk::DescriptorPoolCreateFlags flags)
            {
                _poolFlags = flags;
                return *this;
            }

            /// @brief Set maximum set count for all pool sizes
            /// @param count Maximum set count
            /// @return Self
            Builder &setMaxSets(u32 count)
            {
                _maxSets = count;
                return *this;
            }

            /// @brief Build descriptor pool
            /// @return Shared descriptor pool wrapper
            std::shared_ptr<DescriptorPool> build() const
            {
                return std::make_shared<DescriptorPool>(_device, _maxSets, _poolFlags, _poolSizes);
            }

        private:
            Device &_device;
            astl::vector<vk::DescriptorPoolSize> _poolSizes;
            u32 _maxSets = 1000;
            vk::DescriptorPoolCreateFlags _poolFlags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        };

        /// @brief Create a descriptor pool
        /// @param device Current device
        /// @param maxSets Maximum set count for all pool sizes
        /// @param poolFlags Descriptor pool flags
        /// @param poolSizes Descriptor pool sizes
        DescriptorPool(Device &device, u32 maxSets, vk::DescriptorPoolCreateFlags poolFlags,
                       const astl::vector<vk::DescriptorPoolSize> &poolSizes);

        ~DescriptorPool() { _device.vkDevice.destroyDescriptorPool(_descriptorPool, nullptr, _device.vkLoader); }

        DescriptorPool(const DescriptorPool &) = delete;

        DescriptorPool &operator=(const DescriptorPool &) = delete;

        /// @brief Allocate specified descriptor set
        /// @param descriptorSetLayout Descriptor set layout
        /// @param descriptor Destination descriptor set
        /// @return true if success else false
        bool allocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout, vk::DescriptorSet &descriptor) const;

        /// @brief Free allocate descriptor sets by specified vector of descriptors
        /// @param descriptors Vector of descriptors
        void freeDescriptors(astl::vector<vk::DescriptorSet> &descriptors) const
        {
            _device.vkDevice.freeDescriptorSets(_descriptorPool, static_cast<u32>(descriptors.size()),
                                                descriptors.data(), _device.vkLoader);
        }

        /// @brief Reset descriptor pool
        void resetPool() { _device.vkDevice.resetDescriptorPool(_descriptorPool, {}, _device.vkLoader); }

        /// @brief Get the created descriptor pool
        vk::DescriptorPool descriptorPool() const { return _descriptorPool; }

    private:
        Device &_device;
        vk::DescriptorPool _descriptorPool;

        friend class DescriptorWriter;
    };

    /// @brief A wrapper for writing descriptor set to destinations
    class APPLIB_API DescriptorWriter
    {
    public:
        /// @brief Initialize a descriptor writer
        /// @param setLayout Target descriptor set layout
        /// @param pool Target descriptor pool
        DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool) : _setLayout{setLayout}, _pool{pool} {}

        /// @brief Write buffer to destination descriptor set
        /// @param binding Destination buffer binding
        /// @param bufferInfo BufferInfo structure
        /// @return Self
        DescriptorWriter &writeBuffer(u32 binding, vk::DescriptorBufferInfo *bufferInfo);

        /// @brief Write image to destination descriptor set
        /// @param binding Destination image binding
        /// @param imageInfo ImageInfo structure
        /// @return Self
        DescriptorWriter &writeImage(u32 binding, vk::DescriptorImageInfo *imageInfo);

        /// @brief Allocate descriptor set by applied write bindings
        /// @param set Destination descriptor set
        /// @return true if successful otherwise false
        bool build(vk::DescriptorSet &set)
        {
            if (!set && !_pool.allocateDescriptor(_setLayout.getDescriptorSetLayout(), set)) return false;
            overwrite(set);
            return true;
        }

        /// @brief Update descriptor set
        /// @param set Destroy descriptor set
        void overwrite(const vk::DescriptorSet &set)
        {
            for (auto &write : _writes) write.dstSet = set;
            _pool._device.vkDevice.updateDescriptorSets(_writes.size(), _writes.data(), 0, nullptr,
                                                        _pool._device.vkLoader);
        }

    private:
        DescriptorSetLayout &_setLayout;
        DescriptorPool &_pool;
        astl::vector<vk::WriteDescriptorSet> _writes;
    };
} // namespace app

#endif