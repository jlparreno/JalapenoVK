#include "render/EnvironmentMap.h"

#include "core/DescriptorAllocator.h"
#include "core/PipelineBuilder.h"
#include "core/VulkanContext.h"
#include "core/VulkanTypes.h"
#include "resources/Shader.h"
#include "resources/Texture.h"

#include <vector>

EnvironmentMap::EnvironmentMap(VulkanContext& context, const CreateInfo& info) :
    m_context(context)
{
    CreateEnvironmentCube();
    CreateSamplers();

    ProjectEquirectToCube(*info.equirect, *info.shader);
}

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------

void EnvironmentMap::CreateEnvironmentCube()
{
    // Image
    ImageDescription imageDesc =
    {
        .width       = k_environmentSize,
        .height      = k_environmentSize,
        .format      = k_environmentFormat,
        .usage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .arrayLayers = 6,
        .flags       = vk::ImageCreateFlagBits::eCubeCompatible
    };

    std::tie(m_environmentImage, m_environmentMemory) = m_context.CreateImage(imageDesc);

    // Image view. Covers all six layers at once.
    ImageViewDescription viewDesc =
    {
        .image      = m_environmentImage,
        .format     = k_environmentFormat,
        .viewType   = vk::ImageViewType::eCube,
        .layerCount = 6
    };

    m_environmentView = m_context.CreateImageView(viewDesc);
}

void EnvironmentMap::CreateSamplers()
{
    // Environment sampler. No anisotropy and no mips: the cube has a single level, 
    // and cube sampling is seamless regardless of the addressing mode.
    vk::SamplerCreateInfo samplerInfo
    {
        .magFilter        = vk::Filter::eLinear,
        .minFilter        = vk::Filter::eLinear,
        .mipmapMode       = vk::SamplerMipmapMode::eLinear,
        .addressModeU     = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV     = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW     = vk::SamplerAddressMode::eClampToEdge,
        .mipLodBias       = 0.0f,
        .anisotropyEnable = vk::False,
        .compareEnable    = vk::False,
        .compareOp        = vk::CompareOp::eAlways,
        .minLod           = 0.0f,
        .maxLod           = 0.0f
    };

    m_environmentSampler = vk::raii::Sampler(m_context.GetDevice(), samplerInfo);
}

void EnvironmentMap::ProjectEquirectToCube(const Texture& equirect, const Shader& shader)
{
    // Declaration order is destruction order reversed: the set must go before its
    // pool, and both before the layout they were built from.

    // Set 0, binding 0: the equirectangular source, sampled by the fragment stage.
    std::array<vk::DescriptorSetLayoutBinding, 1> bindings
    {
        {
            {.binding = 0, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}
        }
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };

    vk::raii::DescriptorSetLayout setLayout(m_context.GetDevice(), layoutInfo);

    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        { .type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1 }
    };

    vk::raii::DescriptorPool descriptorPool = DescriptorAllocator::CreatePool(m_context, 1, poolSizes);

    std::vector<vk::raii::DescriptorSet> sets = DescriptorAllocator::AllocateSets(m_context, descriptorPool, setLayout, 1);
    vk::raii::DescriptorSet descriptorSet = std::move(sets[0]);

    // Point the set at the source texture. Texture leaves its images in eShaderReadOnlyOptimal once loaded.
    vk::DescriptorImageInfo imageInfo
    {
        .sampler     = equirect.GetSampler(),
        .imageView   = equirect.GetImageView(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    vk::WriteDescriptorSet write
    {
        .dstSet          = descriptorSet,
        .dstBinding      = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo      = &imageInfo
    };

    m_context.GetDevice().updateDescriptorSets(write, {});

    // Create the graphics pipeline to project the equirect texture to the cube
    auto [pipelineLayout, pipeline] = CreateEquirectToCubePipeline(shader, setLayout);

    // Start command recording
    const auto commandBuffer = m_context.BeginSingleTimeCommands();

    // Whole cube (all six layers): Undefined -> ColorAttachmentOptimal, ready to be rendered into.
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
               .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setOldLayout(vk::ImageLayout::eUndefined)
               .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
               .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
               .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
               .setImage(m_environmentImage)
               .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        commandBuffer->pipelineBarrier2(depInfo);
    }

    // The per-face views must stay alive until the submit completes
    std::vector<vk::raii::ImageView> faceViews;
    faceViews.reserve(6);

    const vk::Extent2D faceExtent{ k_environmentSize, k_environmentSize };

    for (uint32_t face = 0; face < 6; face++)
    {
        // Single-layer 2D view of one cube layer: 
        // the cube is sampled as a cube, but rendered into one face at a time.
        ImageViewDescription faceViewDesc =
        {
            .image          = m_environmentImage,
            .format         = k_environmentFormat,
            .viewType       = vk::ImageViewType::e2D,
            .baseArrayLayer = face,
            .layerCount     = 1
        };

        faceViews.push_back(m_context.CreateImageView(faceViewDesc));

        // Configure color attachment with this faceView, no depth attachment
        vk::RenderingAttachmentInfo colorAttachment;
        colorAttachment.setImageView(*faceViews.back())
                       .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                       .setLoadOp(vk::AttachmentLoadOp::eClear)
                       .setStoreOp(vk::AttachmentStoreOp::eStore)
                       .setClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }));

        vk::RenderingInfo faceRenderingInfo;
        faceRenderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, faceExtent))
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment);

        // Render commands for this face
        commandBuffer->beginRendering(faceRenderingInfo);

        commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

        // Viewport/scissor are dynamic state, so they must be set
        commandBuffer->setViewport(0, vk::Viewport{ 0.0f, 0.0f, static_cast<float>(faceExtent.width), static_cast<float>(faceExtent.height), 0.0f, 1.0f });
        commandBuffer->setScissor(0, vk::Rect2D{ vk::Offset2D{ 0, 0 }, faceExtent });

        // Bind descriptor with the texture image
        commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSet, nullptr);

        // Which face this draw is painting through push constant
        commandBuffer->pushConstants<CubeFaceOrientation>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, k_cubeFacesOrientation[face]);

        // Three vertices, no vertex buffer: the vertex stage builds the fullscreen triangle from SV_VertexID.
        commandBuffer->draw(3, 1, 0, 0);

        commandBuffer->endRendering();
    }

    // Whole cube: ColorAttachmentOptimal -> ShaderReadOnlyOptimal, ready for the skybox and the IBL steps.
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
               .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
               .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
               .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
               .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
               .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
               .setImage(m_environmentImage)
               .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        commandBuffer->pipelineBarrier2(depInfo);
    }

    m_context.EndSingleTimeCommands(*commandBuffer);
}

std::pair<vk::raii::PipelineLayout, vk::raii::Pipeline> EnvironmentMap::CreateEquirectToCubePipeline(const Shader& shader, vk::DescriptorSetLayout setLayout)
{
    // One CubeFaceOrientation, not the whole table: each draw pushes the face it is painting.
    vk::PushConstantRange pushConstantRange;
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eFragment)
                     .setOffset(0)
                     .setSize(sizeof(CubeFaceOrientation));

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo
    {
        .setLayoutCount         = 1,
        .pSetLayouts            = &setLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConstantRange
    };
    vk::raii::PipelineLayout pipelineLayout(m_context.GetDevice(), pipelineLayoutInfo);

    // Configure and create pipeline
    auto pipeline = PipelineBuilder(m_context)
        .SetShader(shader)
        .SetLayout(*pipelineLayout)
        .SetColorFormat(k_environmentFormat)
        .Build();

    return { std::move(pipelineLayout), std::move(pipeline) };
}
