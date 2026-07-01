#include "Swapchain.h"
#include "VulkanContext.h"

#include <stdexcept>
#include <algorithm>
#include <limits>

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

Swapchain::Swapchain(VulkanContext& context, GLFWwindow* window) : 
    m_context(context),
    m_window(window)
{
    CreateSwapchain();
    CreateImageViews();
    CreateSyncObjects();
}


// -----------------------------------------------------------------------
// Frame lifecycle
// -----------------------------------------------------------------------

vk::Result Swapchain::AcquireNextImage(uint32_t& outImageIndex)
{
    const vk::raii::Device& device = m_context.GetDevice();

    // Wait until the GPU has finished with this frame slot
    vk::Result waitResult = device.waitForFences(
        GetInFlightFence(),
        VK_TRUE,
        UINT64_MAX);

    if (waitResult != vk::Result::eSuccess)
        throw std::runtime_error("waitForFences failed");

    auto [result, imageIndex] = m_swapchain.acquireNextImage(
        UINT64_MAX,
        GetImageAvailableSemaphore(),
        nullptr);

    if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR)
    {
        // Only reset the fence if we successfully acquired an image —
        // otherwise we would deadlock on the next wait.
        device.resetFences(GetInFlightFence());
        outImageIndex = imageIndex;
    }

    return result;
}

vk::Result Swapchain::Present(uint32_t imageIndex)
{
    vk::Semaphore waitSemaphores[] = { GetRenderFinishedSemaphore(imageIndex) };

    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphoreCount(1)
        .setWaitSemaphores(waitSemaphores)
        .setSwapchainCount(1)
        .setSwapchains(*m_swapchain)
        .setImageIndices(imageIndex);

    // presentKHR may throw on eErrorOutOfDateKHR depending on the hpp dispatch;
    // catch it and return the result code so the caller can decide.
    try
    {
        return m_context.GetGraphicsQueue().presentKHR(presentInfo);
    }
    catch (const vk::OutOfDateKHRError&)
    {
        return vk::Result::eErrorOutOfDateKHR;
    }
}

void Swapchain::Recreate()
{
    // Handle minimization — wait until the window has a non-zero size
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    // Finish all GPU work before destroying any swapchain resources
    m_context.GetDevice().waitIdle();

    DestroySwapchainResources();

    CreateSwapchain();
    CreateImageViews();
    CreateSyncObjects();  // renderFinished semaphores are per-image; recreate to match the new image count.
}


void Swapchain::CreateSwapchain()
{
    const vk::raii::PhysicalDevice& physDevice = m_context.GetPhysicalDevice();
    const vk::raii::SurfaceKHR& surface = m_context.GetSurface();

    const auto capabilities  = physDevice.getSurfaceCapabilitiesKHR(surface);
    const auto formats       = physDevice.getSurfaceFormatsKHR(surface);
    const auto presentModes  = physDevice.getSurfacePresentModesKHR(surface);

    const vk::SurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
    const vk::PresentModeKHR   presentMode   = ChoosePresentMode(presentModes);
    const vk::Extent2D         extent        = ChooseExtent(capabilities);
    const uint32_t             imageCount    = ChooseSwapMinImageCount(capabilities);

    // Define the Swap Chain with the collected information
    vk::SwapchainCreateInfoKHR swapChainCreateInfo
    {
        .surface            = *surface,
        .minImageCount      = imageCount,
        .imageFormat        = surfaceFormat.format,
        .imageColorSpace    = surfaceFormat.colorSpace,
        .imageExtent        = extent,
        .imageArrayLayers   = 1,
        .imageUsage         = vk::ImageUsageFlagBits::eColorAttachment,	 // This images will be render targets
        .imageSharingMode   = vk::SharingMode::eExclusive,
        .preTransform       = capabilities.currentTransform,
        .compositeAlpha     = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode        = presentMode,
        .clipped            = true
    };

    // Swap Chain creation
    m_swapchain = vk::raii::SwapchainKHR(m_context.GetDevice(), swapChainCreateInfo);
    m_images    = m_swapchain.getImages();
    m_format    = surfaceFormat.format;
    m_extent    = extent;
}

void Swapchain::CreateImageViews()
{
    // Avoid duplicates
    assert(m_imageViews.empty());

    m_imageViews.clear();
    m_imageViews.reserve(m_images.size());

    for (const vk::Image& image : m_images)
    {
        m_imageViews.push_back(m_context.CreateImageView(image, m_format, vk::ImageAspectFlagBits::eColor, 1)); // single mip level, is a swap chain image
    }
}

void Swapchain::CreateSyncObjects()
{
    m_presentCompleteSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();

    // renderFinishedSemaphore -> Frame render is complete.
    // One per swapchain image: only re-acquiring an image guarantees its previous present
    // has consumed the signal, so binding the semaphore to the image (not the frame slot)
    // is what makes reuse safe.
    for (size_t i = 0; i < m_images.size(); ++i)
    {
        m_renderFinishedSemaphores.emplace_back(m_context.GetDevice(), vk::SemaphoreCreateInfo());
    }

    // presentCompleteSemaphore -> The image is ready to render into (signaled by acquire).
    // inFlightFences                -> CPU gate to know whether the GPU is done with this slot.
    //                                 They start signaled so the first frame doesn't stall.
    for (uint32_t i = 0; i < k_maxFramesInFlight; ++i)
    {
        m_presentCompleteSemaphores.emplace_back(m_context.GetDevice(), vk::SemaphoreCreateInfo());
        m_inFlightFences.emplace_back(m_context.GetDevice(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
    }
}

void Swapchain::DestroySwapchainResources()
{
    m_imageViews.clear();
    m_images.clear();
    m_swapchain = nullptr;
}

vk::SurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const
{
    // Receives the list of formats that GPU + OS support
    // Look for preferred format: vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear
    //		8 bits per channel, Blue, Green, Red, Alpha gamma corrected(sRGB)
    //		Standard color space for modern monitors
    for (const auto& format : formats)
    {
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return format;
    }

    // If we don't find it, use the first available
    return formats[0];
}

vk::PresentModeKHR Swapchain::ChoosePresentMode(const std::vector<vk::PresentModeKHR>& modes) const
{
    // Check if at least we have FIFO mode, Vulkan ALWAYS have FIFO mode
    assert(std::ranges::any_of(modes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));

    // Choose Mailbox if available (triple buffering) — low latency without tearing
    for (const auto& mode : modes)
    {
        if (mode == vk::PresentModeKHR::eMailbox)
            return mode;
    }

    // If not, return FIFO
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Swapchain::ChooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const
{
    // If the surface dictates a fixed extent, use it (for example, Android)
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    // If not, ask GLFW for framebuffer size
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    // Return clamped extent to fit in surface ranges
    return {
        std::clamp(static_cast<uint32_t>(width),  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width),
        std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

uint32_t Swapchain::ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities) const
{
    // At least we want 3 images in the swap chain (triple buffering)
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);

    // Check systemm's min and max restrictions, reduce number if we requested more than possible
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}
