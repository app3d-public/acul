#include <acul/gpu/device.hpp>
#include <acul/hash/hashset.hpp>
#include <acul/set.hpp>

#define DEVICE_QUEUE_GRAPHICS 0
#define DEVICE_QUEUE_PRESENT  1
#define DEVICE_QUEUE_COMPUTE  2
#define DEVICE_QUEUE_COUNT    3

namespace acul
{
    namespace gpu
    {
        namespace internal
        {
            device_library libgpu;
        }

        struct device_initializer
        {
            vk::Device &device;
            vk::Instance &instance;
            vk::PhysicalDevice &physical_device;
            vk::SurfaceKHR &surface;
#ifndef NDEBUG
            vk::DebugUtilsMessengerEXT &debug_messenger;
#endif
            VmaAllocator &allocator;
            device::create_ctx *create_ctx;
            struct device::details &details;
            vk::DispatchLoaderDynamic &loader;

            device_initializer(struct device &device, device::create_ctx *create_ctx)
                : device(device.vk_device),
                  instance(device.instance),
                  physical_device(device.physical_device),
                  surface(device.surface),
#ifndef NDEBUG
                  debug_messenger(device.debug_messenger),
#endif
                  allocator(device.allocator),
                  create_ctx(create_ctx),
                  details(*device.details),
                  loader(internal::libgpu.dispatch_loader)
            {
            }

            void init(const acul::string &app_name, u32 version);
            void create_instance(const acul::string &app_name, u32 version);
            void pick_physical_device();
            bool is_device_suitable(vk::PhysicalDevice device, const acul::hashset<acul::string> &extensions,
                                    std::optional<u32> *family_indices);
            bool validate_physical_device(vk::PhysicalDevice device, acul::hashset<acul::string> &ext,
                                          std::optional<u32> *indices);
            void create_logical_device();
            void create_allocator();
#ifndef NDEBUG
            void setup_debug_messenger();
#endif
            void find_queue_families(std::optional<u32> *dst, vk::PhysicalDevice device);
            void allocate_cmd_buf_pool(const vk::CommandPoolCreateInfo &createInfo, command_pool &dst, size_t primary,
                                       size_t secondary);
            void allocate_command_pools();
        };

        void init_device(const acul::string &app_name, u32 version, device &device, device::create_ctx *create_ctx,
                         device::config *config)
        {
            if (!create_ctx) throw acul::runtime_error("Failed to get create context");
            device.details = alloc<struct device::details>();
            if (config) device.details->config = *config;
            device_initializer init{device, create_ctx};
            init.init(app_name, version);
        }

        void destroy_device(device &device)
        {
            if (!device.vk_device) return;

            device.details->destroy(device.vk_device, device.loader);
            release(device.details);
            device.details = nullptr;

            if (device.allocator) vmaDestroyAllocator(device.allocator);
            LOG_INFO("Destroying vk:device");
            device.vk_device.destroy(nullptr, device.loader);
#ifndef NDEBUG
            device.instance.destroyDebugUtilsMessengerEXT(device.debug_messenger, nullptr, device.loader);
#endif
            LOG_INFO("Destroying vk:instance");
            device.instance.destroy(nullptr, device.loader);
        }

        vector<const char *> using_extensitions;

        vector<const char *> get_required_extensions(device::create_ctx *create_ctx)
        {
            vector<const char *> extensions = create_ctx->get_window_extensions();
#ifndef NDEBUG
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif
            return extensions;
        }

        void has_window_required_instance_extensions(device::create_ctx *create_ctx, vk::DispatchLoaderDynamic &loader)
        {
            acul::set<acul::string> available{};
            for (const auto &extension : vk::enumerateInstanceExtensionProperties(nullptr, loader))
                available.insert(extension.extensionName.data());
            LOG_INFO("Checking for required extensions");
            const auto requiredExtensions = get_required_extensions(create_ctx);
            for (const auto &required : requiredExtensions)
                if (available.find(required) == available.end())
                    throw acul::runtime_error("Missing required window extension: " + acul::string(required));
        }

        void device_initializer::init(const acul::string &app_name, u32 version)
        {
            create_instance(app_name, version);
#ifndef NDEBUG
            setup_debug_messenger();
#endif
            if (create_ctx->present_enabled)
            {
                has_window_required_instance_extensions(create_ctx, loader);
                if (create_ctx->create_surface(instance, surface, loader) != vk::Result::eSuccess)
                    throw acul::runtime_error("Failed to create window surface");
            }
            pick_physical_device();
            create_logical_device();
            create_allocator();
            allocate_command_pools();
            auto &fence_pool = details.fence_pool;
            fence_pool.allocator.device = &device;
            fence_pool.allocator.loader = &loader;
            fence_pool.allocate(create_ctx->fence_pool_size);
        }

        bool check_validation_layers_support(const vector<const char *> &validation_layers,
                                             vk::DispatchLoaderDynamic &loader)
        {
            auto available_layers = vk::enumerateInstanceLayerProperties(loader);
            return std::all_of(validation_layers.begin(), validation_layers.end(), [&](const char *layerName) {
                return std::any_of(available_layers.begin(), available_layers.end(),
                                   [&](const VkLayerProperties &layerProperties) {
                                       return strncmp(layerName, layerProperties.layerName, 255) == 0;
                                   });
            });
        }

        bool check_device_extension_support(const acul::hashset<acul::string> &allExtensions,
                                            device::create_ctx *create_ctx)
        {
            for (const auto &extension : create_ctx->extensions)
            {
                auto it = allExtensions.find(extension);
                if (it == allExtensions.end()) return false;
            }
            return true;
        }

        int get_device_rating(const vector<const char *> &optExtensions, vk::PhysicalDeviceProperties &properties)
        {
            int rating(0);
            // Properties
            // Type
            if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                rating += 10;
            else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
                rating += 5;

            // MSAA
            if (properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e64)
                rating += 8;
            else if (properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e32)
                rating += 7;
            else if (properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e16)
                rating += 6;
            else if (properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e8)
                rating += 5;
            else if (properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4)
                rating += 4;
            else if (properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e2)
                rating += 2;

            // Textures
            if (properties.limits.maxImageDimension2D > 65536)
                rating += 8;
            else if (properties.limits.maxImageDimension2D > 32768)
                rating += 6;
            else if (properties.limits.maxImageDimension2D > 16384)
                rating += 4;
            else if (properties.limits.maxImageDimension2D > 8192)
                rating += 2;
            else if (properties.limits.maxImageDimension2D > 4096)
                rating += 1;

            // Work threads
            if (properties.limits.maxComputeWorkGroupCount[0] > 65536)
                rating += 8;
            else if (properties.limits.maxComputeWorkGroupCount[0] > 32768)
                rating += 6;
            else if (properties.limits.maxComputeWorkGroupCount[0] > 16384)
                rating += 4;
            else if (properties.limits.maxComputeWorkGroupCount[0] > 8192)
                rating += 2;
            else if (properties.limits.maxComputeWorkGroupCount[0] > 4096)
                rating += 1;

            // Extension support
            rating += optExtensions.size();
            return rating;
        }

        vector<const char *> get_supported_opt_ext(vk::PhysicalDevice device,
                                                   const acul::hashset<acul::string> &allExtensions,
                                                   const vector<const char *> &optExtensions)
        {
            vector<const char *> supportedExtensions;
            for (const auto &reqExt : optExtensions)
                if (allExtensions.find(reqExt) != allExtensions.end()) supportedExtensions.push_back(reqExt);
            return supportedExtensions;
        }

        vk::SampleCountFlagBits get_max_MSAA(const vk::PhysicalDeviceProperties2 &properties)
        {
            if (properties.properties.limits.sampledImageColorSampleCounts & vk::SampleCountFlagBits::e64)
                return vk::SampleCountFlagBits::e64;
            if (properties.properties.limits.sampledImageColorSampleCounts & vk::SampleCountFlagBits::e32)
                return vk::SampleCountFlagBits::e32;
            if (properties.properties.limits.sampledImageColorSampleCounts & vk::SampleCountFlagBits::e16)
                return vk::SampleCountFlagBits::e16;
            if (properties.properties.limits.sampledImageColorSampleCounts & vk::SampleCountFlagBits::e8)
                return vk::SampleCountFlagBits::e8;
            if (properties.properties.limits.sampledImageColorSampleCounts & vk::SampleCountFlagBits::e4)
                return vk::SampleCountFlagBits::e4;
            if (properties.properties.limits.sampledImageColorSampleCounts & vk::SampleCountFlagBits::e2)
                return vk::SampleCountFlagBits::e2;
            return vk::SampleCountFlagBits::e1;
        }

        void device_initializer::pick_physical_device()
        {
            LOG_INFO("Searching physical device");
            auto devices = instance.enumeratePhysicalDevices(loader);
            vector<const char *> optExtensions;
            acul::hashset<acul::string> extensions;
            i8 device_id = details.config.device;
            auto &queues = details.queues;

            if (device_id >= 0 && device_id < devices.size())
            {
                std::optional<u32> indices[DEVICE_QUEUE_COUNT];
                if (validate_physical_device(devices[device_id], extensions, indices))
                {
                    physical_device = devices[device_id];
                    queues.graphics.family_id = indices[DEVICE_QUEUE_GRAPHICS];
                    queues.present.family_id = indices[DEVICE_QUEUE_PRESENT];
                    queues.compute.family_id = indices[DEVICE_QUEUE_COMPUTE];
                    optExtensions = get_supported_opt_ext(physical_device, extensions, optExtensions);
                    details.properties2.pNext = &details.depth_resolve_properties;
                    details.properties2.properties = physical_device.getProperties(loader);
                }
                else
                    LOG_WARN("User-selected device is not suitable. Searching for another one.");
            }
            else if (device_id != -1)
                LOG_WARN("Invalid device index provided. Searching for a suitable device.");

            if (!physical_device)
            {
                int maxRating = 0;
                for (const auto &device : devices)
                {
                    std::optional<u32> indices[DEVICE_QUEUE_COUNT];
                    if (validate_physical_device(device, extensions, indices))
                    {
                        details.properties2.pNext = &details.depth_resolve_properties;
                        details.properties2 = device.getProperties2(loader);
                        auto optTmp = get_supported_opt_ext(device, extensions, optExtensions);
                        int rating = get_device_rating(optTmp, details.properties2.properties);
                        if (rating > maxRating)
                        {
                            maxRating = rating;
                            optExtensions = optTmp;
                            physical_device = device;
                            queues.graphics.family_id = indices[DEVICE_QUEUE_GRAPHICS];
                            queues.present.family_id = indices[DEVICE_QUEUE_PRESENT];
                            queues.compute.family_id = indices[DEVICE_QUEUE_COMPUTE];
                        }
                    }
                }

                if (!physical_device) throw acul::runtime_error("Failed to find a suitable GPU");
            }
            LOG_INFO("Using: %s", static_cast<char *>(details.properties2.properties.deviceName));
            vk::SampleCountFlags msaa = details.properties2.properties.limits.framebufferColorSampleCounts;
            if (details.config.msaa > msaa)
            {
                LOG_WARN("MSAAx%d is not supported in current device. Using MSAAx%d",
                        static_cast<VkSampleCountFlags>(details.config.msaa), static_cast<VkSampleCountFlags>(msaa));
                details.config.msaa = get_max_MSAA(details.properties2);
            }
            details.memory_properties = physical_device.getMemoryProperties(loader);

            using_extensitions.insert(using_extensitions.end(), create_ctx->extensions.begin(),
                                      create_ctx->extensions.end());
            using_extensitions.insert(using_extensitions.end(), optExtensions.begin(), optExtensions.end());
            assert(queues.graphics.family_id.has_value() && queues.compute.family_id.has_value());
        }

        inline bool is_family_indices_complete(std::optional<u32> *dst, bool check_present)
        {
            bool ret = dst[DEVICE_QUEUE_GRAPHICS].has_value() && dst[DEVICE_QUEUE_COMPUTE].has_value();
            return check_present ? ret && dst[DEVICE_QUEUE_PRESENT].has_value() : ret;
        }

        void device_initializer::find_queue_families(std::optional<u32> *dst, vk::PhysicalDevice device)
        {
            auto queueFamilies = device.getQueueFamilyProperties(loader);

            int i = 0;
            for (const auto &queueFamily : queueFamilies)
            {
                if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) dst[DEVICE_QUEUE_GRAPHICS] = i;
                if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) dst[DEVICE_QUEUE_COMPUTE] = i;
                if (create_ctx->present_enabled)
                    if (device.getSurfaceSupportKHR(i, surface, loader)) dst[DEVICE_QUEUE_PRESENT] = i;
                if (is_family_indices_complete(dst, create_ctx->present_enabled)) break;
                i++;
            }
        }

        bool device_initializer::is_device_suitable(vk::PhysicalDevice device,
                                                    const acul::hashset<acul::string> &extensions,
                                                    std::optional<u32> *familyIndices)
        {
            bool ret = check_device_extension_support(extensions, create_ctx) &&
                       is_family_indices_complete(familyIndices, create_ctx->present_enabled);
            if (!create_ctx->present_enabled) return ret;
            bool swapchain_adequate = false;
            swapchain_support_details swapchain_support = query_swapchain_support(device, surface, loader);
            swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
            return ret && familyIndices[DEVICE_QUEUE_PRESENT].has_value() && swapchain_adequate;
        }

        bool device_initializer::validate_physical_device(vk::PhysicalDevice device, acul::hashset<acul::string> &ext,
                                                          std::optional<u32> *indices)
        {
            auto availableExtensions = device.enumerateDeviceExtensionProperties(nullptr, loader);
            ext.clear();
            for (const auto &extension : availableExtensions) ext.insert(extension.extensionName.data());
            find_queue_families(indices, device);
            return is_device_suitable(device, ext, indices);
        }

        vk::SampleCountFlagBits get_max_usable_sample_count(vk::PhysicalDeviceProperties properties)
        {
            auto counts =
                properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
            if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
            if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
            if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
            if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e8;
            if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e4;
            if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e2;
            return vk::SampleCountFlagBits::e1;
        }

        void device_initializer::create_logical_device()
        {
            LOG_INFO("Creating logical device");
            vector<vk::DeviceQueueCreateInfo> queue_create_infos;
            auto &queues = details.queues;
            assert(queues.graphics.family_id.has_value() && queues.compute.family_id.has_value());
            acul::set<u32> uniqueQueueFamilies = {queues.graphics.family_id.value(), queues.compute.family_id.value()};
            if (create_ctx->present_enabled) uniqueQueueFamilies.insert(queues.present.family_id.value());
            f32 queue_priority = 1.0f;
            for (u32 queue_family : uniqueQueueFamilies)
                queue_create_infos.emplace_back(vk::DeviceQueueCreateFlags(), queue_family, 1, &queue_priority);

            vk::PhysicalDeviceFeatures device_features;
            device_features.setSamplerAnisotropy(true)
                .setSampleRateShading(true)
                .setFillModeNonSolid(true)
                .setGeometryShader(true)
                .setShaderTessellationAndGeometryPointSize(true)
                .setWideLines(true);

            vk::PhysicalDeviceShaderDrawParametersFeatures draw_features;
            draw_features.setShaderDrawParameters(true);

            for (const auto &extension : using_extensitions) LOG_INFO("Enabling Vulkan extension: %s", extension);

            vk::DeviceCreateInfo create_info;
            create_info.setQueueCreateInfoCount(static_cast<u32>(queue_create_infos.size()))
                .setPQueueCreateInfos(queue_create_infos.data())
                .setEnabledExtensionCount(static_cast<u32>(using_extensitions.size()))
                .setPpEnabledExtensionNames(using_extensitions.data())
                .setPEnabledFeatures(&device_features)
                .setPNext(&draw_features);
            device = physical_device.createDevice(create_info, nullptr, loader);

            queues.graphics.vk_queue = device.getQueue(queues.graphics.family_id.value(), 0, loader);
            queues.compute.vk_queue = device.getQueue(queues.compute.family_id.value(), 0, loader);
            if (create_ctx->present_enabled)
                queues.present.vk_queue = device.getQueue(queues.present.family_id.value(), 0, loader);
        }

        void device_initializer::create_allocator()
        {
            VmaVulkanFunctions vma_functions = {};
            vma_functions.vkGetInstanceProcAddr =
                reinterpret_cast<PFN_vkGetInstanceProcAddr>(instance.getProcAddr("vkGetInstanceProcAddr", loader));
            vma_functions.vkGetDeviceProcAddr =
                reinterpret_cast<PFN_vkGetDeviceProcAddr>(device.getProcAddr("vkGetDeviceProcAddr", loader));

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = physical_device;
            allocatorInfo.device = device;
            allocatorInfo.instance = instance;
            allocatorInfo.pVulkanFunctions = &vma_functions;
            allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
            if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
                throw acul::runtime_error("Failed to create memory allocator");
        }

#ifndef NDEBUG
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
                       const vk::DebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data)
        {
            switch (severity)
            {
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                    LOG_DEBUG("%s", callback_data->pMessage);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                    LOG_INFO("%s", callback_data->pMessage);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                    LOG_WARN("%s", callback_data->pMessage);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                    LOG_ERROR("%s", callback_data->pMessage);
                default:
                    break;
            }
            return VK_FALSE;
        }

        void populate_debug_messenger_create_info(vk::DebugUtilsMessengerCreateInfoEXT &createInfo)
        {
            createInfo = vk::DebugUtilsMessengerCreateInfoEXT();
            createInfo.messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                     vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                     vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            createInfo.pfnUserCallback = debug_callback;
        }

        void device_initializer::setup_debug_messenger()
        {
            LOG_DEBUG("Setting up debug messenger");
            vk::DebugUtilsMessengerCreateInfoEXT create_info;
            populate_debug_messenger_create_info(create_info);
            debug_messenger = instance.createDebugUtilsMessengerEXT(create_info, nullptr, loader);
        }
#endif

        void device_initializer::create_instance(const acul::string &appName, u32 version)
        {
            LOG_INFO("Creating Vulkan instance");
            if (!internal::libgpu.vklib.success()) throw acul::runtime_error("Failed to load Vulkan library");
            loader.init(internal::libgpu.vklib.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
#ifndef NDEBUG
            if (!check_validation_layers_support(create_ctx->validation_layers, loader))
                throw acul::runtime_error("Validation layers requested, but not available!");
#endif
            vk::ApplicationInfo app_info;
            app_info.setPApplicationName(appName.c_str())
                .setApplicationVersion(version)
                .setPEngineName("No engine")
                .setEngineVersion(1)
                .setApiVersion(vk::ApiVersion12);

            vk::InstanceCreateInfo create_info({}, &app_info);

            auto extensions = get_required_extensions(create_ctx);
            create_info.setEnabledExtensionCount(static_cast<u32>(extensions.size()))
                .setPpEnabledExtensionNames(extensions.data());

#ifndef NDEBUG
            vk::DebugUtilsMessengerCreateInfoEXT debug_create_Info;
            populate_debug_messenger_create_info(debug_create_Info);
            create_info.setEnabledLayerCount(static_cast<u32>(create_ctx->validation_layers.size()))
                .setPpEnabledLayerNames(create_ctx->validation_layers.data())
                .pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debug_create_Info;
#endif
            instance = vk::createInstance(create_info, nullptr, loader);
            if (!instance) throw acul::runtime_error("Failed to create vk:instance");
            loader.init(instance);
        }

        void device_initializer::allocate_cmd_buf_pool(const vk::CommandPoolCreateInfo &create_info, command_pool &dst,
                                                       size_t primary, size_t secondary)
        {
            dst.vk_pool = device.createCommandPool(create_info, nullptr, loader);
            dst.primary.allocator.device = &device;
            dst.primary.allocator.loader = &loader;
            dst.primary.allocator.command_pool = &dst.vk_pool;
            dst.primary.allocate(primary);

            dst.secondary.allocator.device = &device;
            dst.secondary.allocator.loader = &loader;
            dst.secondary.allocator.command_pool = &details.queues.graphics.pool.vk_pool;
            dst.secondary.allocate(secondary);
        }

        void device_initializer::allocate_command_pools()
        {
            auto &queues = details.queues;
            vk::CommandPoolCreateInfo graphics_pool_info;
            graphics_pool_info
                .setFlags(vk::CommandPoolCreateFlagBits::eTransient |
                          vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                .setQueueFamilyIndex(queues.graphics.family_id.value());
            allocate_cmd_buf_pool(graphics_pool_info, queues.graphics.pool, 5, 10);

            vk::CommandPoolCreateInfo compute_pool_info;
            compute_pool_info
                .setFlags(vk::CommandPoolCreateFlagBits::eTransient |
                          vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                .setQueueFamilyIndex(queues.compute.family_id.value());
            allocate_cmd_buf_pool(compute_pool_info, queues.compute.pool, 2, 2);
        }
    } // namespace gpu
} // namespace acul