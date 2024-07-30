#include <core/io/file.hpp>
#include <core/pipeline.hpp>

using GraphicsConfig = PipelineConfig<vk::GraphicsPipelineCreateInfo>;
GraphicsConfig &GraphicsConfig::loadDefaults()
{
    inputAssemblyInfo.setTopology(vk::PrimitiveTopology::eTriangleList).setPrimitiveRestartEnable(false);
    viewportInfo.setViewportCount(1).setScissorCount(1);
    rasterizationInfo.setDepthClampEnable(false)
        .setRasterizerDiscardEnable(false)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setLineWidth(1.0f)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setFrontFace(vk::FrontFace::eClockwise)
        .setDepthBiasEnable(false);
    colorBlendAttachment
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | // Red
                           vk::ColorComponentFlagBits::eG | // Green
                           vk::ColorComponentFlagBits::eB | // Blue
                           vk::ColorComponentFlagBits::eA)  // Alpha
        .setBlendEnable(false);
    colorBlendInfo.setLogicOpEnable(false).setAttachmentCount(1).setPAttachments(&colorBlendAttachment);
    depthStencilInfo.setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false);
    dynamicStateEnables = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    dynamicStateInfo.setPDynamicStates(dynamicStateEnables.data())
        .setDynamicStateCount(dynamicStateEnables.size())
        .setFlags(vk::PipelineDynamicStateCreateFlags());
    return *this;
}

GraphicsConfig &GraphicsConfig::enableAlphaBlending()
{
    colorBlendAttachment.setBlendEnable(true)
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

GraphicsConfig &GraphicsConfig::enableMSAA(const Device::Config config)
{
    multisampleInfo.setRasterizationSamples(config.msaa);
    if (config.msaa > vk::SampleCountFlagBits::e1 && config.sampleShading)
        multisampleInfo.setSampleShadingEnable(true).setMinSampleShading(config.sampleShading);
    return *this;
}

void ShaderModule::load(Device &device)
{
    if (io::file::readBinary(path.string(), code) != io::file::ReadState::Success)
        throw std::runtime_error("Failed to read shader " + path.filename().string());
    vk::ShaderModuleCreateInfo createInfo;
    createInfo.setCodeSize(code.size()).setPCode(reinterpret_cast<const u32 *>(code.data()));
    if (device.vkDevice.createShaderModule(&createInfo, nullptr, &module, device.vkLoader) != vk::Result::eSuccess)
        throw std::runtime_error("Failed to create shader module");
}

void prepareBasicGraphicsPipeline(PipelineBatch<vk::GraphicsPipelineCreateInfo>::Artifact &artifact, ShaderModule &vert,
                                  ShaderModule &frag, Device &device)
{
    vert.load(device);
    frag.load(device);
    artifact.config.shaderStages.emplace_back();
    artifact.config.shaderStages.back()
        .setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(vert.module)
        .setPName("main");
    artifact.config.shaderStages.emplace_back();
    artifact.config.shaderStages.back()
        .setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(frag.module)
        .setPName("main");
    artifact.config.viewportInfo.setViewportCount(1).setPViewports(nullptr).setScissorCount(1).setPScissors(nullptr);
    artifact.createInfo.setStageCount(2)
        .setPStages(artifact.config.shaderStages.data())
        .setPVertexInputState(&artifact.config.vertexInputInfo)
        .setPInputAssemblyState(&artifact.config.inputAssemblyInfo)
        .setPViewportState(&artifact.config.viewportInfo)
        .setPRasterizationState(&artifact.config.rasterizationInfo)
        .setPMultisampleState(&artifact.config.multisampleInfo)
        .setPColorBlendState(&artifact.config.colorBlendInfo)
        .setPDepthStencilState(&artifact.config.depthStencilInfo)
        .setPDynamicState(&artifact.config.dynamicStateInfo)
        .setLayout(artifact.config.pipelineLayout)
        .setRenderPass(artifact.config.renderPass)
        .setSubpass(artifact.config.subpass)
        .setBasePipelineIndex(-1)
        .setBasePipelineHandle(nullptr);
}