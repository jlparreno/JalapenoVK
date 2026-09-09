#include "core/PipelineBuilder.h"
#include "core/VulkanContext.h"
#include "resources/Shader.h"

PipelineBuilder::PipelineBuilder(VulkanContext& context) : 
    m_context(context)
{
}

vk::raii::Pipeline PipelineBuilder::Build()
{
    // Pipeline validation
    if (!m_shader)
        throw std::runtime_error("PipelineBuilder: no shader set");
    
    if (!m_layout)
        throw std::runtime_error("PipelineBuilder: no pipeline layout set");
    
    if (m_colorFormat == vk::Format::eUndefined)
        throw std::runtime_error("PipelineBuilder: no color format set");

    if (m_depthTest && m_depthFormat == vk::Format::eUndefined)
        throw std::runtime_error("PipelineBuilder: depth test enabled without a depth format");

    // Shader creation
    const vk::raii::ShaderModule& shaderModule = m_shader->GetShaderModule();

    vk::PipelineShaderStageCreateInfo vertStage
    {
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shaderModule,
        .pName = m_vertEntry
    };
    vk::PipelineShaderStageCreateInfo fragStage
    {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = m_fragEntry
    };
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    // Vertex Input, fill only if there is any data
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    if (!m_vertexAttributes.empty())
    {
        vertexInputInfo =
        {
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &m_vertexBinding,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributes.size()),
            .pVertexAttributeDescriptions = m_vertexAttributes.data()
        };
    }

    // We will receive triangles
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList };

    // Viewport/scissor are dynamic, only counts required here
    vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer
    {
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };

    // Multisampling state
    vk::PipelineMultisampleStateCreateInfo multisampling
    {
        .rasterizationSamples = m_samples,
        .sampleShadingEnable = m_minSampleShading > 0.0f ? vk::True : vk::False,
        .minSampleShading = m_minSampleShading
    };

    // Blending (for now we are not using blending)
    vk::PipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending
    {
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // Dynamic states for viewport and scissor
    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState
    {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    // Dynamic rendering: no VkRenderPass, formats are declared here instead.
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain;

    // Set all pipeline creation info
    auto& pipelineInfo = chain.get<vk::GraphicsPipelineCreateInfo>();
    pipelineInfo.setStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPColorBlendState(&colorBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(m_layout)
        .setRenderPass(nullptr);

    // Configure color attachment
    auto& pipelineRenderingInfo = chain.get<vk::PipelineRenderingCreateInfo>();
    pipelineRenderingInfo.setColorAttachmentFormats(m_colorFormat);

    // Configure depth if it has been enabled
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    if (m_depthTest)
    {
        depthStencil =
        {
            .depthTestEnable = m_depthTest,
            .depthWriteEnable = m_depthWrite,
            .depthCompareOp = m_depthCompare,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False
        };

        pipelineInfo.setPDepthStencilState(&depthStencil);
        pipelineRenderingInfo.setDepthAttachmentFormat(m_depthFormat);
    }

    // Create pipeline and return it
    vk::raii::Pipeline pipeline(m_context.GetDevice(), nullptr, chain.get<vk::GraphicsPipelineCreateInfo>());

    return pipeline;
}

PipelineBuilder& PipelineBuilder::SetVertexInput(const vk::VertexInputBindingDescription& binding, vk::ArrayProxy<const vk::VertexInputAttributeDescription> attributes)
{
    m_vertexBinding = binding;
    m_vertexAttributes.assign(attributes.begin(), attributes.end());

    return *this;
}

PipelineBuilder& PipelineBuilder::SetShader(const Shader& shader, const char* vert, const char* frag)
{
    m_shader = &shader;
    m_vertEntry = vert;
    m_fragEntry = frag;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthTest(bool write, vk::CompareOp compareOp)
{
    m_depthTest = true;
    m_depthWrite = write;
    m_depthCompare = compareOp;
    
    return *this;
}

PipelineBuilder& PipelineBuilder::SetLayout(vk::PipelineLayout layout)
{
    m_layout = layout;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetColorFormat(vk::Format format)
{
    m_colorFormat = format;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthFormat(vk::Format format)
{
    m_depthFormat = format;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetSamples(vk::SampleCountFlagBits samples, float minSampleShading)
{
    m_samples = samples;
    m_minSampleShading = minSampleShading;
    
    return *this;
}