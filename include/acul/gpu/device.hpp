#pragma once
#include "../meta.hpp"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop
#include <vulkan/vulkan.hpp>
#include "../api.hpp"
#include "../exception.hpp"
#include "../hash/hashset.hpp"
#include "../log.hpp"
#include "../scalars.hpp"
#include "../vector.hpp"
#include "pool.hpp"

#if VK_HEADER_VERSION > 290
namespace vk
{
    using namespace detail;
}
#endif

namespace acul
{
    namespace gpu
    {
        class device_create_ctx
        {
        public:
            acul::vector<const char *> validation_layers;
            acul::vector<const char *> extensions;
            acul::vector<const char *> extensions_opt;
            bool present_enabled;
            size_t fence_pool_size;

            device_create_ctx() : present_enabled(false), fence_pool_size(0) {}

            virtual ~device_create_ctx() = default;

            virtual vk::Result create_surface(vk::Instance &instance, vk::SurfaceKHR &surface,
                                              vk::DispatchLoaderDynamic &loader)
            {
                return vk::Result::eSuccess;
            };

            virtual acul::vector<const char *> get_window_extensions() { return acul::vector<const char *>(); }

            device_create_ctx &set_validation_layers(const acul::vector<const char *> &validation_layers)
            {
                this->validation_layers = validation_layers;
                return *this;
            }

            device_create_ctx &set_extensions(const acul::vector<const char *> &extensions)
            {
                this->extensions = extensions;
                return *this;
            }

            device_create_ctx &set_opt_extensions(const acul::vector<const char *> &extensions)
            {
                this->extensions_opt = extensions;
                return *this;
            }

            device_create_ctx &set_fence_pool_size(size_t fence_pool_size)
            {
                this->fence_pool_size = fence_pool_size;
                return *this;
            }

        protected:
            device_create_ctx(bool present_enabled) : present_enabled(present_enabled), fence_pool_size(0) {}
        };

        struct fence_pool_alloc
        {
            vk::Device *device = nullptr;
            vk::DispatchLoaderDynamic *loader = nullptr;

            void alloc(vk::Fence *pFences, size_t size)
            {
                vk::FenceCreateFlags flags = vk::FenceCreateFlagBits::eSignaled;
                vk::FenceCreateInfo createInfo(flags);
                for (size_t i = 0; i < size; ++i) pFences[i] = device->createFence(createInfo, nullptr, *loader);
            }

            void release(vk::Fence &fence) { device->destroyFence(fence, nullptr, *loader); }
        };

        template <vk::CommandBufferLevel Level>
        struct cmd_buf_alloc
        {
            vk::Device *device;
            vk::CommandPool *command_pool;
            vk::DispatchLoaderDynamic *loader;

            void alloc(vk::CommandBuffer *pBuffers, size_t size)
            {
                vk::CommandBufferAllocateInfo allocInfo(*command_pool, Level, size);
                auto res = device->allocateCommandBuffers(&allocInfo, pBuffers, *loader);
                if (res != vk::Result::eSuccess) throw acul::bad_alloc(size);
            }

            void release(vk::CommandBuffer &buffer)
            {
                // No need 'cos all buffers will be destroying after call VkDestroyCommandPool
            }
        };

        struct command_pool
        {
            vk::CommandPool vk_pool;
            resource_pool<vk::CommandBuffer, cmd_buf_alloc<vk::CommandBufferLevel::ePrimary>> primary;
            resource_pool<vk::CommandBuffer, cmd_buf_alloc<vk::CommandBufferLevel::eSecondary>> secondary;
        };

        struct queue_family_info
        {
            std::optional<u32> family_id;
            vk::Queue vk_queue;
            command_pool pool;
        };

        struct device_queue
        {
            queue_family_info graphics_queue;
            queue_family_info compute_queue;
            queue_family_info present_queue;
        };

        /// @brief Swapchain details
        struct swapchain_support_details
        {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> present_modes;
        };

        namespace sign_block
        {
            enum : u32
            {
                device = 0x2AF818FE
            };
        }

        // Configuration settings for the Device instance.
        struct device_config : acul::meta::block
        {
            vk::SampleCountFlagBits msaa = vk::SampleCountFlagBits::e1; // The number of samples to use for MSAA.
            i8 device = -1;                                             // The index of the GPU device to use.
            // The minimum fraction of sample shading.
            // A value of 1.0 ensures per-sample shading.
            f32 sample_shading = 0.0f;

            virtual u32 signature() const override { return sign_block::device; }
        };

        class APPLIB_API device : public device_queue
        {
        public:
            device_config config;
            vk::DispatchLoaderDynamic loader;
            vk::Instance instance;
            vk::Device vk_device;
            vk::PhysicalDevice physical_device;
            vk::PhysicalDeviceProperties2 properties;
            vk::PhysicalDeviceMemoryProperties memory_properties;
            vk::PhysicalDeviceDepthStencilResolveProperties depth_resolve_properties;
            VmaAllocator allocator;
            vk::SurfaceKHR surface;
            resource_pool<vk::Fence, fence_pool_alloc> fence_pool;

            device() = default;
            device(const device &) = delete;
            device &operator=(const device &) = delete;
            device(device &&) = delete;
            device &operator=(device &&) = delete;

            // Initializes the device with the application name and version.
            void init(const acul::string &app_name, u32 version, device_create_ctx *create_ctx);

            void destroy_window_surface()
            {
                if (!surface) return;
                logInfo("Destroying vk:surface");
                instance.destroySurfaceKHR(surface, nullptr, loader);
            }

            void destroy();

            static vk::DynamicLoader &vklib();

            // Waits for the device to be idle.
            void wait_idle() const { vk_device.waitIdle(loader); }

            /// @brief Find supported color format by current physical device
            /// @param candidates Candidates to search
            /// @param tiling Image tiling type
            /// @param features VK format features
            /// @return Filtered format on success
            vk::Format find_supported_format(const acul::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                             vk::FormatFeatureFlags features);

            swapchain_support_details get_swapchain_support() { return query_swapchain_support(physical_device); }

            /// @brief Check whether the specified format supports linear filtration
            /// @param format VK format to check
            bool supports_linear_filter(vk::Format format)
            {
                vk::FormatProperties props = physical_device.getFormatProperties(format, loader);
                if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear) return true;
                return false;
            }

            /// @brief Get aligned size for UBO buffer by current physical device
            /// @param originalSize Size of original buffer
            size_t get_aligned_UBO_size(size_t original_size) const
            {
                size_t min_UBO_alignment = properties.properties.limits.minUniformBufferOffsetAlignment;
                if (min_UBO_alignment > 0)
                    original_size = (original_size + min_UBO_alignment - 1) & ~(min_UBO_alignment - 1);
                return original_size;
            }

        private:
            vk::DebugUtilsMessengerEXT _debug_messenger;
            device_create_ctx *_create_ctx = nullptr;

            void create_instance(const acul::string &app_name, u32 version);
            void pick_physical_device();
            bool is_device_suitable(vk::PhysicalDevice device, const acul::hashset<acul::string> &extensions,
                                    std::optional<u32> *family_indices);
            bool validate_physical_device(vk::PhysicalDevice device, acul::hashset<acul::string> &ext,
                                          std::optional<u32> *indices);
            void create_logical_device();
            void create_allocator();
            bool check_validation_layers_support(const acul::vector<const char *> &validation_layers);
            void setup_debug_messenger();
            acul::vector<const char *> get_required_extensions();
            bool check_device_extension_support(const acul::hashset<acul::string> &extensions) const;

            // extensions are optional
            int get_device_rating(const acul::vector<const char *> &extensions);
            void find_queue_families(std::optional<u32> *dst, vk::PhysicalDevice device);
            void allocate_cmd_buf_pool(const vk::CommandPoolCreateInfo &createInfo, command_pool &dst, size_t primary,
                                       size_t secondary);
            void allocate_command_pools();
            void has_window_required_instance_extensions();
            swapchain_support_details query_swapchain_support(vk::PhysicalDevice device);
        };

        /// @brief Get maximum MSAA sample count for a physical device
        /// @param properties Physical device properties
        /// @return Maximum MSAA sample count
        APPLIB_API vk::SampleCountFlagBits get_max_MSAA(const vk::PhysicalDeviceProperties2 &properties);

        namespace streams
        {
            inline void write_device_config(acul::bin_stream &stream, acul::meta::block *block)
            {
                device_config *conf = static_cast<device_config *>(block);
                stream.write(conf->msaa).write(conf->device).write(conf->sample_shading);
            }

            inline acul::meta::block *read_device_config(acul::bin_stream &stream)
            {
                device_config *conf = acul::alloc<device_config>();
                stream.read(conf->msaa).read(conf->device).read(conf->sample_shading);
                return conf;
            }

            constexpr acul::meta::stream device = {read_device_config, write_device_config};
        } // namespace streams
    } // namespace gpu
} // namespace acul