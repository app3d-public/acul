#include <acul/gpu/buffer.hpp>
#include <acul/gpu/descriptors.hpp>
#include <acul/gpu/device.hpp>

using namespace acul;
using namespace acul::gpu;

void test_gpu_device(device &);
void test_gpu_buffer(device &);
void test_gpu_utils(device &);
void test_gpu_descriptors(device &);
void test_gpu_pipeline(device &d);
void test_gpu_vector(device &d);

void test_gpu()
{
    task::service_dispatch sd;
    auto *service = acul::alloc<log::log_service>();
    sd.register_service(service);
    service->level = log::level::Trace;

    auto *console = service->add_logger<log::console_logger>("console");
    service->default_logger = console;
    console->set_pattern("%(message)\n");
    assert(console->name() == "console");

    device::create_ctx ctx;
    ctx.set_validation_layers({"VK_LAYER_KHRONOS_validation"})
        .set_extensions({vk::EXTConservativeRasterizationExtensionName})
        .set_opt_extensions({vk::EXTMemoryPriorityExtensionName, vk::EXTPageableDeviceLocalMemoryExtensionName})
        .set_fence_pool_size(8);
    device d;
    init_device("app_test", 1, d, &ctx);
    assert(d.vk_device);
    assert(d.physical_device);
    assert(d.get_device_properties().vendorID != 0);

    test_gpu_device(d);
    test_gpu_buffer(d);
    test_gpu_utils(d);
    test_gpu_descriptors(d);
    test_gpu_pipeline(d);
    test_gpu_vector(d);

    destroy_device(d);
}