#include <core/descriptors.hpp>
#include <core/log.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace app
{
    DescriptorSetLayout::Builder &DescriptorSetLayout::Builder::addBinding(u32 binding,
                                                                           vk::DescriptorType descriptorType,
                                                                           vk::ShaderStageFlags stageFlags, u32 count)
    {
        assert(_bindings.count(binding) == 0 && "Binding already in use");
        vk::DescriptorSetLayoutBinding layoutBinding(binding, descriptorType, count, stageFlags);
        _bindings[binding] = layoutBinding;
        return *this;
    }

    // *************** Descriptor Set Layout *********************

    DescriptorSetLayout::DescriptorSetLayout(Device &device,
                                             const HashMap<u32, vk::DescriptorSetLayoutBinding> &bindings)
        : _device{device}, _bindings{bindings}
    {
        DArray<vk::DescriptorSetLayoutBinding> setLayoutBindings{};
        for (auto kv : bindings) setLayoutBindings.push_back(kv.second);

        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo;
        descriptorSetLayoutInfo.setBindingCount(setLayoutBindings.size()).setPBindings(setLayoutBindings.data());
        if (_device.vkDevice.createDescriptorSetLayout(&descriptorSetLayoutInfo, nullptr, &_descriptorSetLayout,
                                                       _device.vkLoader) != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create descriptor set layout");
    }

    // *************** Descriptor Pool *********************

    DescriptorPool::DescriptorPool(Device &device, u32 maxSets, vk::DescriptorPoolCreateFlags poolFlags,
                                   const DArray<vk::DescriptorPoolSize> &poolSizes)
        : _device{device}
    {
        vk::DescriptorPoolCreateInfo descriptorPoolInfo(poolFlags, maxSets, poolSizes.size(), poolSizes.data());
        if (_device.vkDevice.createDescriptorPool(&descriptorPoolInfo, nullptr, &_descriptorPool, _device.vkLoader) !=
            vk::Result::eSuccess)
            throw std::runtime_error("Failed to create descriptor pool");
    }

    bool DescriptorPool::allocateDescriptor(const vk::DescriptorSetLayout descriptorSetLayout,
                                            vk::DescriptorSet &descriptor) const
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.setDescriptorPool(_descriptorPool).setDescriptorSetCount(1).setPSetLayouts(&descriptorSetLayout);

        // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
        // a new pool whenever an old pool fills up. But this is beyond our current scope
        vk::Result result = _device.vkDevice.allocateDescriptorSets(&allocInfo, &descriptor, _device.vkLoader);
        if (result != vk::Result::eSuccess)
        {
            logError("Failed to allocate descriptor sets. Result: %s", vk::to_string(result).c_str());
            return false;
        }
        return true;
    }

    // *************** Descriptor Writer *********************

    DescriptorWriter &DescriptorWriter::writeBuffer(u32 binding, vk::DescriptorBufferInfo *bufferInfo)
    {
        assert(_setLayout._bindings.count(binding) == 1 && "Layout does not contain specified binding");
        const auto &bindingDescription = _setLayout._bindings[binding];
        assert(bindingDescription.descriptorCount == 1 &&
               "Binding single descriptor info, but binding expects multiple");

        vk::WriteDescriptorSet write;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.pBufferInfo = bufferInfo;
        write.descriptorCount = 1;

        _writes.push_back(write);
        return *this;
    }

    DescriptorWriter &DescriptorWriter::writeImage(u32 binding, vk::DescriptorImageInfo *imageInfo)
    {
        assert(_setLayout._bindings.count(binding) == 1 && "Layout does not contain specified binding");
        const auto &bindingDescription = _setLayout._bindings[binding];
        assert(bindingDescription.descriptorCount == 1 &&
               "Binding single descriptor info, but binding expects multiple");

        vk::WriteDescriptorSet write;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.pImageInfo = imageInfo;
        write.descriptorCount = 1;

        _writes.push_back(write);
        return *this;
    }
} // namespace app