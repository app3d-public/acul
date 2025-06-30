#pragma once
#include "../hash/hashset.hpp"
#include "../meta.hpp"
#include "../set.hpp"
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include <vk_mem_alloc.h>
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif
#include <vulkan/vulkan.hpp>
#include "../api.hpp"
#include "../exception/exception.hpp"
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
        namespace internal
        {
            extern APPLIB_API struct device_library
            {
                vk::DynamicLoader vklib;
                vk::DispatchLoaderDynamic dispatch_loader;
            } libgpu;
        } // namespace internal

        /// @brief Swapchain details
        struct swapchain_support_details
        {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> present_modes;
        };

        struct device
        {
            struct details;

            vk::Instance instance;
            vk::Device vk_device;
            vk::PhysicalDevice physical_device;
            VmaAllocator allocator;
            vk::SurfaceKHR surface;
            details *details;
            vk::DispatchLoaderDynamic &loader;

            device() : details(nullptr), loader(internal::libgpu.dispatch_loader) {}

            void destroy_window_surface(vk::DispatchLoaderDynamic &dispatch_loader)
            {
                if (!surface) return;
                LOG_INFO("Destroying vk:surface");
                instance.destroySurfaceKHR(surface, nullptr, dispatch_loader);
            }

            /// @brief Check whether the specified format supports linear filtration
            /// @param format VK format to check
            bool supports_linear_filter(vk::Format format, vk::DispatchLoaderDynamic &dispatch_loader)
            {
                vk::FormatProperties props = physical_device.getFormatProperties(format, dispatch_loader);
                if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear) return true;
                return false;
            }

            /// @brief Find supported color format by current physical device
            /// @param candidates Candidates to search
            /// @param tiling Image tiling type
            /// @param features VK format features
            /// @return Filtered format on success
            vk::Format find_supported_format(const vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                             vk::FormatFeatureFlags features)
            {
                for (vk::Format format : candidates)
                {
                    vk::FormatProperties props = physical_device.getFormatProperties(format, loader);
                    if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
                        return format;
                    else if (tiling == vk::ImageTiling::eOptimal &&
                             (props.optimalTilingFeatures & features) == features)
                        return format;
                }
                throw acul::runtime_error("Failed to find supported format");
            }

            inline vk::PhysicalDeviceProperties &get_device_properties() const;
            inline swapchain_support_details query_swapchain_support();

            struct config;
            struct queue;
            class create_ctx;

#ifndef NDEBUG
            vk::DebugUtilsMessengerEXT debug_messenger;
#endif
        };

        class device::create_ctx
        {
        public:
            vector<const char *> validation_layers;
            vector<const char *> extensions;
            vector<const char *> extensions_opt;
            bool present_enabled;
            size_t fence_pool_size;

            create_ctx() : present_enabled(false), fence_pool_size(0) {}

            virtual ~create_ctx() = default;

            virtual void init_extensions(const acul::set<acul::string> &ext, acul::vector<const char *> &dst) {}

            virtual vk::Result create_surface(vk::Instance &instance, vk::SurfaceKHR &surface,
                                              vk::DispatchLoaderDynamic &loader)
            {
                return vk::Result::eSuccess;
            };

            create_ctx &set_validation_layers(const vector<const char *> &validation_layers)
            {
                this->validation_layers = validation_layers;
                return *this;
            }

            create_ctx &set_extensions(const vector<const char *> &extensions)
            {
                this->extensions = extensions;
                return *this;
            }

            create_ctx &set_opt_extensions(const vector<const char *> &extensions)
            {
                this->extensions_opt = extensions;
                return *this;
            }

            create_ctx &set_fence_pool_size(size_t fence_pool_size)
            {
                this->fence_pool_size = fence_pool_size;
                return *this;
            }

        protected:
            create_ctx(bool present_enabled) : present_enabled(present_enabled), fence_pool_size(0) {}
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

        struct device::queue
        {
            queue_family_info graphics;
            queue_family_info compute;
            queue_family_info present;

            void destroy(vk::Device device, vk::DispatchLoaderDynamic &loader)
            {
                LOG_INFO("Destroying command pools");
                device.destroyCommandPool(graphics.pool.vk_pool, nullptr, loader);
                device.destroyCommandPool(compute.pool.vk_pool, nullptr, loader);
            }
        };

        inline swapchain_support_details query_swapchain_support(vk::PhysicalDevice device, vk::SurfaceKHR surface,
                                                                 vk::DispatchLoaderDynamic &loader)
        {
            swapchain_support_details details;
            details.capabilities = device.getSurfaceCapabilitiesKHR(surface, loader);
            details.formats = device.getSurfaceFormatsKHR(surface, loader);
            details.present_modes = device.getSurfacePresentModesKHR(surface, loader);
            return details;
        }

        inline swapchain_support_details device::query_swapchain_support()
        {
            return gpu::query_swapchain_support(physical_device, surface, loader);
        }

        namespace sign_block
        {
            enum : u32
            {
                Device = 0x2AF818FE
            };
        }

        // Configuration settings for the Device instance.
        struct device::config : acul::meta::block
        {
            vk::SampleCountFlagBits msaa = vk::SampleCountFlagBits::e1; // The number of samples to use for MSAA.
            i8 device = -1;                                             // The index of the GPU device to use.
            // The minimum fraction of sample shading.
            // A value of 1.0 ensures per-sample shading.
            f32 sample_shading = 0.0f;

            virtual u32 signature() const override { return sign_block::Device; }
        };

        struct device::details
        {
            device::config config;
            device::queue queues;
            vk::PhysicalDeviceProperties2 properties2;
            vk::PhysicalDeviceMemoryProperties memory_properties;
            vk::PhysicalDeviceDepthStencilResolveProperties depth_resolve_properties;
            resource_pool<vk::Fence, fence_pool_alloc> fence_pool;

            void destroy(vk::Device &device, vk::DispatchLoaderDynamic &loader)
            {
                LOG_INFO("Destroying device resources");
                queues.destroy(device, loader);
                fence_pool.destroy();
            }

            /// @brief Get aligned size for UBO buffer by current physical device
            /// @param original_size Size of original buffer
            size_t get_aligned_ubo_size(size_t original_size) const
            {
                size_t min_ubo_alignment = properties2.properties.limits.minUniformBufferOffsetAlignment;
                if (min_ubo_alignment > 0)
                    original_size = (original_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
                return original_size;
            }

            bool is_opt_extension_supported(const char *extension) { return _extensions.contains(extension); }

        private:
            hashset<const char *> _extensions;

            friend struct device_initializer;
        };

        inline vk::PhysicalDeviceProperties &device::get_device_properties() const
        {
            return details->properties2.properties;
        }

        APPLIB_API void init_device(const acul::string &app_name, u32 version, device &device,
                                    device::create_ctx *create_ctx, device::config *config = nullptr);

        APPLIB_API void destroy_device(device &device);

        /// @brief Get maximum MSAA sample count for a physical device
        /// @param properties Physical device properties
        /// @return Maximum MSAA sample count
        APPLIB_API vk::SampleCountFlagBits get_max_msaa(const vk::PhysicalDeviceProperties2 &properties);

        namespace streams
        {
            inline void write_device_config(acul::bin_stream &stream, acul::meta::block *block)
            {
                device::config *conf = static_cast<device::config *>(block);
                stream.write(conf->msaa).write(conf->device).write(conf->sample_shading);
            }

            inline acul::meta::block *read_device_config(acul::bin_stream &stream)
            {
                device::config *conf = acul::alloc<device::config>();
                stream.read(conf->msaa).read(conf->device).read(conf->sample_shading);
                return conf;
            }

            constexpr acul::meta::stream device = {read_device_config, write_device_config};
        } // namespace streams
    } // namespace gpu
} // namespace acul