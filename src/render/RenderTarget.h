#pragma once

#include "core/VulkanIncludes.h"

// Forward declarations
class VulkanContext;

/**
 * @brief Shared destination the render passes draw into: an MSAA color attachment plus its depth buffer.
 *
 * Owns the offscreen attachments a frame is rendered to, so that several
 * passes can share a single set of attachments instead of each allocating its
 * own. The color attachment is multisampled; resolving it into the current
 * swapchain image is done by the pass that writes to it last, this class only
 * owns the images.
 *
 * Sized to match the swapchain, so the Renderer must call OnResize() every time
 * the swapchain is recreated.
 */
class RenderTarget
{
public:

    /**
     * @brief Creates the color and depth attachments at the given size.
     *
     * Queries the device for the maximum usable sample count and picks a supported
     * depth format, then allocates both attachments from them.
     *
     * @param context     The Vulkan context used for all GPU resource operations.
     * @param colorFormat Format of the color attachment. Must match the swapchain's, since the color attachment is resolved into it.
     * @param extent      Initial size of both attachments. Should match the swapchain extent.
     */
    RenderTarget(VulkanContext& context, vk::Format colorFormat, vk::Extent2D extent);

    /**
     * @brief Destructor. Owned vk::raii handles release themselves.
     */
    ~RenderTarget() = default;

    // Non-copyable / non-movable (owns vk::raii handles)
    RenderTarget(const RenderTarget&)            = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&&)                 = delete;
    RenderTarget& operator=(RenderTarget&&)      = delete;

    /**
     * @brief Recreate the attachments at a new size.
     *
     * Called by the Renderer after the swapchain has been recreated. Assumes the
     * caller has already ensured the GPU is idle (Swapchain::Recreate() calls
     * waitIdle() internally, so this is safe to invoke right after).
     */
    void OnResize(vk::Extent2D newExtent);

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the multisampled color image, for the layout transitions a pass records before rendering.
     */
    vk::Image       GetColorImage() const { return m_colorImage; }

    /**
     * @brief Returns the view a pass binds as its color attachment.
     */
    vk::ImageView   GetColorView()  const { return m_colorView; }

    /**
     * @brief Returns the depth image, for the layout transitions a pass records before rendering.
     */
    vk::Image       GetDepthImage() const { return m_depthImage; }

    /**
     * @brief Returns the view a pass binds as its depth attachment.
     */
    vk::ImageView   GetDepthView()  const { return m_depthView; }

    /**
     * @brief Returns the sample count attachments were created with.
     *
     * A pipeline rendering into this target must declare the exact same count.
     */
    vk::SampleCountFlagBits GetSamples()     const { return m_samples; }

    /**
     * @brief Returns the color attachment format (matches the swapchain's).
     */
    vk::Format GetColorFormat() const { return m_colorFormat; }

    /**
     * @brief Returns the depth format picked from what the device supports.
     */
    vk::Format GetDepthFormat() const { return m_depthFormat; }

    /**
     * @brief Returns the current size of both attachments.
     */
    vk::Extent2D GetExtent()      const { return m_extent; }

private:

    // ----------------------------------------------
    // INITIALIZATION HELPERS
    // ----------------------------------------------

    /**
     * @brief Allocates the color and depth attachments at the current extent.
     *
     * Creates each image with its backing memory and its view, using the sample
     * count and formats resolved at construction. Assumes any previously created
     * attachments have already been released (see OnResize()).
     */
    void CreateAttachments();

    /**
     * @brief Returns the highest sample count the device supports for both color and depth attachments.
     *
     * Intersects the two limits reported by the physical device and walks down
     * from 64x, so the result is always usable by both attachments at once.
     */
    vk::SampleCountFlagBits QueryMaxUsableSampleCount() const;

    /**
     * @brief Picks a depth format the device supports for depth/stencil attachments.
     */
    vk::Format FindDepthFormat() const;

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Non-owned references / config
    VulkanContext&                  m_context;    // Vulkan context used for all GPU operations. Not owned by this class.

    // Supplied at construction, kept so the attachments can be rebuilt on resize
    vk::Extent2D                    m_extent;                                     // Current size of both attachments. Tracks the swapchain extent.
    vk::Format                      m_colorFormat { vk::Format::eUndefined };     // Color attachment format. Matches the swapchain's, since the color is resolved into it.

    // Derived at construction from what the device supports
    vk::Format                      m_depthFormat { vk::Format::eUndefined };     // Depth-only format, no stencil aspect.
    vk::SampleCountFlagBits         m_samples     { vk::SampleCountFlagBits::e1 }; // Highest count the device supports for both color and depth.

    // Attachments (MSAA color + depth). Rendered into, then color is resolved to swapchain.
    vk::raii::Image                 m_colorImage  { nullptr };
    vk::raii::DeviceMemory          m_colorMemory { nullptr };
    vk::raii::ImageView             m_colorView   { nullptr };

    vk::raii::Image                 m_depthImage  { nullptr };
    vk::raii::DeviceMemory          m_depthMemory { nullptr };
    vk::raii::ImageView             m_depthView   { nullptr };
};
