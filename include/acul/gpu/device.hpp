#pragma once
#include "../hash/hashset.hpp"
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
            vk::Instance instance;
            vk::Device vk_device;
            vk::PhysicalDevice physical_device;
            VmaAllocator allocator;
            vk::SurfaceKHR surface;
            vk::DispatchLoaderDynamic &loader;
            struct device_runtime_data *rd;

            device() : loader(internal::libgpu.dispatch_loader), rd(nullptr) {}

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

            inline swapchain_support_details query_swapchain_support();
#ifndef NDEBUG
            vk::DebugUtilsMessengerEXT debug_messenger;
#endif
        };

        class device_present_ctx
        {
        public:
            virtual ~device_present_ctx() = default;

            virtual vk::Result create_surface(vk::Instance &instance, vk::SurfaceKHR &surface,
                                              vk::DispatchLoaderDynamic &loader) = 0;

            virtual void assign_instance_extensions(const set<string> &ext, acul::vector<const char *> &dst) = 0;
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

        struct device_queue_group
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

        struct device_runtime_data
        {
            device_queue_group queues;
            vk::PhysicalDeviceProperties2 properties2;
            vk::PhysicalDeviceMemoryProperties memory_properties;
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

            vk::PhysicalDeviceProperties &get_device_properties() { return properties2.properties; }

            bool is_opt_extension_supported(const char *extension) { return _extensions.contains(extension); }

        private:
            hashset<const char *> _extensions;

            friend struct device_initializer;
        };

        class physical_device_selector
        {
        public:
            virtual ~physical_device_selector() = default;

            virtual const vk::PhysicalDevice *select(const std::vector<vk::PhysicalDevice> &devices) = 0;
        };

        class device_create_ctx
        {
        public:
            vector<const char *> validation_layers;
            vector<const char *> device_extensions;
            vector<const char *> device_extensions_optional;
            size_t fence_pool_size;
            vk::PhysicalDeviceFeatures device_features;
            void *device_logical_next;
            void *device_physical_next;
            device_runtime_data *runtime_data;
            physical_device_selector *ph_selector;

            // Callback types
            using PFN_assign_instance_extensions = void (*)(device_create_ctx *, const set<string> &,
                                                            vector<const char *> &);
            PFN_assign_instance_extensions assign_instance_extensions;

            device_present_ctx *present_ctx;

            inline device_create_ctx();

            device_create_ctx &set_validation_layers(const vector<const char *> &validation_layers)
            {
                this->validation_layers = validation_layers;
                return *this;
            }

            device_create_ctx &set_device_extensions(const vector<const char *> &extensions)
            {
                device_extensions = extensions;
                return *this;
            }

            device_create_ctx &set_device_extensions_optional(const vector<const char *> &extensions)
            {
                device_extensions_optional = extensions;
                return *this;
            }

            device_create_ctx &set_fence_pool_size(size_t fence_pool_size)
            {
                this->fence_pool_size = fence_pool_size;
                return *this;
            }

            device_create_ctx &set_present_ctx(device_present_ctx *ctx)
            {
                present_ctx = ctx;
                return *this;
            }

            device_create_ctx &set_assign_instance_extensions(PFN_assign_instance_extensions callback)
            {
                assign_instance_extensions = callback;
                return *this;
            }

            device_create_ctx &set_ph_selector(physical_device_selector *selector)
            {
                ph_selector = selector;
                return *this;
            }

            device_create_ctx &set_device_logical_next(void *pNext)
            {
                device_logical_next = pNext;
                return *this;
            }

            device_create_ctx &set_device_physical_next(void *pNext)
            {
                device_physical_next = pNext;
                return *this;
            }

            device_create_ctx &set_runtime_data(device_runtime_data *data)
            {
                this->runtime_data = data;
                return *this;
            }
        };

        APPLIB_API void assign_instance_extensions_default(device_create_ctx *ctx, const set<string> &ext,
                                                           vector<const char *> &dst);

        inline device_create_ctx::device_create_ctx()
            : fence_pool_size(0),
              device_logical_next(nullptr),
              device_physical_next(nullptr),
              runtime_data(nullptr),
              ph_selector(nullptr),
              assign_instance_extensions(assign_instance_extensions_default),
              present_ctx(nullptr)
        {
        }

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

        APPLIB_API void init_device(const acul::string &app_name, u32 version, device &device,
                                    device_create_ctx *create_ctx);

        APPLIB_API void destroy_device(device &device);

        /// @brief Get maximum MSAA sample count for a physical device
        /// @param properties Physical device properties
        /// @return Maximum MSAA sample count
        APPLIB_API vk::SampleCountFlagBits get_max_msaa(const vk::PhysicalDeviceProperties2 &properties);
    } // namespace gpu
} // namespace acul