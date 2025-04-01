#pragma once

#include "../log.hpp"
#include "device.hpp"

namespace acul
{
    namespace gpu
    {
        // Сonfiguration settings for a Vulkan graphics pipeline.
        struct pipeline_config_base
        {
            acul::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
            vk::PipelineLayout pipelineLayout = nullptr;
        };

        template <typename T>
        struct pipeline_config;

        using graphics_config = pipeline_config<vk::GraphicsPipelineCreateInfo>;
        using compute_config = pipeline_config<vk::ComputePipelineCreateInfo>;

        template <>
        struct APPLIB_API pipeline_config<vk::GraphicsPipelineCreateInfo> final : pipeline_config_base
        {
            vk::PipelineViewportStateCreateInfo viewport_info;
            vk::PipelineInputAssemblyStateCreateInfo input_assembly_info;
            vk::PipelineRasterizationStateCreateInfo rasterization_info;
            vk::PipelineMultisampleStateCreateInfo multisample_info;
            vk::PipelineColorBlendAttachmentState color_blend_attachment;
            vk::PipelineColorBlendStateCreateInfo color_blend_info;
            vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
            vk::PipelineRasterizationConservativeStateCreateInfoEXT conservative_state_info;
            acul::vector<vk::DynamicState> dynamic_state_enables;
            vk::PipelineDynamicStateCreateInfo dynamic_state_info;
            acul::vector<vk::VertexInputBindingDescription> binding_descriptions;
            acul::vector<vk::VertexInputAttributeDescription> attribute_descriptions;
            vk::PipelineVertexInputStateCreateInfo vertex_input_info;
            vk::SpecializationInfo specialization_info;
            acul::vector<vk::SpecializationMapEntry> specialization_map;
            vk::RenderPass render_pass = nullptr;
            u32 subpass = 0;

            /**
             * @brief Load default pipeline configuration settings.
             *
             * This method initializes the pipeline configuration with default values
             * suitable for most graphics pipelines.
             *
             * @return Reference to the updated pipeline_config object.
             */
            pipeline_config &load_defaults();

            /**
             * @brief Enable alpha blending in the pipeline configuration.
             *
             * This method configures the pipeline to enable alpha blending, which is
             * typically used for transparency effects.
             *
             * @return Reference to the updated pipeline_config object.
             */
            pipeline_config &enable_alpha_blending();

            /**
             * @brief Enable multi-sample anti-aliasing (MSAA) in the pipeline configuration.
             *
             * This method configures the pipeline to use MSAA based on the given device configuration.
             *
             * @param config The device configuration specifying the MSAA settings.
             * @return Reference to the updated pipeline_config object.
             */
            pipeline_config &enable_MSAA(const device::config_t config)
            {
                multisample_info.setRasterizationSamples(config.msaa);
                if (config.msaa > vk::SampleCountFlagBits::e1 && config.sample_shading)
                    multisample_info.setSampleShadingEnable(true).setMinSampleShading(config.sample_shading);
                return *this;
            }
        };

        template <>
        struct pipeline_config<vk::ComputePipelineCreateInfo> final : public pipeline_config_base
        {
        };

        /**
         * @brief Struct to represent a Vulkan shader module.
         *
         * This struct contains information about a shader module, including its file path,
         * the Vulkan shader module object, and the shader code.
         */
        struct APPLIB_API shader_module
        {
            string path;             ///< Path to the shader file.
            vk::ShaderModule module; ///< Vulkan shader module object.
            vector<char> code;       ///< Raw shader code.

            /**
             * @brief Load the shader module from the specified device.
             *
             * This method loads the shader module into the Vulkan device, compiling the shader code
             * and creating a Vulkan shader module object.
             *
             * @param device The Vulkan device to load the shader module into.
             */
            void load(device &device);

            /**
             * @brief Destroy the shader module.
             *
             * This method destroys the Vulkan shader module object associated with this ShaderModule.
             *
             * @param device The Vulkan device that contains the shader module.
             */
            void destroy(device &device) { device.vk_device.destroyShaderModule(module, nullptr, device.loader); }
        };

        using shader_list = acul::vector<shader_module>;

        /**
         * @brief Template struct for batching Vulkan pipeline creation.
         *
         * This struct facilitates the batch creation of Vulkan pipelines. The template parameter
         * specifies the type of pipeline being created
         *
         * @tparam T The type of pipeline create info.
         */
        template <typename T>
        struct pipeline_batch
        {
            using create_info = T;
            struct artifact
            {
                template <typename V>
                using custom_data_t = acul::destructible_value<V, artifact &>;

                vk::Pipeline *pipeline;
                pipeline_config<create_info> config;
                create_info create_info;
                destructible_data<artifact &> *tmp;
            };
            vector<artifact> artifacts; ///< Stores configurations for each pipeline.
            shader_list shaders;        ///< Shader modules associated with the pipelines.
            vk::PipelineCache cache;    ///< Pipeline cache used for pipeline creation.

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
            bool allocate_pipelines(device &device)
            {
                vk::Pipeline pipelines[artifacts.size()];
                create_info create_info[artifacts.size()];
                for (int i = 0; i < artifacts.size(); i++) create_info[i] = artifacts[i].create_info;

                vk::Result res;
                if constexpr (std::is_same<T, vk::GraphicsPipelineCreateInfo>::value)
                    res = device.vk_device.createGraphicsPipelines(cache, artifacts.size(), create_info, nullptr,
                                                                   pipelines, device.loader);
                else
                    res = device.vk_device.createComputePipelines(cache, artifacts.size(), create_info, nullptr,
                                                                  pipelines, device.loader);
                if (res != vk::Result::eSuccess)
                {
                    logError("Failed to create pipelines: %s", vk::to_string(res).c_str());
                    return false;
                }
                for (int i = 0; i < artifacts.size(); i++)
                    if (artifacts[i].pipeline)
                        *(artifacts[i].pipeline) = std::move(pipelines[i]);
                    else
                        device.vk_device.destroyPipeline(pipelines[i], nullptr, device.loader);
                for (auto &shader : shaders) shader.destroy(device);
                logInfo("Created %zu pipelines", artifacts.size());
                return true;
            }

            ~pipeline_batch()
            {
                for (auto &a : artifacts) acul::release(a.tmp);
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
         * @param render_pass The render pass to be used by the pipeline.
         * @param cache The pipeline cache to be used for the pipeline creation.
         * @return The result of the pipeline creation operation. Returns vk::Result::eSuccess if
         * the pipeline was successfully created, otherwise returns the appropriate Vulkan error code.
         */
        template <typename T>
        [[nodiscard]] vk::Result create_pipeline(T *instance, device &device,
                                                 vk::RenderPass render_pass = VK_NULL_HANDLE,
                                                 vk::PipelineCache cache = nullptr)
        {
            logInfo("Creating %s pipeline", instance->name().c_str());
            typename T::Artifact artifact;
            shader_list shaders;
            vk::Result res;
            if constexpr (std::is_same_v<typename T::Artifact, vk::GraphicsPipelineCreateInfo>)
            {
                T::configure_pipeline_artifact(artifact, shaders, render_pass, instance->pipelineLayout, device);
                auto rv = device.vk_device.createGraphicsPipeline(cache, artifact.create_info, nullptr, device.loader);
                if (rv.result == vk::Result::eSuccess) instance->pipeline = rv.value;
                res = rv.result;
            }
            else
            {
                T::configure_pipeline_artifact(artifact, shaders, instance->pipelineLayout, device);
                auto rv = device.vk_device.createComputePipeline(cache, artifact.create_info, nullptr, device.loader);
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
         * @param render_pass The render pass to be used by the pipeline.
         * @param instance A pointer to the instance containing the pipeline layout and other settings.
         * @param device The Vulkan device to be used for the pipeline creation.
         */
        template <typename T>
        inline void add_pipeline_to_batch(pipeline_batch<T> &batch, T &pass, device &device,
                                          vk::RenderPass render_pass = VK_NULL_HANDLE)
        {
            logInfo("Adding %s pipeline to the batch", pass.name.c_str());
            batch.artifacts.emplace_back();
            if constexpr (std::is_same_v<T, vk::GraphicsPipelineCreateInfo>)
                pass.configure_pipeline_artifact(batch.artifacts.back(), batch.shaders, render_pass, pass.layout,
                                                 device);
            else
                pass.configure_pipeline_artifact(batch.artifacts.back(), batch.shaders, pass.layout, device);
            batch.artifacts.back().pipeline = &pass.pipeline;
        }

        /**
         * @brief Prepares a basic graphics pipeline.
         *
         * This function sets up the basic configuration for a graphics pipeline, suitable for simple
         * operations such as rendering a full-screen quad. It configures the pipeline with the provided
         * vertex and fragment shaders, and sets default states for various pipeline stages.
         *
         * @param config The pipeline configuration to be filled in.
         * @param create_info The create info structure for the graphics pipeline.
         * @param vert The vertex shader module.
         * @param frag The fragment shader module.
         * @param device The Vulkan device to be used for pipeline creation.
         */
        APPLIB_API void
        prepare_base_graphics_pipeline(pipeline_batch<vk::GraphicsPipelineCreateInfo>::artifact &artifact,
                                       shader_module &vert, shader_module &frag, device &device);
    } // namespace gpu
} // namespace acul