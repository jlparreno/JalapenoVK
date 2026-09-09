#include "render/passes/SkyboxPass.h"

#include "core/DescriptorAllocator.h"
#include "core/PipelineBuilder.h"
#include "core/VulkanContext.h"
#include "render/EnvironmentMap.h"
#include "render/RenderTarget.h"

SkyboxPass::SkyboxPass(const std::string& name, VulkanContext& context, const CreateInfo& info) :
    RenderPass(name),
    m_context(context), 
    m_info(info)
{
    CreateDescriptorSetLayout();
    CreatePipeline();

    CreateDescriptorPool();
    CreateDescriptorSets();
}

// -----------------------------------------------------------------------------
// Pass execution
// -----------------------------------------------------------------------------

void SkyboxPass::BeginPass(vk::raii::CommandBuffer& cmd, const FrameInfo& frame)
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

    // Color attachment: MSAA image. No resolve as we are not the last pass.
    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.setImageView(m_info.renderTarget->GetColorView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }));

    vk::RenderingInfo renderingInfo;
    renderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, m_frame.extent))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&colorAttachment);

    cmd.beginRendering(renderingInfo);
}

void SkyboxPass::Render(vk::raii::CommandBuffer& cmd, const FrameInfo& frame)
{
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);

    // Viewport/scissor are dynamic — set from the current frame extent
    cmd.setViewport(0, vk::Viewport{
        0.0f, 0.0f,
        static_cast<float>(m_frame.extent.width),
        static_cast<float>(m_frame.extent.height),
        0.0f, 1.0f });
    cmd.setScissor(0, vk::Rect2D{ vk::Offset2D{ 0, 0 }, m_frame.extent });

    // Bind descriptor set with environment cubemap
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_pipelineLayout, 0, *m_descriptorSet, nullptr);

    // Set push constant with the matrix info
    cmd.pushConstants<glm::mat4>(*m_pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, m_frame.invViewProj);

    // Draw fullscreen triangle
    cmd.draw(3, 1, 0, 0);
}

void SkyboxPass::EndPass(vk::raii::CommandBuffer& cmd, const FrameInfo& frame)
{
    cmd.endRendering();
}

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------

void SkyboxPass::CreateDescriptorSetLayout()
{
    std::array<vk::DescriptorSetLayoutBinding, 1> bindings
    {
        {
            {.binding = 0, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}
        }
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    m_layout = vk::raii::DescriptorSetLayout(m_context.GetDevice(), layoutInfo);
}

void SkyboxPass::CreatePipeline()
{
    vk::PushConstantRange pushConstantRange;
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eFragment)
        .setOffset(0)
        .setSize(sizeof(glm::mat4));

    std::array<vk::DescriptorSetLayout, 1> layouts{ *m_layout };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo
    {
        .setLayoutCount = 1,
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    m_pipelineLayout = vk::raii::PipelineLayout(m_context.GetDevice(), pipelineLayoutInfo);

    m_pipeline = PipelineBuilder(m_context)
        .SetShader(*m_info.shader)
        .SetLayout(*m_pipelineLayout)
        .SetColorFormat(m_info.renderTarget->GetColorFormat())
        .SetSamples(m_info.renderTarget->GetSamples())
        .Build();
}

void SkyboxPass::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1 }
    };

    m_descriptorPool = DescriptorAllocator::CreatePool(m_context, 1, poolSizes);
}

void SkyboxPass::CreateDescriptorSets()
{
    std::vector<vk::raii::DescriptorSet> sets = DescriptorAllocator::AllocateSets(m_context, m_descriptorPool, m_layout, 1);
    m_descriptorSet = std::move(sets[0]);

    vk::DescriptorImageInfo imageInfo
    {
        .sampler = m_info.environmentMap->GetEnvironmentSampler(),
        .imageView = m_info.environmentMap->GetEnvironmentView(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    vk::WriteDescriptorSet write
    {
        .dstSet = m_descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &imageInfo
    };

    m_context.GetDevice().updateDescriptorSets(write, {});
}