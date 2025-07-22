#pragma once

#include "../list.hpp"
#include "../log.hpp"
#include "device.hpp"

namespace acul
{
    namespace gpu
    {
        // Сonfiguration settings for a Vulkan graphics pipeline.
        struct pipeline_config_base
        {
            acul::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
            vk::PipelineLayout pipeline_layout = nullptr;
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
             * @param msaa MSAA Levelt
             * @param sample_shading Sample Shading. If set to 0.0 (default) then no apply this option
             * @return Reference to the updated pipeline_config object.
             */
            pipeline_config &enable_msaa(vk::SampleCountFlagBits msaa, f32 sample_shading = 0.0f)
            {
                multisample_info.setRasterizationSamples(msaa);
                if (msaa > vk::SampleCountFlagBits::e1 && sample_shading != 0.0f)
                    multisample_info.setSampleShadingEnable(true).setMinSampleShading(sample_shading);
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
            bool load(device &device);

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

        template <typename T>
        struct pipeline_batch;

        namespace details
        {
            template <typename T>
            struct artifact
            {
                template <typename V>
                using custom_data_t = acul::destructible_value<V, artifact &>;

                pipeline_config<T> config;
                T create_info;
                destructible_data<artifact &> *tmp = nullptr;
                std::function<void(vk::Pipeline)> commit = nullptr;
            };

            template <typename T>
            struct pipeline_artifact_configure
            {
                static_assert(sizeof(T) == 0, "pipeline_artifact_configure<>: unsupported pipeline type");
            };

            template <>
            struct pipeline_artifact_configure<vk::GraphicsPipelineCreateInfo>
            {
                using callback_type = bool (*)(artifact<vk::GraphicsPipelineCreateInfo> &, shader_list &,
                                               vk::RenderPass, vk::PipelineLayout &, device &);
            };

            template <>
            struct pipeline_artifact_configure<vk::ComputePipelineCreateInfo>
            {
                using callback_type = bool (*)(artifact<vk::ComputePipelineCreateInfo> &, shader_list &,
                                               vk::PipelineLayout &, device &);
            };
        } // namespace details

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
            using artifact = details::artifact<T>;
            using PFN_configure_artifact = typename details::pipeline_artifact_configure<create_info>::callback_type;
            list<artifact> artifacts; ///< Stores configurations for each pipeline.
            shader_list shaders;      ///< Shader modules associated with the pipelines.
            vk::PipelineCache cache;  ///< Pipeline cache used for pipeline creation.

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
            bool allocate_pipelines(device &device, size_t size)
            {
                vk::Pipeline pipelines[size];
                create_info create_info[size];
                auto it = artifacts.begin();
                for (size_t i = 0; i < size; i++, ++it) create_info[i] = it->create_info;

                vk::Result res;
                if constexpr (std::is_same<T, vk::GraphicsPipelineCreateInfo>::value)
                    res = device.vk_device.createGraphicsPipelines(cache, size, create_info, nullptr, pipelines,
                                                                   device.loader);
                else
                    res = device.vk_device.createComputePipelines(cache, size, create_info, nullptr, pipelines,
                                                                  device.loader);
                for (auto &shader : shaders) shader.destroy(device);
                if (res != vk::Result::eSuccess)
                {
                    LOG_ERROR("Failed to create pipelines: %s", vk::to_string(res).c_str());
                    return false;
                }

                it = artifacts.begin();
                for (size_t i = 0; i < size; i++, ++it)
                    if (it->commit)
                        it->commit(pipelines[i]);
                    else
                        device.vk_device.destroyPipeline(pipelines[i], nullptr, device.loader);
                LOG_INFO("Created %zu pipelines", size);
                return true;
            }

            ~pipeline_batch()
            {
                for (auto &a : artifacts) acul::release(a.tmp);
            }
        };

        using graphics_pipeline_batch = pipeline_batch<vk::GraphicsPipelineCreateInfo>;
        using compute_pipeline_batch = pipeline_batch<vk::ComputePipelineCreateInfo>;

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
        APPLIB_API bool
        prepare_base_graphics_pipeline(pipeline_batch<vk::GraphicsPipelineCreateInfo>::artifact &artifact,
                                       shader_module &vert, shader_module &frag, device &device);
    } // namespace gpu
} // namespace acul