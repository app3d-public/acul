#include <acul/gpu/device.hpp>
#include <acul/set.hpp>

#define DEVICE_QUEUE_GRAPHICS 0
#define DEVICE_QUEUE_PRESENT  1
#define DEVICE_QUEUE_COMPUTE  2
#define DEVICE_QUEUE_COUNT    3

namespace acul
{
    namespace gpu
    {
        acul::vector<const char *> using_extensitions;

        acul::vector<const char *> get_supported_opt_ext(vk::PhysicalDevice device,
                                                         const acul::hashset<acul::string> &allExtensions,
                                                         const acul::vector<const char *> &optExtensions)
        {
            acul::vector<const char *> supportedExtensions;
            for (const auto &reqExt : optExtensions)
                if (allExtensions.find(reqExt) != allExtensions.end()) supportedExtensions.push_back(reqExt);
            return supportedExtensions;
        }

        void destroy_queue(device_queue &queue, vk::Device &device, vk::DispatchLoaderDynamic &loader)
        {
            logInfo("Destroying command pools");
            device.destroyCommandPool(queue.graphics_queue.pool.vk_pool, nullptr, loader);
            device.destroyCommandPool(queue.compute_queue.pool.vk_pool, nullptr, loader);
        }

        inline bool is_family_indices_complete(std::optional<u32> *dst, bool check_present)
        {
            bool ret = dst[DEVICE_QUEUE_GRAPHICS].has_value() && dst[DEVICE_QUEUE_COMPUTE].has_value();
            return check_present ? ret && dst[DEVICE_QUEUE_PRESENT].has_value() : ret;
        }

        void device::find_queue_families(std::optional<u32> *dst, vk::PhysicalDevice device)
        {
            auto queueFamilies = device.getQueueFamilyProperties(loader);

            int i = 0;
            for (const auto &queueFamily : queueFamilies)
            {
                if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) dst[DEVICE_QUEUE_GRAPHICS] = i;
                if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) dst[DEVICE_QUEUE_COMPUTE] = i;
                if (_create_ctx->present_enabled)
                    if (device.getSurfaceSupportKHR(i, surface, loader)) dst[DEVICE_QUEUE_PRESENT] = i;
                if (is_family_indices_complete(dst, _create_ctx->present_enabled)) break;
                i++;
            }
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL
        debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
                       const vk::DebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data)
        {
            switch (severity)
            {
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                    logDebug("%s", callback_data->pMessage);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                    logInfo("%s", callback_data->pMessage);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                    logWarn("%s", callback_data->pMessage);
                    break;
                case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                    logError("%s", callback_data->pMessage);
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
            createInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debug_callback);
        }

        void device::setup_debug_messenger()
        {
            logDebug("Setting up debug messenger");
            vk::DebugUtilsMessengerCreateInfoEXT create_info;
            populate_debug_messenger_create_info(create_info);
            _debug_messenger = instance.createDebugUtilsMessengerEXT(create_info, nullptr, loader);
        }

        void device::create_instance(const acul::string &appName, u32 version)
        {
            logInfo("Creating Vulkan instance");
            if (!vklib().success()) throw acul::runtime_error("Failed to load Vulkan library");
            loader.init(vklib().getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
#ifndef NDEBUG
            if (!check_validation_layers_support(_create_ctx->validation_layers))
                throw acul::runtime_error("Validation layers requested, but not available!");
#endif
            vk::ApplicationInfo app_info;
            app_info.setPApplicationName(appName.c_str())
                .setApplicationVersion(version)
                .setPEngineName("No engine")
                .setEngineVersion(1)
                .setApiVersion(vk::ApiVersion12);

            vk::InstanceCreateInfo create_info({}, &app_info);

            auto extensions = get_required_extensions();
            create_info.setEnabledExtensionCount(static_cast<u32>(extensions.size()))
                .setPpEnabledExtensionNames(extensions.data());

#ifndef NDEBUG
            vk::DebugUtilsMessengerCreateInfoEXT debug_create_Info;
            populate_debug_messenger_create_info(debug_create_Info);
            create_info.setEnabledLayerCount(static_cast<u32>(_create_ctx->validation_layers.size()))
                .setPpEnabledLayerNames(_create_ctx->validation_layers.data())
                .pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debug_create_Info;
#endif
            instance = vk::createInstance(create_info, nullptr, loader);
            if (!instance) throw acul::runtime_error("Failed to create vk:instance");
            loader.init(instance);
        }

        void device::allocate_cmd_buf_pool(const vk::CommandPoolCreateInfo &create_info, command_pool &dst,
                                           size_t primary, size_t secondary)
        {
            dst.vk_pool = vk_device.createCommandPool(create_info, nullptr, loader);
            dst.primary.allocator.device = &vk_device;
            dst.primary.allocator.loader = &loader;
            dst.primary.allocator.command_pool = &dst.vk_pool;
            dst.primary.allocate(primary);

            dst.secondary.allocator.device = &vk_device;
            dst.secondary.allocator.loader = &loader;
            dst.secondary.allocator.command_pool = &graphics_queue.pool.vk_pool;
            dst.secondary.allocate(secondary);
        }

        void device::allocate_command_pools()
        {
            vk::CommandPoolCreateInfo graphics_pool_info;
            graphics_pool_info
                .setFlags(vk::CommandPoolCreateFlagBits::eTransient |
                          vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                .setQueueFamilyIndex(graphics_queue.family_id.value());
            allocate_cmd_buf_pool(graphics_pool_info, graphics_queue.pool, 5, 10);

            vk::CommandPoolCreateInfo compute_pool_info;
            compute_pool_info
                .setFlags(vk::CommandPoolCreateFlagBits::eTransient |
                          vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
                .setQueueFamilyIndex(compute_queue.family_id.value());
            allocate_cmd_buf_pool(compute_pool_info, compute_queue.pool, 2, 2);
        }

        void device::init(const acul::string &app_name, u32 version, device_create_ctx *create_ctx)
        {
            if (!create_ctx) throw acul::runtime_error("Failed to get create context");
            _create_ctx = create_ctx;
            create_instance(app_name, version);
#ifndef NDEBUG
            setup_debug_messenger();
#endif
            if (create_ctx->present_enabled)
            {
                has_window_required_instance_extensions();
                if (create_ctx->create_surface(instance, surface, loader) != vk::Result::eSuccess)
                    throw acul::runtime_error("Failed to create window surface");
            }
            pick_physical_device();
            create_logical_device();
            create_allocator();
            allocate_command_pools();
            fence_pool.allocator.device = &vk_device;
            fence_pool.allocator.loader = &loader;
            fence_pool.allocate(create_ctx->fence_pool_size);
            create_ctx = nullptr;
        }

        void device::destroy()
        {
            if (!vk_device) return;
            destroy_queue(*this, vk_device, loader);
            fence_pool.destroy();
            if (allocator) vmaDestroyAllocator(allocator);
            logInfo("Destroying vk:device");
            vk_device.destroy(nullptr, loader);
#ifndef NDEBUG
            instance.destroyDebugUtilsMessengerEXT(_debug_messenger, nullptr, loader);
#endif
            logInfo("Destroying vk:instance");
            instance.destroy(nullptr, loader);
        }

        vk::DynamicLoader &device::vklib()
        {
            static vk::DynamicLoader lib;
            return lib;
        }

        bool device::check_validation_layers_support(const acul::vector<const char *> &validation_layers)
        {
            std::vector<vk::LayerProperties> available_layers = vk::enumerateInstanceLayerProperties(loader);
            return std::all_of(validation_layers.begin(), validation_layers.end(), [&](const char *layerName) {
                return std::any_of(available_layers.begin(), available_layers.end(),
                                   [&](const VkLayerProperties &layerProperties) {
                                       return strncmp(layerName, layerProperties.layerName, 255) == 0;
                                   });
            });
        }

        acul::vector<const char *> device::get_required_extensions()
        {
            acul::vector<const char *> extensions = _create_ctx->get_window_extensions();
#ifndef NDEBUG
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif
            return extensions;
        }

        bool device::check_device_extension_support(const acul::hashset<acul::string> &allExtensions) const
        {
            for (const auto &extension : _create_ctx->extensions)
            {
                auto it = allExtensions.find(extension);
                if (it == allExtensions.end()) return false;
            }
            return true;
        }

        int device::get_device_rating(const acul::vector<const char *> &optExtensions)
        {
            int rating(0);
            // Properties
            // Type
            if (properties.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                rating += 10;
            else if (properties.properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
                rating += 5;

            // MSAA
            if (properties.properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e64)
                rating += 8;
            else if (properties.properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e32)
                rating += 7;
            else if (properties.properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e16)
                rating += 6;
            else if (properties.properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e8)
                rating += 5;
            else if (properties.properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4)
                rating += 4;
            else if (properties.properties.limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e2)
                rating += 2;

            // Textures
            if (properties.properties.limits.maxImageDimension2D > 65536)
                rating += 8;
            else if (properties.properties.limits.maxImageDimension2D > 32768)
                rating += 6;
            else if (properties.properties.limits.maxImageDimension2D > 16384)
                rating += 4;
            else if (properties.properties.limits.maxImageDimension2D > 8192)
                rating += 2;
            else if (properties.properties.limits.maxImageDimension2D > 4096)
                rating += 1;

            // Work threads
            if (properties.properties.limits.maxComputeWorkGroupCount[0] > 65536)
                rating += 8;
            else if (properties.properties.limits.maxComputeWorkGroupCount[0] > 32768)
                rating += 6;
            else if (properties.properties.limits.maxComputeWorkGroupCount[0] > 16384)
                rating += 4;
            else if (properties.properties.limits.maxComputeWorkGroupCount[0] > 8192)
                rating += 2;
            else if (properties.properties.limits.maxComputeWorkGroupCount[0] > 4096)
                rating += 1;

            // Extension support
            rating += optExtensions.size();
            return rating;
        }

        void device::pick_physical_device()
        {
            logInfo("Searching physical device");
            auto devices = instance.enumeratePhysicalDevices(loader);
            acul::vector<const char *> optExtensions;
            acul::hashset<acul::string> extensions;

            if (config.device >= 0 && config.device < devices.size())
            {
                std::optional<u32> indices[DEVICE_QUEUE_COUNT];
                if (validate_physical_device(devices[config.device], extensions, indices))
                {
                    physical_device = devices[config.device];
                    graphics_queue.family_id = indices[DEVICE_QUEUE_GRAPHICS];
                    present_queue.family_id = indices[DEVICE_QUEUE_PRESENT];
                    compute_queue.family_id = indices[DEVICE_QUEUE_COMPUTE];
                    optExtensions = get_supported_opt_ext(physical_device, extensions, optExtensions);
                    properties.pNext = &depth_resolve_properties;
                    properties = physical_device.getProperties(loader);
                }
                else
                    logWarn("User-selected device is not suitable. Searching for another one.");
            }
            else if (config.device != -1)
                logWarn("Invalid device index provided. Searching for a suitable device.");

            if (!physical_device)
            {
                int maxRating = 0;
                for (const auto &device : devices)
                {
                    std::optional<u32> indices[DEVICE_QUEUE_COUNT];
                    if (validate_physical_device(device, extensions, indices))
                    {
                        properties.pNext = &depth_resolve_properties;
                        properties = device.getProperties2(loader);
                        auto optTmp = get_supported_opt_ext(device, extensions, optExtensions);
                        int rating = get_device_rating(optTmp);
                        if (rating > maxRating)
                        {
                            maxRating = rating;
                            optExtensions = optTmp;
                            physical_device = device;
                            graphics_queue.family_id = indices[DEVICE_QUEUE_GRAPHICS];
                            present_queue.family_id = indices[DEVICE_QUEUE_PRESENT];
                            compute_queue.family_id = indices[DEVICE_QUEUE_COMPUTE];
                        }
                    }
                }

                if (!physical_device) throw acul::runtime_error("Failed to find a suitable GPU");
            }
            logInfo("Using: %s", static_cast<char *>(properties.properties.deviceName));
            vk::SampleCountFlags msaa = properties.properties.limits.framebufferColorSampleCounts;
            if (config.msaa > msaa)
            {
                logWarn("MSAAx%d is not supported in current device. Using MSAAx%d",
                        static_cast<VkSampleCountFlags>(config.msaa), static_cast<VkSampleCountFlags>(msaa));
                config.msaa = get_max_MSAA(properties);
            }
            memory_properties = physical_device.getMemoryProperties(loader);

            using_extensitions.insert(using_extensitions.end(), _create_ctx->extensions.begin(),
                                      _create_ctx->extensions.end());
            using_extensitions.insert(using_extensitions.end(), optExtensions.begin(), optExtensions.end());
            assert(graphics_queue.family_id.has_value() && compute_queue.family_id.has_value());
        }

        bool device::is_device_suitable(vk::PhysicalDevice device, const acul::hashset<acul::string> &extensions,
                                        std::optional<u32> *familyIndices)
        {
            bool ret = check_device_extension_support(extensions) &&
                       is_family_indices_complete(familyIndices, _create_ctx->present_enabled);
            if (!_create_ctx->present_enabled) return ret;
            bool swapchain_adequate = false;
            swapchain_support_details swapchain_support = query_swapchain_support(device);
            swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
            return ret && familyIndices[DEVICE_QUEUE_PRESENT].has_value() && swapchain_adequate;
        }

        bool device::validate_physical_device(vk::PhysicalDevice device, acul::hashset<acul::string> &ext,
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

        void device::create_logical_device()
        {
            logInfo("Creating logical device");
            acul::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
            assert(graphics_queue.family_id.has_value() && compute_queue.family_id.has_value());
            acul::set<u32> uniqueQueueFamilies = {graphics_queue.family_id.value(), compute_queue.family_id.value()};
            if (_create_ctx->present_enabled) uniqueQueueFamilies.insert(present_queue.family_id.value());
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

            for (const auto &extension : using_extensitions) logInfo("Enabling Vulkan extension: %s", extension);

            vk::DeviceCreateInfo create_info;
            create_info.setQueueCreateInfoCount(static_cast<u32>(queue_create_infos.size()))
                .setPQueueCreateInfos(queue_create_infos.data())
                .setEnabledExtensionCount(static_cast<u32>(using_extensitions.size()))
                .setPpEnabledExtensionNames(using_extensitions.data())
                .setPEnabledFeatures(&device_features)
                .setPNext(&draw_features);
            vk_device = physical_device.createDevice(create_info, nullptr, loader);

            graphics_queue.vk_queue = vk_device.getQueue(graphics_queue.family_id.value(), 0, loader);
            compute_queue.vk_queue = vk_device.getQueue(compute_queue.family_id.value(), 0, loader);
            if (_create_ctx->present_enabled)
                present_queue.vk_queue = vk_device.getQueue(present_queue.family_id.value(), 0, loader);
        }

        void device::create_allocator()
        {
            VmaVulkanFunctions vma_functions = {};
            vma_functions.vkGetInstanceProcAddr =
                reinterpret_cast<PFN_vkGetInstanceProcAddr>(instance.getProcAddr("vkGetInstanceProcAddr", loader));
            vma_functions.vkGetDeviceProcAddr =
                reinterpret_cast<PFN_vkGetDeviceProcAddr>(vk_device.getProcAddr("vkGetDeviceProcAddr", loader));

            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = physical_device;
            allocatorInfo.device = vk_device;
            allocatorInfo.instance = instance;
            allocatorInfo.pVulkanFunctions = &vma_functions;
            allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
            if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
                throw acul::runtime_error("Failed to create memory allocator");
        }

        void device::has_window_required_instance_extensions()
        {
            acul::set<acul::string> available{};
            for (const auto &extension : vk::enumerateInstanceExtensionProperties(nullptr, loader))
                available.insert(extension.extensionName.data());
            logInfo("Checking for required extensions");
            const auto requiredExtensions = get_required_extensions();
            for (const auto &required : requiredExtensions)
                if (available.find(required) == available.end())
                    throw acul::runtime_error("Missing required window extension: " + acul::string(required));
        }

        swapchain_support_details device::query_swapchain_support(vk::PhysicalDevice device)
        {
            swapchain_support_details details;
            details.capabilities = device.getSurfaceCapabilitiesKHR(surface, loader);
            details.formats = device.getSurfaceFormatsKHR(surface, loader);
            details.present_modes = device.getSurfacePresentModesKHR(surface, loader);
            return details;
        }

        vk::Format device::find_supported_format(const acul::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                                 vk::FormatFeatureFlags features)
        {
            for (vk::Format format : candidates)
            {
                vk::FormatProperties props = physical_device.getFormatProperties(format, loader);
                if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
                    return format;
                else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
                    return format;
            }
            throw acul::runtime_error("Failed to find supported format");
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
    } // namespace gpu
} // namespace acul