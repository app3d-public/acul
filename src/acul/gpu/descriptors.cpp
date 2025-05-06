#include <acul/gpu/descriptors.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace acul
{
    namespace gpu
    {

        // *************** Descriptor Set Layout *********************

        descriptor_set_layout::descriptor_set_layout(device &device,
                                                     const acul::hashmap<u32, vk::DescriptorSetLayoutBinding> &bindings)
            : _device(device), _bindings{bindings}
        {
            vector<vk::DescriptorSetLayoutBinding> set_layout_bindings{};
            for (auto kv : bindings) set_layout_bindings.push_back(kv.second);
            vk::DescriptorSetLayoutCreateInfo layout_info;
            layout_info.setBindingCount(set_layout_bindings.size()).setPBindings(set_layout_bindings.data());
            if (_device.vk_device.createDescriptorSetLayout(&layout_info, nullptr, &_descriptor_set_layout,
                                                            _device.loader) != vk::Result::eSuccess)
                throw runtime_error("Failed to create descriptor set layout");
        }

        // *************** Descriptor Pool *********************

        bool descriptor_pool::allocate_descriptor(const vk::DescriptorSetLayout descriptor_set_layout,
                                                  vk::DescriptorSet &descriptor) const
        {
            vk::DescriptorSetAllocateInfo allocInfo;
            allocInfo.setDescriptorPool(_descriptor_pool)
                .setDescriptorSetCount(1)
                .setPSetLayouts(&descriptor_set_layout);

            // Might want to create a "descriptor pool manager" class that handles this case, and builds
            // a new pool whenever an old pool fills up. But this is beyond our current scope
            vk::Result result = _device.vk_device.allocateDescriptorSets(&allocInfo, &descriptor, _device.loader);
            if (result != vk::Result::eSuccess)
            {
                LOG_ERROR("Failed to allocate descriptor sets. Result: %s", vk::to_string(result).c_str());
                return false;
            }
            return true;
        }

        // *************** Descriptor Writer *********************

        descriptor_writer &descriptor_writer::write_buffer(u32 binding, vk::DescriptorBufferInfo *buffer_info)
        {
            assert(_set_layout._bindings.count(binding) == 1 && "Layout does not contain specified binding");
            const auto &binding_description = _set_layout._bindings[binding];
            assert(binding_description.descriptorCount == 1 &&
                   "Binding single descriptor info, but binding expects multiple");

            vk::WriteDescriptorSet write;
            write.descriptorType = binding_description.descriptorType;
            write.dstBinding = binding;
            write.pBufferInfo = buffer_info;
            write.descriptorCount = 1;

            _writes.push_back(write);
            return *this;
        }

        descriptor_writer &descriptor_writer::write_image(u32 binding, vk::DescriptorImageInfo *image_info)
        {
            assert(_set_layout._bindings.count(binding) == 1 && "Layout does not contain specified binding");
            const auto &bindingDescription = _set_layout._bindings[binding];
            assert(bindingDescription.descriptorCount == 1 &&
                   "Binding single descriptor info, but binding expects multiple");

            vk::WriteDescriptorSet write;
            write.descriptorType = bindingDescription.descriptorType;
            write.dstBinding = binding;
            write.pImageInfo = image_info;
            write.descriptorCount = 1;

            _writes.push_back(write);
            return *this;
        }
    } // namespace gpu
} // namespace acul