#include "render/passes/SkyboxPass.h"

#include "core/DescriptorAllocator.h"
#include "core/PipelineBuilder.h"
#include "core/VulkanContext.h"
#include "render/EnvironmentMap.h"
#include "render/RenderTarget.h"
#include "resources/ResourceManager.h"
#include "resources/Shader.h"

#include <stdexcept>

SkyboxPass::SkyboxPass(const std::string& name, VulkanContext& context, ResourceManager& resourceManager, const CreateInfo& info) :
    RenderPass(name),
    m_context(context),
    m_info(info)
{
    const auto shader = resourceManager.LoadResource<Shader>(context, "skybox.slang", vk::ShaderStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment));

    if (!shader)
    {
        throw std::runtime_error("SkyboxPass: failed to load skybox.slang");
    }

    CreateDescriptorSetLayout();
    CreatePipeline(*shader);
    CreateDescriptorPool();
    CreateDescriptorSets();
}

// -----------------------------------------------------------------------------
// Pass execution
// -----------------------------------------------------------------------------

void SkyboxPass::BeginPass(vk::raii::CommandBuffer& cmd, const FrameInfo& frame)
{
    // Transition MSAA color attachment: Undefined -> ColorAttachmentOptimal
    m_context.TransitionImageLayout(cmd,
    {
        .image          = m_info.renderTarget->GetColorImage(),
        .oldLayout      = vk::ImageLayout::eUndefined,
        .srcStageMask   = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask  = vk::AccessFlagBits2::eNone,
        .newLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite
    });

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

    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_context.GetDevice(), layoutInfo);
}

void SkyboxPass::CreatePipeline(const Shader& shader)
{
    auto [layout, pipeline] = PipelineBuilder(m_context)
        .SetShader(shader)
        .SetDescriptorSetLayouts(*m_descriptorSetLayout)
        .SetPushConstants(vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4))
        .SetColorFormat(m_info.renderTarget->GetColorFormat())
        .SetSamples(m_info.renderTarget->GetSamples())
        .Build();

    m_pipelineLayout = std::move(layout);
    m_pipeline       = std::move(pipeline);
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
    std::vector<vk::raii::DescriptorSet> sets = DescriptorAllocator::AllocateSets(m_context, m_descriptorPool, m_descriptorSetLayout, 1);
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