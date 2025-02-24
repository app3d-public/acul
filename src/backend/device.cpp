#include <astl/set.hpp>
#include <backend/device.hpp>
#include <core/log.hpp>

#define DEVICE_QUEUE_GRAPHICS 0
#define DEVICE_QUEUE_PRESENT  1
#define DEVICE_QUEUE_COMPUTE  2
#define DEVICE_QUEUE_COUNT    3

astl::vector<const char *> usingExtensitions;

astl::vector<const char *> getSupportedOptExt(vk::PhysicalDevice device,
                                              const astl::hashset<std::string> &allExtensions,
                                              const astl::vector<const char *> &optExtensions)
{
    astl::vector<const char *> supportedExtensions;
    for (const auto &reqExt : optExtensions)
        if (allExtensions.find(reqExt) != allExtensions.end()) supportedExtensions.push_back(reqExt);
    return supportedExtensions;
}

void destroyQueue(DeviceQueue &queue, vk::Device &device, vk::DispatchLoaderDynamic &loader)
{
    logInfo("Destroying command pools");
    device.destroyCommandPool(queue.graphicsQueue.pool.vkPool, nullptr, loader);
    device.destroyCommandPool(queue.computeQueue.pool.vkPool, nullptr, loader);
}

inline bool isFamilyIndicesComplete(std::optional<u32> *dst, bool checkPresent)
{
    bool ret = dst[DEVICE_QUEUE_GRAPHICS].has_value() && dst[DEVICE_QUEUE_COMPUTE].has_value();
    return checkPresent ? ret && dst[DEVICE_QUEUE_PRESENT].has_value() : ret;
}

void Device::findQueueFamilies(std::optional<u32> *dst, vk::PhysicalDevice device)
{
    auto queueFamilies = device.getQueueFamilyProperties(vkLoader);

    int i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) dst[DEVICE_QUEUE_GRAPHICS] = i;
        if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) dst[DEVICE_QUEUE_COMPUTE] = i;
        if (_createCtx->enablePresent)
            if (device.getSurfaceSupportKHR(i, surface, vkLoader)) dst[DEVICE_QUEUE_PRESENT] = i;
        if (isFamilyIndicesComplete(dst, _createCtx->enablePresent)) break;
        i++;
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                                    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData)
{
    switch (messageSeverity)
    {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
            logDebug("%s", pCallbackData->pMessage);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            logInfo("%s", pCallbackData->pMessage);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            logWarn("%s", pCallbackData->pMessage);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            logError("%s", pCallbackData->pMessage);
        default:
            break;
    }
    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT &createInfo)
{
    createInfo = vk::DebugUtilsMessengerCreateInfoEXT();
    createInfo.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback);
}

void Device::setupDebugMessenger()
{
    logDebug("Setting up debug messenger");
    if (!_useValidationLayers) return;
    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);
    _debugMessenger = vkInstance.createDebugUtilsMessengerEXT(createInfo, nullptr, vkLoader);
}

void Device::createInstance(const std::string &appName, u32 version)
{
    logInfo("Creating vk:instance");
    if (!vklib().success()) throw std::runtime_error("Failed to load Vulkan library");
    vkLoader.init(vklib().getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    if (_useValidationLayers && !checkValidationLayerSupport(_createCtx->validationLayers))
        throw std::runtime_error("Validation layers requested, but not available!");

    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName(appName.c_str())
        .setApplicationVersion(version)
        .setPEngineName("No engine")
        .setEngineVersion(1)
        .setApiVersion(vk::ApiVersion12);

    vk::InstanceCreateInfo createInfo({}, &appInfo);

    auto extensions = getRequiredExtensions();
    createInfo.setEnabledExtensionCount(static_cast<u32>(extensions.size()))
        .setPpEnabledExtensionNames(extensions.data());

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (_useValidationLayers)
    {
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.setEnabledLayerCount(static_cast<u32>(_createCtx->validationLayers.size()))
            .setPpEnabledLayerNames(_createCtx->validationLayers.data())
            .pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    }
    vkInstance = vk::createInstance(createInfo, nullptr, vkLoader);
    if (!vkInstance) throw std::runtime_error("Failed to create vk:instance");
    vkLoader.init(vkInstance);
}

void Device::allocateCmdBufPool(const vk::CommandPoolCreateInfo &createInfo, CommandPool &dst, size_t primary,
                                size_t secondary)
{
    dst.vkPool = vkDevice.createCommandPool(createInfo, nullptr, vkLoader);
    dst.primary.allocator.device = &vkDevice;
    dst.primary.allocator.loader = &vkLoader;
    dst.primary.allocator.commandPool = &dst.vkPool;
    dst.primary.allocate(primary);

    dst.secondary.allocator.device = &vkDevice;
    dst.secondary.allocator.loader = &vkLoader;
    dst.secondary.allocator.commandPool = &graphicsQueue.pool.vkPool;
    dst.secondary.allocate(secondary);
}

void Device::allocateCommandPools()
{
    vk::CommandPoolCreateInfo graphicPoolInfo;
    graphicPoolInfo
        .setFlags(vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(graphicsQueue.familyIndex.value());
    allocateCmdBufPool(graphicPoolInfo, graphicsQueue.pool, 5, 10);

    vk::CommandPoolCreateInfo computePoolInfo;
    computePoolInfo
        .setFlags(vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(computeQueue.familyIndex.value());
    allocateCmdBufPool(computePoolInfo, computeQueue.pool, 2, 2);
}

void Device::init(const std::string &appName, u32 version, DeviceCreateCtx *createCtx)
{
    if (!createCtx) throw std::runtime_error("Failed to get create context");
    _createCtx = createCtx;
    createInstance(appName, version);
    setupDebugMessenger();
    if (createCtx->enablePresent)
    {
        hasWindowRequiredInstanceExtensions();
        if (createCtx->createSurface(vkInstance, surface, vkLoader) != vk::Result::eSuccess)
            throw std::runtime_error("Failed to create window surface");
    }
    pickPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    allocateCommandPools();
    fencePool.allocator.device = &vkDevice;
    fencePool.allocator.loader = &vkLoader;
    fencePool.allocate(createCtx->fencePoolSize);
    _createCtx = nullptr;
}

void Device::destroy()
{
    if (!vkDevice) return;
    destroyQueue(*this, vkDevice, vkLoader);
    fencePool.destroy();
    if (allocator) vmaDestroyAllocator(allocator);
    logInfo("Destroying vk:device");
    vkDevice.destroy(nullptr, vkLoader);
    if (_useValidationLayers) vkInstance.destroyDebugUtilsMessengerEXT(_debugMessenger, nullptr, vkLoader);
    logInfo("Destroying vk:instance");
    vkInstance.destroy(nullptr, vkLoader);
}

vk::DynamicLoader &Device::vklib()
{
    static vk::DynamicLoader lib;
    return lib;
}

bool Device::checkValidationLayerSupport(const astl::vector<const char *> &validationLayers)
{
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties(vkLoader);
    return std::all_of(validationLayers.begin(), validationLayers.end(), [&](const char *layerName) {
        return std::any_of(availableLayers.begin(), availableLayers.end(),
                           [&](const VkLayerProperties &layerProperties) {
                               return std::string(layerName) == layerProperties.layerName;
                           });
    });
}

astl::vector<const char *> Device::getRequiredExtensions()
{
    astl::vector<const char *> extensions = _createCtx->getWindowExtensions();
    if (_useValidationLayers) extensions.push_back(vk::EXTDebugUtilsExtensionName);
    return extensions;
}

bool Device::checkDeviceExtensionSupport(const astl::hashset<std::string> &allExtensions) const
{
    for (const auto &extension : _createCtx->extensions)
    {
        auto it = allExtensions.find(extension);
        if (it == allExtensions.end()) return false;
    }
    return true;
}

int Device::getDeviceRating(const astl::vector<const char *> &optExtensions)
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

void Device::pickPhysicalDevice()
{
    logInfo("Searching physical device");
    auto devices = vkInstance.enumeratePhysicalDevices(vkLoader);
    astl::vector<const char *> optExtensions;
    astl::hashset<std::string> extensions;

    if (config.gpuDevice >= 0 && config.gpuDevice < devices.size())
    {
        std::optional<u32> indices[DEVICE_QUEUE_COUNT];
        if (validatePhysicalDevice(devices[config.gpuDevice], extensions, indices))
        {
            physicalDevice = devices[config.gpuDevice];
            graphicsQueue.familyIndex = indices[DEVICE_QUEUE_GRAPHICS];
            presentQueue.familyIndex = indices[DEVICE_QUEUE_PRESENT];
            computeQueue.familyIndex = indices[DEVICE_QUEUE_COMPUTE];
            optExtensions = getSupportedOptExt(physicalDevice, extensions, optExtensions);
            properties.pNext = &depthResolveProperties;
            properties = physicalDevice.getProperties(vkLoader);
        }
        else
            logWarn("User-selected device is not suitable. Searching for another one.");
    }
    else if (config.gpuDevice != -1)
        logWarn("Invalid device index provided. Searching for a suitable device.");

    if (!physicalDevice)
    {
        int maxRating = 0;
        for (const auto &device : devices)
        {
            std::optional<u32> indices[DEVICE_QUEUE_COUNT];
            if (validatePhysicalDevice(device, extensions, indices))
            {
                properties.pNext = &depthResolveProperties;
                properties = device.getProperties2(vkLoader);
                auto optTmp = getSupportedOptExt(device, extensions, optExtensions);
                int rating = getDeviceRating(optTmp);
                if (rating > maxRating)
                {
                    maxRating = rating;
                    optExtensions = optTmp;
                    physicalDevice = device;
                    graphicsQueue.familyIndex = indices[DEVICE_QUEUE_GRAPHICS];
                    presentQueue.familyIndex = indices[DEVICE_QUEUE_PRESENT];
                    computeQueue.familyIndex = indices[DEVICE_QUEUE_COMPUTE];
                }
            }
        }

        if (!physicalDevice) throw std::runtime_error("Failed to find a suitable GPU");
    }
    logInfo("Using: %s", static_cast<char *>(properties.properties.deviceName));
    vk::SampleCountFlags msaa = properties.properties.limits.framebufferColorSampleCounts;
    if (config.msaa > msaa)
    {
        logWarn("MSAAx%d is not supported in current device. Using MSAAx%d",
                static_cast<VkSampleCountFlags>(config.msaa), static_cast<VkSampleCountFlags>(msaa));
        config.msaa = getMaxMSAA(properties);
    }
    memoryProperties = physicalDevice.getMemoryProperties(vkLoader);

    usingExtensitions.insert(usingExtensitions.end(), _createCtx->extensions.begin(), _createCtx->extensions.end());
    usingExtensitions.insert(usingExtensitions.end(), optExtensions.begin(), optExtensions.end());
    assert(graphicsQueue.familyIndex.has_value() && computeQueue.familyIndex.has_value());
}

bool Device::isDeviceSuitable(vk::PhysicalDevice device, const astl::hashset<std::string> &allExtensions,
                              std::optional<u32> *familyIndices)
{
    bool ret =
        checkDeviceExtensionSupport(allExtensions) && isFamilyIndicesComplete(familyIndices, _createCtx->enablePresent);
    if (!_createCtx->enablePresent) return ret;
    bool swapChainAdequate = false;
    SwapchainSupportDetails swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    return ret && familyIndices[DEVICE_QUEUE_PRESENT].has_value() && swapChainAdequate;
}

bool Device::validatePhysicalDevice(vk::PhysicalDevice device, astl::hashset<std::string> &ext,
                                    std::optional<u32> *indices)
{
    auto availableExtensions = device.enumerateDeviceExtensionProperties(nullptr, vkLoader);
    ext.clear();
    for (const auto &extension : availableExtensions) ext.insert(extension.extensionName);
    findQueueFamilies(indices, device);
    return isDeviceSuitable(device, ext, indices);
}

vk::SampleCountFlagBits getMaxUsableSampleCount(vk::PhysicalDeviceProperties properties)
{
    auto counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
    if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
    if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
    if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e8;
    if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e4;
    if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e2;
    return vk::SampleCountFlagBits::e1;
}

void Device::createLogicalDevice()
{
    logInfo("Creating logical device");
    astl::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    assert(graphicsQueue.familyIndex.has_value() && computeQueue.familyIndex.has_value());
    astl::set<u32> uniqueQueueFamilies = {graphicsQueue.familyIndex.value(), computeQueue.familyIndex.value()};
    if (_createCtx->enablePresent) uniqueQueueFamilies.insert(presentQueue.familyIndex.value());
    f32 queuePriority = 1.0f;
    for (u32 queueFamily : uniqueQueueFamilies)
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), queueFamily, 1, &queuePriority);

    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.setSamplerAnisotropy(true)
        .setSampleRateShading(true)
        .setFillModeNonSolid(true)
        .setGeometryShader(true)
        .setShaderTessellationAndGeometryPointSize(true)
        .setWideLines(true);

    vk::PhysicalDeviceShaderDrawParametersFeatures drawFeatures;
    drawFeatures.setShaderDrawParameters(true);

    for (const auto &extension : usingExtensitions) logInfo("Enabling VK extension: %s", extension);

    vk::DeviceCreateInfo createInfo;
    createInfo.setQueueCreateInfoCount(static_cast<u32>(queueCreateInfos.size()))
        .setPQueueCreateInfos(queueCreateInfos.data())
        .setEnabledExtensionCount(static_cast<u32>(usingExtensitions.size()))
        .setPpEnabledExtensionNames(usingExtensitions.data())
        .setPEnabledFeatures(&deviceFeatures)
        .setPNext(&drawFeatures);
    vkDevice = physicalDevice.createDevice(createInfo, nullptr, vkLoader);

    graphicsQueue.vkQueue = vkDevice.getQueue(graphicsQueue.familyIndex.value(), 0, vkLoader);
    computeQueue.vkQueue = vkDevice.getQueue(computeQueue.familyIndex.value(), 0, vkLoader);
    if (_createCtx->enablePresent)
        presentQueue.vkQueue = vkDevice.getQueue(presentQueue.familyIndex.value(), 0, vkLoader);
}

void Device::createAllocator()
{
    VmaVulkanFunctions vmaFunctions = {};
    vmaFunctions.vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(vkInstance.getProcAddr("vkGetInstanceProcAddr", vkLoader));
    vmaFunctions.vkGetDeviceProcAddr =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(vkDevice.getProcAddr("vkGetDeviceProcAddr", vkLoader));

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = vkDevice;
    allocatorInfo.instance = vkInstance;
    allocatorInfo.pVulkanFunctions = &vmaFunctions;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
        throw std::runtime_error("Failed to create memory allocator");
}

void Device::destroyWindowSurface()
{
    if (!surface) return;
    logInfo("Destroying vk:surface");
    vkInstance.destroySurfaceKHR(surface, nullptr, vkLoader);
}

void Device::hasWindowRequiredInstanceExtensions()
{
    astl::set<std::string> available{};
    for (const auto &extension : vk::enumerateInstanceExtensionProperties(nullptr, vkLoader))
        available.insert(extension.extensionName);

    logInfo("Checking for required extensions");
    const auto requiredExtensions = getRequiredExtensions();

    for (const auto &required : requiredExtensions)
        if (available.find(required) == available.end())
            throw std::runtime_error("Missing required window extension: " + std::string(required));
}

SwapchainSupportDetails Device::querySwapChainSupport(vk::PhysicalDevice device)
{
    SwapchainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface, vkLoader);
    details.formats = device.getSurfaceFormatsKHR(surface, vkLoader);
    details.presentModes = device.getSurfacePresentModesKHR(surface, vkLoader);
    return details;
}

vk::Format Device::findSupportedFormat(const astl::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features)
{
    for (vk::Format format : candidates)
    {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format, vkLoader);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("Failed to find supported format");
}

bool Device::supportsLinearFilter(vk::Format format)
{
    vk::FormatProperties props = physicalDevice.getFormatProperties(format, vkLoader);
    if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear) return true;
    return false;
}

size_t Device::getAlignedUBOSize(size_t originalSize) const
{
    size_t minUboAlignment = properties.properties.limits.minUniformBufferOffsetAlignment;
    if (minUboAlignment > 0) originalSize = (originalSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    return originalSize;
}

vk::SampleCountFlagBits getMaxMSAA(const vk::PhysicalDeviceProperties2 &properties)
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