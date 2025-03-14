#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop
#include <vulkan/vulkan.hpp>
#include "../acul/hashset.hpp"
#include "../acul/scalars.hpp"
#include "../acul/vector.hpp"
#include "../core/api.hpp"
#include "pool.hpp"

#if VK_HEADER_VERSION > 290
namespace vk
{
    using namespace detail;
}
#endif

class DeviceCreateCtx
{
public:
    acul::vector<const char *> validationLayers;
    acul::vector<const char *> extensions;
    acul::vector<const char *> optExtensions;
    bool enablePresent;
    size_t fencePoolSize;

    DeviceCreateCtx() : enablePresent(false), fencePoolSize(0) {}

    virtual ~DeviceCreateCtx() = default;

    virtual vk::Result createSurface(vk::Instance &instance, vk::SurfaceKHR &surface, vk::DispatchLoaderDynamic &loader)
    {
        return vk::Result::eSuccess;
    };

    virtual acul::vector<const char *> getWindowExtensions() { return acul::vector<const char *>(); }

    DeviceCreateCtx &setValidationLayers(const acul::vector<const char *> &validationLayers)
    {
        this->validationLayers = validationLayers;
        return *this;
    }

    DeviceCreateCtx &setExtensions(const acul::vector<const char *> &extensions)
    {
        this->extensions = extensions;
        return *this;
    }

    DeviceCreateCtx &setOptExtensions(const acul::vector<const char *> &extensions)
    {
        this->optExtensions = extensions;
        return *this;
    }

    DeviceCreateCtx &setFencePoolSize(size_t fencePoolSize)
    {
        this->fencePoolSize = fencePoolSize;
        return *this;
    }

protected:
    DeviceCreateCtx(bool enablePresent) : enablePresent(enablePresent), fencePoolSize(0) {}
};

struct FencePoolAlloc
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
struct CmdBufAlloc
{
    vk::Device *device;
    vk::CommandPool *commandPool;
    vk::DispatchLoaderDynamic *loader;

    void alloc(vk::CommandBuffer *pBuffers, size_t size)
    {
        vk::CommandBufferAllocateInfo allocInfo(*commandPool, Level, size);
        auto res = device->allocateCommandBuffers(&allocInfo, pBuffers, *loader);
        if (res != vk::Result::eSuccess) throw std::bad_alloc();
    }

    void release(vk::CommandBuffer &buffer)
    {
        // No need 'cos all buffers will be destroying after call VkDestroyCommandPool
    }
};

struct CommandPool
{
    vk::CommandPool vkPool;
    VkPool<vk::CommandBuffer, CmdBufAlloc<vk::CommandBufferLevel::ePrimary>> primary;
    VkPool<vk::CommandBuffer, CmdBufAlloc<vk::CommandBufferLevel::eSecondary>> secondary;
};

struct QueueFamilyInfo
{
    std::optional<u32> familyIndex;
    vk::Queue vkQueue;
    CommandPool pool;
};

struct DeviceQueue
{
    QueueFamilyInfo graphicsQueue;
    QueueFamilyInfo computeQueue;
    QueueFamilyInfo presentQueue;
};

/// @brief Swapchain details
struct SwapchainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class APPLIB_API Device : public DeviceQueue
{
public:
    struct Config
    {
        vk::SampleCountFlagBits msaa = vk::SampleCountFlagBits::e1; // The number of samples to use for MSAA.
        i8 gpuDevice = -1;                                          // The index of the GPU device to use.
        // The minimum fraction of sample shading.
        // A value of 1.0 ensures per-sample shading.
        f32 sampleShading = 0.0f;
    } config; // Configuration settings for the Device instance.

    vk::DispatchLoaderDynamic vkLoader;
    vk::Instance vkInstance;
    vk::Device vkDevice;
    vk::PhysicalDevice physicalDevice;
    vk::PhysicalDeviceProperties2 properties;
    vk::PhysicalDeviceMemoryProperties memoryProperties;
    vk::PhysicalDeviceDepthStencilResolveProperties depthResolveProperties;
    VmaAllocator allocator;
    vk::SurfaceKHR surface;
    VkPool<vk::Fence, FencePoolAlloc> fencePool;

    Device() = default;
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;

    // Initializes the device with the application name and version.
    void init(const std::string &appName, u32 version, DeviceCreateCtx *createCtx);

    void destroyWindowSurface();

    void destroy();

    static vk::DynamicLoader &vklib();

    // Waits for the device to be idle.
    void waitIdle() const { vkDevice.waitIdle(vkLoader); }

    /// @brief Find supported color format by current physical device
    /// @param candidates Candidates to search
    /// @param tiling Image tiling type
    /// @param features VK format features
    /// @return Filtered format on success
    vk::Format findSupportedFormat(const acul::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                   vk::FormatFeatureFlags features);

    SwapchainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }

    /// @brief Check whether the specified format supports linear filtration
    /// @param format VK format to check
    bool supportsLinearFilter(vk::Format format);

    /// @brief Get aligned size for UBO buffer by current physical device
    /// @param originalSize Size of original buffer
    size_t getAlignedUBOSize(size_t originalSize) const;

private:
#ifdef NDEBUG
    const bool _useValidationLayers = false;
#else
    const bool _useValidationLayers = true;
#endif
    vk::DebugUtilsMessengerEXT _debugMessenger;
    DeviceCreateCtx *_createCtx = nullptr;

    void createInstance(const std::string &appName, u32 version);

    void pickPhysicalDevice();

    bool isDeviceSuitable(vk::PhysicalDevice device, const acul::hashset<std::string> &allExtensions,
                          std::optional<u32> *familyIndices);

    bool validatePhysicalDevice(vk::PhysicalDevice device, acul::hashset<std::string> &ext,
                                std::optional<u32> *indices);

    void createLogicalDevice();

    void createAllocator();

    bool checkValidationLayerSupport(const acul::vector<const char *> &validationLayers);

    void setupDebugMessenger();

    acul::vector<const char *> getRequiredExtensions();

    bool checkDeviceExtensionSupport(const acul::hashset<std::string> &allExtensions) const;

    int getDeviceRating(const acul::vector<const char *> &optExtensions);

    void findQueueFamilies(std::optional<u32> *dst, vk::PhysicalDevice device);

    void allocateCmdBufPool(const vk::CommandPoolCreateInfo &createInfo, CommandPool &dst, size_t primary,
                            size_t secondary);
    void allocateCommandPools();

    void hasWindowRequiredInstanceExtensions();
    SwapchainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
};

/// @brief Get maximum MSAA sample count for a physical device
/// @param properties Physical device properties
/// @return Maximum MSAA sample count
APPLIB_API vk::SampleCountFlagBits getMaxMSAA(const vk::PhysicalDeviceProperties2 &properties);