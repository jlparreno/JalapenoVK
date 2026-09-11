#include "render/ImageBasedLighting.h"

#include "core/DescriptorAllocator.h"
#include "core/PipelineBuilder.h"
#include "core/VulkanContext.h"
#include "core/VulkanTypes.h"
#include "render/EnvironmentMap.h"
#include "render/RenderTypes.h"
#include "resources/ResourceManager.h"
#include "resources/Shader.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

ImageBasedLighting::ImageBasedLighting(VulkanContext& context, ResourceManager& resourceManager, const EnvironmentMap& environment) :
    m_context(context)
{
    const vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    const auto irradianceShader = resourceManager.LoadResource<Shader>(context, "irradiance_convolution.slang", stages);
    const auto prefilterShader  = resourceManager.LoadResource<Shader>(context, "prefilter_specular.slang", stages);
    const auto brdfLutShader    = resourceManager.LoadResource<Shader>(context, "brdf_lut.slang", stages);

    if (!irradianceShader || !prefilterShader || !brdfLutShader)
    {
        throw std::runtime_error("ImageBasedLighting: failed to load its generation shaders");
    }

    // Create class resources
    CreateIrradianceCube();
    CreatePrefilterCube();
    CreateBrdfLut();
    CreateSamplers();

    // Generate the lighting information on resources
    ConvolveIrradiance(environment, *irradianceShader);
    PrefilterSpecular(environment, *prefilterShader);
    IntegrateBrdfLut(*brdfLutShader);

    // Create DescriptorSet 2, pointing at the three resources just generated
    CreateDescriptorSetLayout();
    CreateDescriptorPool();
    CreateDescriptorSet();
}

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------

void ImageBasedLighting::CreateIrradianceCube()
{
    // Image
    ImageDescription imageDesc =
    {
        .width = k_irradianceSize,
        .height = k_irradianceSize,
        .format = k_cubeFormat,
        .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .arrayLayers = 6,
        .flags = vk::ImageCreateFlagBits::eCubeCompatible
    };

    std::tie(m_irradianceImage, m_irradianceMemory) = m_context.CreateImage(imageDesc);

    // Image view. Covers all six layers at once.
    ImageViewDescription viewDesc =
    {
        .image = m_irradianceImage,
        .format = k_cubeFormat,
        .viewType = vk::ImageViewType::eCube,
        .layerCount = 6
    };

    m_irradianceView = m_context.CreateImageView(viewDesc);
}

void ImageBasedLighting::CreatePrefilterCube()
{
    // Image
    ImageDescription imageDesc =
    {
        .width = k_prefilterSize,
        .height = k_prefilterSize,
        .format = k_cubeFormat,
        .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .mipLevels = k_prefilterMips,
        .arrayLayers = 6,
        .flags = vk::ImageCreateFlagBits::eCubeCompatible
    };

    std::tie(m_prefilterImage, m_prefilterMemory) = m_context.CreateImage(imageDesc);

    // Image view. Covers all six layers at once.
    ImageViewDescription viewDesc =
    {
        .image = m_prefilterImage,
        .format = k_cubeFormat,
        .viewType = vk::ImageViewType::eCube,
        .levelCount = k_prefilterMips,
        .layerCount = 6
    };

    m_prefilterView = m_context.CreateImageView(viewDesc);
}

void ImageBasedLighting::CreateBrdfLut()
{
    // Image
    ImageDescription imageDesc =
    {
        .width  = k_brdfLutSize,
        .height = k_brdfLutSize,
        .format = k_brdfLutFormat,
        .usage  = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
    };

    std::tie(m_brdfLutImage, m_brdfLutMemory) = m_context.CreateImage(imageDesc);

    // Image view
    ImageViewDescription viewDesc =
    {
        .image    = m_brdfLutImage,
        .format   = k_brdfLutFormat,
        .viewType = vk::ImageViewType::e2D
    };

    m_brdfLutView = m_context.CreateImageView(viewDesc);
}

void ImageBasedLighting::CreateSamplers()
{
    // Shared configuration. No anisotropy, and ClampToEdge address mode.
    // The samplers differ only in maxLod.
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

    // The irradiance cube has a single level: there is nothing to filter between.
    m_irradianceSampler = vk::raii::Sampler(m_context.GetDevice(), samplerInfo);

    // The BRDF LUT is a single-level 2D table. Same configuration as the irradiance today
    m_brdfLutSampler = vk::raii::Sampler(m_context.GetDevice(), samplerInfo);

    // The prefiltered cube holds one roughness per level
    samplerInfo.maxLod = k_prefilterMips;

    m_prefilterSampler = vk::raii::Sampler(m_context.GetDevice(), samplerInfo);
}

void ImageBasedLighting::CreateDescriptorSetLayout()
{
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings
    {
        {
            {.binding = 0, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment},
            {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment},
            {.binding = 2, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}
        }
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_context.GetDevice(), layoutInfo);
}

void ImageBasedLighting::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 3 }
    };

    m_descriptorPool = DescriptorAllocator::CreatePool(m_context, 1, poolSizes);
}

void ImageBasedLighting::CreateDescriptorSet()
{
    std::vector<vk::raii::DescriptorSet> sets = DescriptorAllocator::AllocateSets(m_context, m_descriptorPool, m_descriptorSetLayout, 1);
    m_descriptorSet = std::move(sets[0]);

    // Irradiance
    vk::DescriptorImageInfo irradianceInfo
    {
        .sampler = m_irradianceSampler,
        .imageView = m_irradianceView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    std::array<vk::WriteDescriptorSet, 3> writes;
    writes[0] =
    {
        .dstSet = m_descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &irradianceInfo
    };

    // Prefilter
    vk::DescriptorImageInfo prefilterInfo
    {
        .sampler = m_prefilterSampler,
        .imageView = m_prefilterView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    writes[1] =
    {
        .dstSet = m_descriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &prefilterInfo
    };

    // BRDF LUT
    vk::DescriptorImageInfo lutInfo
    {
        .sampler = m_brdfLutSampler,
        .imageView = m_brdfLutView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    writes[2] =
    {
        .dstSet = m_descriptorSet,
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &lutInfo
    };

    m_context.GetDevice().updateDescriptorSets(writes, {});
}

void ImageBasedLighting::ConvolveIrradiance(const EnvironmentMap& environment, const Shader& shader)
{
    // The source environment cube, which EnvironmentMap leaves in eShaderReadOnlyOptimal with its mip chain filled.
    const auto descriptor = DescriptorAllocator::CreateSingleImageDescriptor(m_context, environment.GetEnvironmentSampler(), environment.GetEnvironmentView());

    // Create the graphics pipeline driving the convolution draws.
    auto [pipelineLayout, pipeline] = PipelineBuilder(m_context)
        .SetShader(shader)
        .SetDescriptorSetLayouts(*descriptor.layout)
        .SetPushConstants(vk::ShaderStageFlagBits::eFragment, sizeof(CubeFaceOrientation))
        .SetColorFormat(k_cubeFormat)
        .Build();

    // Start command recording
    const auto commandBuffer = m_context.BeginSingleTimeCommands();

    // Whole cube (all six layers): Undefined -> ColorAttachmentOptimal, ready to be rendered into.
    m_context.TransitionImageLayout(*commandBuffer,
    {
        .image          = m_irradianceImage,
        .oldLayout      = vk::ImageLayout::eUndefined,
        .srcStageMask   = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask  = vk::AccessFlagBits2::eNone,
        .newLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite,
        .layerCount     = 6
    });

    // The per-face views must stay alive until the submit completes
    std::vector<vk::raii::ImageView> faceViews;
    faceViews.reserve(6);

    const vk::Extent2D faceExtent{ k_irradianceSize, k_irradianceSize };

    for (uint32_t face = 0; face < 6; face++)
    {
        // Single-layer 2D view of one cube layer:
        // the cube is sampled as a cube, but rendered into one face at a time.
        ImageViewDescription faceViewDesc =
        {
            .image = m_irradianceImage,
            .format = k_cubeFormat,
            .viewType = vk::ImageViewType::e2D,
            .baseArrayLayer = face,
            .layerCount = 1
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

        // Bind descriptor with the environment cube
        commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptor.set, nullptr);

        // Which face this draw is painting through push constant
        commandBuffer->pushConstants<CubeFaceOrientation>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, k_cubeFacesOrientation[face]);

        // Three vertices, no vertex buffer: the vertex stage builds the fullscreen triangle from SV_VertexID.
        commandBuffer->draw(3, 1, 0, 0);

        commandBuffer->endRendering();
    }

    // Whole cube: ColorAttachmentOptimal -> ShaderReadOnlyOptimal, ready to sample.
    m_context.TransitionImageLayout(*commandBuffer,
    {
        .image          = m_irradianceImage,
        .oldLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .srcStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite,
        .newLayout      = vk::ImageLayout::eShaderReadOnlyOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask  = vk::AccessFlagBits2::eShaderRead,
        .layerCount     = 6
    });

    m_context.EndSingleTimeCommands(*commandBuffer);
}

void ImageBasedLighting::PrefilterSpecular(const EnvironmentMap& environment, const Shader& shader)
{
    // The source environment cube, which EnvironmentMap leaves in eShaderReadOnlyOptimal with its mip chain filled.
    const auto descriptor = DescriptorAllocator::CreateSingleImageDescriptor(m_context, environment.GetEnvironmentSampler(), environment.GetEnvironmentView());

    // Create the graphics pipeline driving the prefilter draws.
    auto [pipelineLayout, pipeline] = PipelineBuilder(m_context)
        .SetShader(shader)
        .SetDescriptorSetLayouts(*descriptor.layout)
        .SetPushConstants(vk::ShaderStageFlagBits::eFragment, sizeof(CubeFaceOrientation) + sizeof(float)) // The face orientation plus the roughness of the level being painted, right after it.
        .SetColorFormat(k_cubeFormat)
        .Build();

    // Start command recording
    const auto commandBuffer = m_context.BeginSingleTimeCommands();

    // Whole cube (all six layers, all mips): Undefined -> ColorAttachmentOptimal, ready to be rendered into.
    m_context.TransitionImageLayout(*commandBuffer,
    {
        .image          = m_prefilterImage,
        .oldLayout      = vk::ImageLayout::eUndefined,
        .srcStageMask   = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask  = vk::AccessFlagBits2::eNone,
        .newLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite,
        .levelCount     = k_prefilterMips,
        .layerCount     = 6
    });

    // The per-face views must stay alive until the submit completes
    std::vector<vk::raii::ImageView> faceViews;
    faceViews.reserve(30); // 6 faces x 5 mip levels each

    for (uint32_t mip = 0; mip < k_prefilterMips; mip++)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            const uint32_t      mipSize = k_prefilterSize >> mip;
            const vk::Extent2D  faceExtent{ mipSize, mipSize };

            // Single-layer 2D view of one cube layer, at a mip level
            ImageViewDescription faceViewDesc =
            {
                .image = m_prefilterImage,
                .format = k_cubeFormat,
                .viewType = vk::ImageViewType::e2D,
                .baseMipLevel = mip,
                .levelCount = 1,
                .baseArrayLayer = face,
                .layerCount = 1
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
            commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptor.set, nullptr);

            // Which face this draw is painting through push constant
            commandBuffer->pushConstants<CubeFaceOrientation>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, k_cubeFacesOrientation[face]);

            // Roughness depending on mip level through push constant
            const float roughness = float(mip) / float(k_prefilterMips - 1);
            commandBuffer->pushConstants<float>(*pipelineLayout, vk::ShaderStageFlagBits::eFragment, sizeof(CubeFaceOrientation), roughness);

            // Three vertices, no vertex buffer: the vertex stage builds the fullscreen triangle from SV_VertexID.
            commandBuffer->draw(3, 1, 0, 0);

            commandBuffer->endRendering();
        }
    }

    // Whole cube: ColorAttachmentOptimal -> ShaderReadOnlyOptimal, ready to sample.
    m_context.TransitionImageLayout(*commandBuffer,
    {
        .image          = m_prefilterImage,
        .oldLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .srcStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite,
        .newLayout      = vk::ImageLayout::eShaderReadOnlyOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask  = vk::AccessFlagBits2::eShaderRead,
        .levelCount     = k_prefilterMips,
        .layerCount     = 6
    });

    m_context.EndSingleTimeCommands(*commandBuffer);
}

void ImageBasedLighting::IntegrateBrdfLut(const Shader& shader)
{
    // No descriptor layout, pool or set here. Directly create the pipeline
    auto [pipelineLayout, pipeline] = PipelineBuilder(m_context)
        .SetShader(shader)
        .SetColorFormat(k_brdfLutFormat)
        .Build();

    // Start command recording
    const auto commandBuffer = m_context.BeginSingleTimeCommands();

    // Undefined -> ColorAttachmentOptimal, ready to be rendered into. One level and one layer
    m_context.TransitionImageLayout(*commandBuffer,
    {
        .image          = m_brdfLutImage,
        .oldLayout      = vk::ImageLayout::eUndefined,
        .srcStageMask   = vk::PipelineStageFlagBits2::eNone,
        .srcAccessMask  = vk::AccessFlagBits2::eNone,
        .newLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite
    });

    const vk::Extent2D extent{ k_brdfLutSize, k_brdfLutSize };

    // Configure color attachment, no depth attachment
    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.setImageView(*m_brdfLutView)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }));

    vk::RenderingInfo renderingInfo;
    renderingInfo.setRenderArea(vk::Rect2D({ 0, 0 }, extent))
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&colorAttachment);

    // A single draw: no faces to walk and no levels to walk.
    commandBuffer->beginRendering(renderingInfo);

    commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    // Viewport/scissor are dynamic state, so they must be set
    commandBuffer->setViewport(0, vk::Viewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f });
    commandBuffer->setScissor(0, vk::Rect2D{ vk::Offset2D{ 0, 0 }, extent });

    // Neither descriptor sets nor push constants: the shader derives the pair it is
    // solving for from the fragment's own position.

    // Three vertices, no vertex buffer: the vertex stage builds the fullscreen triangle from SV_VertexID.
    commandBuffer->draw(3, 1, 0, 0);

    commandBuffer->endRendering();

    // ColorAttachmentOptimal -> ShaderReadOnlyOptimal, ready to sample.
    m_context.TransitionImageLayout(*commandBuffer,
    {
        .image          = m_brdfLutImage,
        .oldLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .srcStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite,
        .newLayout      = vk::ImageLayout::eShaderReadOnlyOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask  = vk::AccessFlagBits2::eShaderRead
    });

    m_context.EndSingleTimeCommands(*commandBuffer);
}