#include "render/passes/GeometryPass.h"

#include "core/DescriptorAllocator.h"
#include "core/PipelineBuilder.h"
#include "core/VulkanContext.h"
#include "render/RenderTarget.h"
#include "resources/Mesh.h"
#include "resources/Shader.h"
#include "materials/Material.h"

#include <cstring>

GeometryPass::GeometryPass(const std::string& name, VulkanContext& context, const CreateInfo& info) : 
    RenderPass(name),
    m_context(context), 
    m_info(info)
{
    CreateDescriptorSetLayout();
    CreatePipeline();
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
               .setImage(m_info.renderTarget->GetColorImage())
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
               .setImage(m_info.renderTarget->GetDepthImage())
               .setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        cmd.pipelineBarrier2(depInfo);
    }

    // Color attachment: MSAA image + resolve into current swapchain image
    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.setImageView(m_info.renderTarget->GetColorView())
                   .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                   .setResolveMode(vk::ResolveModeFlagBits::eAverage)
                   .setResolveImageView(m_frame.swapchainImageView)
                   .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                   .setLoadOp(vk::AttachmentLoadOp::eClear)
                   .setStoreOp(vk::AttachmentStoreOp::eStore)
                   .setClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0106f, 0.0489f, 0.0731f, 1.0f }));

    // Depth attachment
    vk::RenderingAttachmentInfo depthAttachment;
    depthAttachment.setImageView(m_info.renderTarget->GetDepthView())
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

        for (const auto& primitive : renderable.mesh->GetPrimitives())
        {
            if (Material* material = renderable.mesh->GetMaterial(primitive.materialIndex))
            {
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 1, material->GetDescriptorSet(), nullptr);
            }

            cmd.drawIndexed(primitive.indexCount, 1, primitive.firstIndex, 0, 0);
        }
    }
}

void GeometryPass::EndPass(vk::raii::CommandBuffer& cmd, const FrameInfo&)
{
    cmd.endRendering();
}

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------

void GeometryPass::CreateDescriptorSetLayout()
{
    std::array<vk::DescriptorSetLayoutBinding, 1> bindings
    {
        {
            {.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment}
        }
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    m_layout = vk::raii::DescriptorSetLayout(m_context.GetDevice(), layoutInfo);
}

void GeometryPass::CreatePipeline()
{
    vk::PushConstantRange pushConstantRange;
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eVertex)
                     .setOffset(0)
                     .setSize(sizeof(glm::mat4));

    std::array<vk::DescriptorSetLayout, 2> layouts{ *m_layout, m_info.materialLayout };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo
    {
        .setLayoutCount         = 2,
        .pSetLayouts            = layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConstantRange
    };
    m_pipelineLayout = vk::raii::PipelineLayout(m_context.GetDevice(), pipelineLayoutInfo);

    m_pipeline = PipelineBuilder(m_context)
        .SetShader(*m_info.shader)
        .SetLayout(*m_pipelineLayout)
        .SetColorFormat(m_info.renderTarget->GetColorFormat())
        .SetDepthFormat(m_info.renderTarget->GetDepthFormat())
        .SetDepthTest(true, vk::CompareOp::eLess)
        .SetSamples(m_info.renderTarget->GetSamples(), 0.2f)
        .SetVertexInput(Vertex::getBindingDescription(), Vertex::getAttributeDescriptions())
        .Build();
}

void GeometryPass::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        { .type = vk::DescriptorType::eUniformBuffer, .descriptorCount = k_maxFramesInFlight }
    };

    m_descriptorPool = DescriptorAllocator::CreatePool(m_context, k_maxFramesInFlight, poolSizes);
}

void GeometryPass::CreateUniformBuffers()
{
    m_uniformBuffers.clear();

    for (size_t i = 0; i < k_maxFramesInFlight; i++)
    {
        vk::DeviceSize bufferSize = sizeof(SceneData);

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
    m_descriptorSets = DescriptorAllocator::AllocateSets(m_context, m_descriptorPool, *m_layout, k_maxFramesInFlight);

    for (size_t i = 0; i < k_maxFramesInFlight; i++)
    {
        vk::DescriptorBufferInfo bufferInfo
        {
            .buffer = m_uniformBuffers[i].buffer,
            .offset = 0,
            .range  = sizeof(SceneData)
        };

        std::array<vk::WriteDescriptorSet, 1> writes
        {
            {
                {
                    .dstSet          = m_descriptorSets[i],
                    .dstBinding      = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType  = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo     = &bufferInfo
                }
            }
        };

        m_context.GetDevice().updateDescriptorSets(writes, {});
    }
}

void GeometryPass::UpdateUniformBuffer(uint32_t frameIndex)
{
    SceneData ubo{};
    ubo.view  = m_frame.view;
    ubo.proj  = m_frame.proj;
    ubo.lightDirection = glm::vec4(m_frame.lightDirection, 0.0f);
    ubo.lightColorIntensity = glm::vec4(m_frame.lightColor, m_frame.lightIntensity);
    ubo.cameraPosition = glm::vec4(m_frame.cameraPosition, 0.0f);

    std::memcpy(m_uniformBuffers[frameIndex].mapped, &ubo, sizeof(ubo));
}
