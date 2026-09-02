#include "render/passes/GeometryPass.h"

#include "core/DescriptorAllocator.h"
#include "core/VulkanContext.h"
#include "resources/Mesh.h"
#include "resources/Shader.h"
#include "resources/Texture.h"

#include <cstring>

GeometryPass::GeometryPass(const std::string& name, VulkanContext& context, const CreateInfo& info)
    : RenderPass(name), m_context(context), m_info(info)
{
    m_samples     = QueryMaxUsableSampleCount();
    m_depthFormat = FindDepthFormat();

    CreateDescriptorSetLayout();
    CreatePipeline();
    CreateAttachments();
    CreateDescriptorPool();
    CreateUniformBuffers();
    CreateDescriptorSets();
}

// -----------------------------------------------------------------------------
// Pass execution
// -----------------------------------------------------------------------------

void GeometryPass::BeginPass(vk::raii::CommandBuffer& cmd, const FrameInfo&)
{
    // Transition MSAA color attachment: Undefined -> ColorAttachmentOptimal
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
               .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setOldLayout(vk::ImageLayout::eUndefined)
               .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
               .setImage(m_colorImage)
               .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        cmd.pipelineBarrier2(depInfo);
    }

    // Transition depth attachment: Undefined -> DepthAttachmentOptimal
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
               .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests)
               .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead
                                | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
               .setOldLayout(vk::ImageLayout::eUndefined)
               .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
               .setImage(m_depthImage)
               .setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        cmd.pipelineBarrier2(depInfo);
    }

    // Color attachment: MSAA image + resolve into current swapchain image
    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.setImageView(*m_colorView)
                   .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                   .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                   .setResolveImageView(m_frame.swapchainImageView)
                   .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                   .setStoreOp(vk::AttachmentStoreOp::eStore)
                   .setClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0106f, 0.0489f, 0.0731f, 1.0f }));

    // Depth attachment
    vk::RenderingAttachmentInfo depthAttachment;
    depthAttachment.setImageView(*m_depthView)
                   .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                   .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                   .setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

    vk::RenderingInfo renderingInfo;
    renderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, m_frame.extent))
                 .setLayerCount(1)
                 .setColorAttachmentCount(1)
                 .setPColorAttachments(&colorAttachment)
                 .setPDepthAttachment(&depthAttachment);

    cmd.beginRendering(renderingInfo);
}

void GeometryPass::Render(vk::raii::CommandBuffer& cmd, const FrameInfo& frame)
{
    if (m_frame.renderables.empty()) return;

    UpdateUniformBuffer(frame.frameIndex);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);

    // Viewport/scissor are dynamic — set from the current frame extent
    cmd.setViewport(0, vk::Viewport{
        0.0f, 0.0f,
        static_cast<float>(m_frame.extent.width),
        static_cast<float>(m_frame.extent.height),
        0.0f, 1.0f });
    cmd.setScissor(0, vk::Rect2D{ vk::Offset2D{ 0, 0 }, m_frame.extent });

    // Bind per-frame descriptor set (UBO + texture)
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0, *m_descriptorSets[frame.frameIndex], nullptr);

    for (auto& renderable : m_frame.renderables)
    {
        cmd.pushConstants<glm::mat4>(*m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, renderable.model);

        // Bind the mesh geometry
        cmd.bindVertexBuffers(0, renderable.mesh->GetVertexBuffer(), { 0 });
        cmd.bindIndexBuffer(renderable.mesh->GetIndexBuffer(), 0, vk::IndexType::eUint32);

        cmd.drawIndexed(renderable.mesh->GetIndexCount(), 1, 0, 0, 0);
    }
}

void GeometryPass::EndPass(vk::raii::CommandBuffer& cmd, const FrameInfo&)
{
    cmd.endRendering();
}

void GeometryPass::OnResize(vk::Extent2D newExtent)
{
    // Release the current attachments first so their memory is freed before we allocate the new ones.
    // Views must go before the images/memory they reference.
    m_colorView   = nullptr;
    m_colorMemory = nullptr;
    m_colorImage  = nullptr;

    m_depthView   = nullptr;
    m_depthMemory = nullptr;
    m_depthImage  = nullptr;

    m_info.extent = newExtent;
    CreateAttachments();
}

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------

void GeometryPass::CreateDescriptorSetLayout()
{
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings
    {
        {
            {.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer,        .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex},
            {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}
        }
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    m_setLayout = vk::raii::DescriptorSetLayout(m_context.GetDevice(), layoutInfo);
}

void GeometryPass::CreatePipeline()
{
    const vk::raii::ShaderModule& shaderModule = m_info.shader->GetShaderModule();

    // Vertex + fragment stages (entry points defined in the Slang source)
    vk::PipelineShaderStageCreateInfo vertStage
    {
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shaderModule,
        .pName = "vertMain"
    };
    vk::PipelineShaderStageCreateInfo fragStage
    {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = "fragMain"
    };
    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    // Vertex input matches the Vertex struct defined in RenderTypes.h
    auto bindingDescription    = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo
    {
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions    = attributeDescriptions.data()
    };

    vk::PipelineDepthStencilStateCreateInfo depthStencil
    {
        .depthTestEnable       = vk::True,
        .depthWriteEnable      = vk::True,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable     = vk::False
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = vk::PrimitiveTopology::eTriangleList };

    // Viewport/scissor are dynamic — only counts required here
    vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };

    vk::PipelineRasterizationStateCreateInfo rasterizer
    {
        .depthClampEnable        = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eNone,
        .frontFace               = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable         = vk::False,
        .lineWidth               = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling
    {
        .rasterizationSamples = m_samples,
        .sampleShadingEnable  = vk::True,
        .minSampleShading     = 0.2f
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable    = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending
    {
        .logicOpEnable   = vk::False,
        .logicOp         = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment
    };

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState
    {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates    = dynamicStates.data()
    };

    vk::PushConstantRange pushConstantRange;
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eVertex)
                     .setOffset(0)
                     .setSize(sizeof(glm::mat4));

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo
    {
        .setLayoutCount         = 1,
        .pSetLayouts            = &*m_setLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConstantRange
    };
    m_pipelineLayout = vk::raii::PipelineLayout(m_context.GetDevice(), pipelineLayoutInfo);

    // Dynamic rendering: no VkRenderPass, formats are declared here instead
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> chain;

    auto& pipelineInfo = chain.get<vk::GraphicsPipelineCreateInfo>();
    pipelineInfo.setStages(shaderStages)
                .setPVertexInputState(&vertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPMultisampleState(&multisampling)
                .setPDepthStencilState(&depthStencil)
                .setPColorBlendState(&colorBlending)
                .setPDynamicState(&dynamicState)
                .setLayout(m_pipelineLayout)
                .setRenderPass(nullptr);

    auto& renderingInfo = chain.get<vk::PipelineRenderingCreateInfo>();
    renderingInfo.setColorAttachmentCount(1)
                 .setColorAttachmentFormats(m_info.colorFormat)
                 .setDepthAttachmentFormat(m_depthFormat);

    m_pipeline = vk::raii::Pipeline(m_context.GetDevice(), nullptr, chain.get<vk::GraphicsPipelineCreateInfo>());
}

void GeometryPass::CreateAttachments()
{
    // MSAA color attachment
    std::tie(m_colorImage, m_colorMemory) = m_context.CreateImage(
        m_info.extent.width, m_info.extent.height, 1,
        m_samples,
        m_info.colorFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    m_colorView = m_context.CreateImageView(m_colorImage, m_info.colorFormat, vk::ImageAspectFlagBits::eColor, 1);

    // Depth attachment
    std::tie(m_depthImage, m_depthMemory) = m_context.CreateImage(
        m_info.extent.width, m_info.extent.height, 1,
        m_samples,
        m_depthFormat,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    m_depthView = m_context.CreateImageView(m_depthImage, m_depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

void GeometryPass::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        {.type = vk::DescriptorType::eUniformBuffer,        .descriptorCount = k_maxFramesInFlight},
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = k_maxFramesInFlight}
    };

    m_descriptorPool = DescriptorAllocator::CreatePool(m_context, k_maxFramesInFlight, poolSizes);
}

void GeometryPass::CreateUniformBuffers()
{
    m_uniformBuffers.clear();

    for (size_t i = 0; i < k_maxFramesInFlight; i++)
    {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        auto [buffer, bufferMem] = m_context.CreateBuffer(bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        m_uniformBuffers.emplace_back(UBOBuffer());
        m_uniformBuffers[i].buffer = std::move(buffer);
        m_uniformBuffers[i].memory = std::move(bufferMem);
        m_uniformBuffers[i].mapped = m_uniformBuffers[i].memory.mapMemory(0, bufferSize);
    }
}

void GeometryPass::CreateDescriptorSets()
{
    m_descriptorSets = DescriptorAllocator::AllocateSets(m_context, m_descriptorPool, *m_setLayout, k_maxFramesInFlight);

    for (size_t i = 0; i < k_maxFramesInFlight; i++)
    {
        vk::DescriptorBufferInfo bufferInfo
        {
            .buffer = m_uniformBuffers[i].buffer,
            .offset = 0,
            .range  = sizeof(UniformBufferObject)
        };

        vk::DescriptorImageInfo imageInfo
        {
            .sampler     = m_info.texture->GetSampler(),
            .imageView   = m_info.texture->GetImageView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        std::array<vk::WriteDescriptorSet, 2> writes
        {
            {
                {
                    .dstSet          = m_descriptorSets[i],
                    .dstBinding      = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo     = &bufferInfo
                },
                {
                    .dstSet          = m_descriptorSets[i],
                    .dstBinding      = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo      = &imageInfo
                }
            }
        };

        m_context.GetDevice().updateDescriptorSets(writes, {});
    }
}

void GeometryPass::UpdateUniformBuffer(uint32_t frameIndex)
{
    UniformBufferObject ubo{};
    ubo.view  = m_frame.view;
    ubo.proj  = m_frame.proj;

    std::memcpy(m_uniformBuffers[frameIndex].mapped, &ubo, sizeof(ubo));
}

vk::SampleCountFlagBits GeometryPass::QueryMaxUsableSampleCount() const
{
    vk::PhysicalDeviceProperties props = m_context.GetPhysicalDevice().getProperties();

    vk::SampleCountFlags counts = props.limits.framebufferColorSampleCounts
                                & props.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
    if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
    if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
    if (counts & vk::SampleCountFlagBits::e8)  return vk::SampleCountFlagBits::e8;
    if (counts & vk::SampleCountFlagBits::e4)  return vk::SampleCountFlagBits::e4;
    if (counts & vk::SampleCountFlagBits::e2)  return vk::SampleCountFlagBits::e2;
    return vk::SampleCountFlagBits::e1;
}

vk::Format GeometryPass::FindDepthFormat() const
{
    // Depth-only to keep aspect mask simple (no stencil aspect required in barriers/views).
    return m_context.FindSupportedFormat(
        { vk::Format::eD32Sfloat },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}
