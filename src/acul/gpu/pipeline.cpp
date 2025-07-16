#include <acul/gpu/pipeline.hpp>
#include <acul/io/file.hpp>

namespace acul
{
    namespace gpu
    {
        graphics_config &graphics_config::load_defaults()
        {
            input_assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList).setPrimitiveRestartEnable(false);
            viewport_info.setViewportCount(1).setScissorCount(1);
            rasterization_info.setDepthClampEnable(false)
                .setRasterizerDiscardEnable(false)
                .setPolygonMode(vk::PolygonMode::eFill)
                .setLineWidth(1.0f)
                .setCullMode(vk::CullModeFlagBits::eNone)
                .setFrontFace(vk::FrontFace::eClockwise)
                .setDepthBiasEnable(false);
            color_blend_attachment
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | // Red
                                   vk::ColorComponentFlagBits::eG | // Green
                                   vk::ColorComponentFlagBits::eB | // Blue
                                   vk::ColorComponentFlagBits::eA)  // Alpha
                .setBlendEnable(false);
            color_blend_info.setLogicOpEnable(false).setAttachmentCount(1).setPAttachments(&color_blend_attachment);
            depth_stencil_info.setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false);
            dynamic_state_enables = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
            dynamic_state_info.setPDynamicStates(dynamic_state_enables.data())
                .setDynamicStateCount(dynamic_state_enables.size())
                .setFlags(vk::PipelineDynamicStateCreateFlags());
            return *this;
        }

        graphics_config &graphics_config::enable_alpha_blending()
        {
            color_blend_attachment.setBlendEnable(true)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eSrcAlpha)
                .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setAlphaBlendOp(vk::BlendOp::eAdd);
            return *this;
        }

        void shader_module::load(device &device)
        {
            if (io::file::read_binary(path, code) != io::file::op_state::success)
                throw runtime_error("Failed to read shader: %s" + io::get_filename(path));
            vk::ShaderModuleCreateInfo create_info;
            create_info.setCodeSize(code.size()).setPCode(reinterpret_cast<const u32 *>(code.data()));
            if (device.vk_device.createShaderModule(&create_info, nullptr, &module, device.loader) !=
                vk::Result::eSuccess)
                throw runtime_error("Failed to create shader module");
        }

        void prepare_base_graphics_pipeline(pipeline_batch<vk::GraphicsPipelineCreateInfo>::artifact &artifact,
                                            shader_module &vert, shader_module &frag, device &device)
        {
            vert.load(device);
            frag.load(device);
            artifact.config.shader_stages.emplace_back();
            artifact.config.shader_stages.back()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(vert.module)
                .setPName("main");
            artifact.config.shader_stages.emplace_back();
            artifact.config.shader_stages.back()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(frag.module)
                .setPName("main");
            artifact.config.viewport_info.setViewportCount(1).setPViewports(nullptr).setScissorCount(1).setPScissors(
                nullptr);
            artifact.create_info.setStageCount(2)
                .setPStages(artifact.config.shader_stages.data())
                .setPVertexInputState(&artifact.config.vertex_input_info)
                .setPInputAssemblyState(&artifact.config.input_assembly_info)
                .setPViewportState(&artifact.config.viewport_info)
                .setPRasterizationState(&artifact.config.rasterization_info)
                .setPMultisampleState(&artifact.config.multisample_info)
                .setPColorBlendState(&artifact.config.color_blend_info)
                .setPDepthStencilState(&artifact.config.depth_stencil_info)
                .setPDynamicState(&artifact.config.dynamic_state_info)
                .setLayout(artifact.config.pipeline_layout)
                .setRenderPass(artifact.config.render_pass)
                .setSubpass(artifact.config.subpass)
                .setBasePipelineIndex(-1)
                .setBasePipelineHandle(nullptr);
        }
    } // namespace gpu
} // namespace acul