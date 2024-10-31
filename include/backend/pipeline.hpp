#ifndef APP_GRAPHICS_PIPELINE_H
#define APP_GRAPHICS_PIPELINE_H

#include "../core/log.hpp"
#include "device.hpp"

// Сonfiguration settings for a Vulkan graphics pipeline.
struct IPipelineConfig
{
    astl::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    vk::PipelineLayout pipelineLayout = nullptr;
};

template <typename T>
struct PipelineConfig;

template <>
struct APPLIB_API PipelineConfig<vk::GraphicsPipelineCreateInfo> final : public IPipelineConfig
{
    vk::PipelineViewportStateCreateInfo viewportInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
    vk::PipelineMultisampleStateCreateInfo multisampleInfo;
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo;
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo;
    vk::PipelineRasterizationConservativeStateCreateInfoEXT conservativeState;
    astl::vector<vk::DynamicState> dynamicStateEnables;
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
    astl::vector<vk::VertexInputBindingDescription> bindingDescriptions;
    astl::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vk::RenderPass renderPass = nullptr;
    u32 subpass = 0;

    /**
     * @brief Load default pipeline configuration settings.
     *
     * This method initializes the pipeline configuration with default values
     * suitable for most graphics pipelines.
     *
     * @return Reference to the updated PipelineConfig object.
     */
    PipelineConfig &loadDefaults();

    /**
     * @brief Enable alpha blending in the pipeline configuration.
     *
     * This method configures the pipeline to enable alpha blending, which is
     * typically used for transparency effects.
     *
     * @return Reference to the updated PipelineConfig object.
     */
    PipelineConfig &enableAlphaBlending();

    /**
     * @brief Enable multi-sample anti-aliasing (MSAA) in the pipeline configuration.
     *
     * This method configures the pipeline to use MSAA based on the given device configuration.
     *
     * @param config The device configuration specifying the MSAA settings.
     * @return Reference to the updated PipelineConfig object.
     */
    PipelineConfig &enableMSAA(const Device::Config config);
};

template <>
struct PipelineConfig<vk::ComputePipelineCreateInfo> final : public IPipelineConfig
{
};

/**
 * @brief Struct to represent a Vulkan shader module.
 *
 * This struct contains information about a shader module, including its file path,
 * the Vulkan shader module object, and the shader code.
 */
struct APPLIB_API ShaderModule
{
    std::filesystem::path path; ///< Path to the shader file.
    vk::ShaderModule module;    ///< Vulkan shader module object.
    astl::vector<char> code;    ///< Raw shader code.

    /**
     * @brief Load the shader module from the specified device.
     *
     * This method loads the shader module into the Vulkan device, compiling the shader code
     * and creating a Vulkan shader module object.
     *
     * @param device The Vulkan device to load the shader module into.
     */
    void load(Device &device);

    /**
     * @brief Destroy the shader module.
     *
     * This method destroys the Vulkan shader module object associated with this ShaderModule.
     *
     * @param device The Vulkan device that contains the shader module.
     */
    void destroy(Device &device) { device.vkDevice.destroyShaderModule(module, nullptr, device.vkLoader); }
};

using Shaders = astl::vector<ShaderModule>;

/**
 * @brief Template struct for batching Vulkan pipeline creation.
 *
 * This struct facilitates the batch creation of Vulkan pipelines. The template parameter
 * specifies the type of pipeline being created
 *
 * @tparam T The type of pipeline create info.
 */
template <typename T>
struct PipelineBatch
{
    struct Artifact
    {
        using create_info_t = T;

        vk::Pipeline *pipeline;
        PipelineConfig<create_info_t> config;
        create_info_t createInfo;
    };
    astl::vector<Artifact> artifacts;   ///< Stores configurations for each pipeline.
    astl::vector<ShaderModule> shaders; ///< Shader modules associated with the pipelines.
    vk::PipelineCache cache;            ///< Pipeline cache used for pipeline creation.

    /**
     * @brief Creates Vulkan pipelines in a batch operation.
     *
     * This method uses the stored create info structures to create multiple Vulkan pipelines at once.
     * After creation, it updates the pointers to the newly created pipeline objects and cleans up any
     * temporary shader modules.
     *
     * @param device The Vulkan device to use for pipeline creation.
     * @return True if all pipelines were successfully created, otherwise false.
     */
    inline bool allocatePipelines(Device &device)
    {
        vk::Pipeline pipelines[artifacts.size()];
        T createInfo[artifacts.size()];
        for (int i = 0; i < artifacts.size(); i++) createInfo[i] = artifacts[i].createInfo;

        vk::Result res;
        if constexpr (std::is_same<T, vk::GraphicsPipelineCreateInfo>::value)
            res = device.vkDevice.createGraphicsPipelines(cache, artifacts.size(), createInfo, nullptr, pipelines,
                                                          device.vkLoader);
        else
            res = device.vkDevice.createComputePipelines(cache, artifacts.size(), createInfo, nullptr, pipelines,
                                                         device.vkLoader);
        if (res != vk::Result::eSuccess)
        {
            logError("Failed to create pipelines: %s", vk::to_string(res).c_str());
            return false;
        }
        for (int i = 0; i < artifacts.size(); i++)
            if (artifacts[i].pipeline)
                *(artifacts[i].pipeline) = std::move(pipelines[i]);
            else
                device.vkDevice.destroyPipeline(pipelines[i], nullptr, device.vkLoader);
        for (auto &shader : shaders) shader.destroy(device);
        logInfo("Created %zu pipelines", artifacts.size());
        return true;
    }
};

/**
 * @brief Creates a Vulkan pipeline for the given instance.
 *
 * This function prepares the pipeline configuration, loads the necessary shaders,
 * and creates a Vulkan pipeline for the specified instance.
 *
 * @tparam T The type of the instance for which the pipeline is being created.
 * @param instance A pointer to the instance containing the pipeline layout and other settings.
 * @param device The Vulkan device to be used for the pipeline creation.
 * @param renderPass The render pass to be used by the pipeline.
 * @param cache The pipeline cache to be used for the pipeline creation.
 * @return The result of the pipeline creation operation. Returns vk::Result::eSuccess if
 * the pipeline was successfully created, otherwise returns the appropriate Vulkan error code.
 */
template <typename T>
[[nodiscard]] vk::Result createPipeline(T *instance, Device &device, vk::RenderPass renderPass = VK_NULL_HANDLE,
                                        vk::PipelineCache cache = nullptr)
{
    logInfo("Creating %s pipeline", instance->name().c_str());
    typename T::Artifact artifact;
    Shaders shaders;
    vk::Result res;
    if constexpr (std::is_same_v<typename T::Artifact, vk::GraphicsPipelineCreateInfo>)
    {
        T::configurePipelineArtifact(artifact, shaders, renderPass, instance->pipelineLayout, device);
        auto rv = device.vkDevice.createGraphicsPipeline(cache, artifact.createInfo, nullptr, device.vkLoader);
        if (rv.result == vk::Result::eSuccess) instance->pipeline = rv.value;
        res = rv.result;
    }
    else
    {
        T::configurePipelineArtifact(artifact, shaders, instance->pipelineLayout, device);
        auto rv = device.vkDevice.createComputePipeline(cache, artifact.createInfo, nullptr, device.vkLoader);
        if (rv.result == vk::Result::eSuccess) instance->pipeline = rv.value;
        res = rv.result;
    }
    for (auto &shader : shaders) shader.destroy(device);
    return res;
}

/**
 * @brief Adds a pipeline configuration to the batch for later creation.
 *
 * This function prepares a pipeline configuration and adds it to the batch. It sets up the necessary
 * configurations and tracks the pipeline object that will be created.
 *
 * @tparam T The type of the instance for which the pipeline is being configured.
 * @param batch The batch to which the pipeline configuration will be added.
 * @param renderPass The render pass to be used by the pipeline.
 * @param instance A pointer to the instance containing the pipeline layout and other settings.
 * @param device The Vulkan device to be used for the pipeline creation.
 */
template <typename T>
inline void addPipelineToBatch(PipelineBatch<typename T::Artifact::create_info_t> &batch, T *instance, Device &device,
                               vk::RenderPass renderPass = VK_NULL_HANDLE)
{
    logInfo("Adding %s pipeline to the batch", instance->name().c_str());
    batch.artifacts.emplace_back();
    using create_info_t = typename T::Artifact::create_info_t;
    if constexpr (std::is_same_v<create_info_t, vk::GraphicsPipelineCreateInfo>)
        T::configurePipelineArtifact(batch.artifacts.back(), batch.shaders, renderPass, instance->pipelineLayout,
                                     device);
    else
        T::configurePipelineArtifact(batch.artifacts.back(), batch.shaders, instance->pipelineLayout, device);
    batch.artifacts.back().pipeline = &instance->pipeline;
}

/**
 * @brief Prepares a basic graphics pipeline.
 *
 * This function sets up the basic configuration for a graphics pipeline, suitable for simple
 * operations such as rendering a full-screen quad. It configures the pipeline with the provided
 * vertex and fragment shaders, and sets default states for various pipeline stages.
 *
 * @param config The pipeline configuration to be filled in.
 * @param createInfo The create info structure for the graphics pipeline.
 * @param vert The vertex shader module.
 * @param frag The fragment shader module.
 * @param device The Vulkan device to be used for pipeline creation.
 */
APPLIB_API void prepareBasicGraphicsPipeline(PipelineBatch<vk::GraphicsPipelineCreateInfo>::Artifact &artifact,
                                             ShaderModule &vert, ShaderModule &frag, Device &device);
#endif