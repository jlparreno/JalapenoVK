#include "render/RenderTarget.h"

#include "core/VulkanContext.h"
#include "core/VulkanTypes.h"

RenderTarget::RenderTarget(VulkanContext& context, vk::Format colorFormat, vk::Extent2D extent) : 
    m_context(context),
    m_extent(extent),
    m_colorFormat(colorFormat)
{
    m_samples     = QueryMaxUsableSampleCount();
    m_depthFormat = FindDepthFormat();

    CreateAttachments();
}

void RenderTarget::OnResize(vk::Extent2D newExtent)
{
    // Release the current attachments first so their memory is freed before we allocate the new ones.
    // Views must go before the images/memory they reference.
    m_colorView   = nullptr;
    m_colorMemory = nullptr;
    m_colorImage  = nullptr;

    m_depthView   = nullptr;
    m_depthMemory = nullptr;
    m_depthImage  = nullptr;

    m_extent = newExtent;
    CreateAttachments();
}

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------

void RenderTarget::CreateAttachments()
{
    // MSAA color attachment
    ImageDescription imageDesc =
    {
        .width = m_extent.width,
        .height = m_extent.height,
        .format = m_colorFormat,
        .usage = vk::ImageUsageFlagBits::eColorAttachment,
        .samples = m_samples,
    };
    
    std::tie(m_colorImage, m_colorMemory) = m_context.CreateImage(imageDesc);

    ImageViewDescription viewDesc =
    {
        .image = m_colorImage, 
        .format = m_colorFormat
    };

    m_colorView = m_context.CreateImageView(viewDesc);

    // Depth attachment
    ImageDescription depthImageDesc =
    {
        .width = m_extent.width,
        .height = m_extent.height,
        .format = m_depthFormat,
        .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
        .samples = m_samples,
    };

    std::tie(m_depthImage, m_depthMemory) = m_context.CreateImage(depthImageDesc);

    ImageViewDescription depthViewDesc =
    {
        .image = m_depthImage,
        .format = m_depthFormat,
        .aspect = vk::ImageAspectFlagBits::eDepth
    };

    m_depthView = m_context.CreateImageView(depthViewDesc);
}

vk::SampleCountFlagBits RenderTarget::QueryMaxUsableSampleCount() const
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

vk::Format RenderTarget::FindDepthFormat() const
{
    // Depth-only to keep aspect mask simple (no stencil aspect required in barriers/views).
    return m_context.FindSupportedFormat( { vk::Format::eD32Sfloat }, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}
