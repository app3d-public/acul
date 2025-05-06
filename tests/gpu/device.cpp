#include <acul/gpu/device.hpp>

using namespace acul;
using namespace acul::gpu;

void test_gpu_device(device &d)
{
    bool has_filter = d.supports_linear_filter(vk::Format::eR8G8B8A8Unorm, d.loader);
    auto format =
        d.find_supported_format({vk::Format::eR8G8B8A8Unorm}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlags{});

    // Bin stream
    {
        bin_stream stream;
        meta::block *block = &d.details->config;
        assert(block->signature() == sign_block::Device);

        // Write
        gpu::streams::write_device_config(stream, block);
        stream.pos(0);

        // Read
        meta::block *new_block = gpu::streams::read_device_config(stream);
        auto *conf = static_cast<gpu::device::config *>(new_block);

        assert(conf->msaa == d.details->config.msaa);
        assert(conf->device == d.details->config.device);
        assert(conf->sample_shading == d.details->config.sample_shading);

        acul::release(conf);
    }
}