#include <acul/gpu/buffer.hpp>
#include <acul/gpu/descriptors.hpp>

using namespace acul;
using namespace acul::gpu;

void create_test_image(device &d, vk::Image &image)
{
    vk::ImageCreateInfo image_info_ci{};
    image_info_ci.imageType = vk::ImageType::e2D;
    image_info_ci.extent = vk::Extent3D{1, 1, 1};
    image_info_ci.mipLevels = 1;
    image_info_ci.arrayLayers = 1;
    image_info_ci.format = vk::Format::eR8G8B8A8Unorm;
    image_info_ci.tiling = vk::ImageTiling::eOptimal;
    image_info_ci.initialLayout = vk::ImageLayout::eUndefined;
    image_info_ci.usage = vk::ImageUsageFlagBits::eSampled;
    image_info_ci.samples = vk::SampleCountFlagBits::e1;
    image_info_ci.sharingMode = vk::SharingMode::eExclusive;

    image = d.vk_device.createImage(image_info_ci, nullptr, d.loader);
}

void create_test_image_view(device &d, vk::Image &image, vk::ImageView &image_view)
{
    vk::ImageViewCreateInfo view_info{};
    view_info.image = image;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = vk::Format::eR8G8B8A8Unorm;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    image_view = d.vk_device.createImageView(view_info, nullptr, d.loader);
}

void create_test_image_memory(device &d, vk::Image &image, vk::DeviceMemory &memory)
{
    // === Allocate memory (dummy memory, size is queried from image) ===
    vk::MemoryRequirements mem_req = d.vk_device.getImageMemoryRequirements(image, d.loader);
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = 0; // assume type 0 is host visible (for test only)

    vk::DeviceMemory image_memory = d.vk_device.allocateMemory(alloc_info, nullptr, d.loader);
    d.vk_device.bindImageMemory(image, image_memory, 0, d.loader);

    d.vk_device.freeMemory(image_memory, nullptr, d.loader);
}

void create_test_image_sampler(device &d, vk::Sampler &sampler)
{
    // === Create sampler ===
    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter = vk::Filter::eNearest;
    sampler_info.minFilter = vk::Filter::eNearest;
    sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;

    sampler = d.vk_device.createSampler(sampler_info, nullptr, d.loader);
}

void test_gpu_descriptors(device &d)
{
    buffer b;
    b.instance_count = 1;
    b.vk_usage_flags = vk::BufferUsageFlagBits::eUniformBuffer;
    b.vma_usage_flags = VMA_MEMORY_USAGE_CPU_ONLY;
    b.memory_property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    b.priority = 0.0f;
    construct_buffer(b, sizeof(int));
    assert(allocate_buffer(b, d));

    assert(map_buffer(b, d));
    int value = 123;
    write_to_buffer(b, &value);
    flush_buffer(b, d);

    // === layout ===
    auto layout = descriptor_set_layout::builder()
                      .add_binding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
                      .add_binding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                      .build(d);
    assert(layout);
    assert(layout->layout());

    // === descriptor pool ===
    auto pool = descriptor_pool::builder()
                    .add_pool_size(vk::DescriptorType::eUniformBuffer, 1)
                    .add_pool_size(vk::DescriptorType::eCombinedImageSampler, 1)
                    .set_max_sets(2)
                    .set_pool_flags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |
                                    vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
                    .build(d);
    assert(pool);
    assert(pool->vk_pool());

    // === Descriptor write ===
    vk::DescriptorBufferInfo buffer_info;
    buffer_info.buffer = b.vk_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(int);

    vk::DescriptorSet descriptor_set;
    descriptor_writer writer(*layout, *pool);
    writer.write_buffer(0, &buffer_info);
    assert(writer.build(descriptor_set));
    assert(descriptor_set);

    // Image
    vk::Image image;
    vk::DeviceMemory image_memory;
    vk::ImageView image_view;
    vk::Sampler sampler;

    create_test_image(d, image);
    create_test_image_memory(d, image, image_memory);
    create_test_image_view(d, image, image_view);
    create_test_image_sampler(d, sampler);

    vk::DescriptorImageInfo image_info{};
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_info.imageView = image_view;
    image_info.sampler = sampler;

    vk::DescriptorSet image_set;
    descriptor_writer image_writer(*layout, *pool);
    image_writer.write_image(1, &image_info);
    assert(image_writer.build(image_set));
    assert(image_set);

    // === Clean ===
    d.vk_device.destroySampler(sampler, nullptr, d.loader);
    d.vk_device.destroyImageView(image_view, nullptr, d.loader);
    d.vk_device.freeMemory(image_memory, nullptr, d.loader);
    d.vk_device.destroyImage(image, nullptr, d.loader);

    vector<vk::DescriptorSet> sets{descriptor_set};
    pool->free_descriptors(sets);
    pool->reset_pool();
    destroy_buffer(b, d);
}