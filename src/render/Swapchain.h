#pragma once

#include "core/VulkanIncludes.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

// Forward declarations
class VulkanContext;


/**
 * @brief Manages the Vulkan swapchain and all per-frame synchronization primitives.
 *
 * Swapchain owns the swapchain object, its images and image views, and the
 * per-frame semaphores and fences required for double-buffered rendering.
 * It also handles swapchain recreation when the window is resized.
 *
 * Swapchain must be recreated (via Recreate()) whenever the surface becomes
 * incompatible with the current swapchain (e.g. window resize).
 */
class Swapchain
{
public:

    Swapchain(VulkanContext& context, GLFWwindow* window);
    ~Swapchain() = default;

    // Non-copyable, non-movable
    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&)                 = delete;
    Swapchain& operator=(Swapchain&&)      = delete;


    // ----------------------------------------------
    // FRAME LIFECYCLE
    // ----------------------------------------------

    /**
     * @brief Acquires the next available swapchain image for rendering.
     *
     * Signals imageAvailableSemaphore for the current frame when the image
     * is ready. Returns the acquired image index via outImageIndex.
     *
     * @param outImageIndex  Receives the index of the acquired swapchain image.
     * @return               vk::Result of the acquire operation.
     *                       eErrorOutOfDateKHR signals the caller to recreate the swapchain.
     */
    vk::Result AcquireNextImage(uint32_t& outImageIndex);

    /**
     * @brief Presents the rendered image to the display.
     *
     * Waits on renderFinishedSemaphore for the current frame before presenting.
     *
     * @param imageIndex  Index of the swapchain image to present.
     * @return            vk::Result of the present operation.
     *                    eErrorOutOfDateKHR or eSuboptimalKHR signal the caller to recreate.
     */
    vk::Result Present(uint32_t imageIndex);

    /**
     * @brief Advances the current frame index to the next slot.
     *
     * Must be called at the end of each rendered frame.
     */
    void AdvanceFrame() { m_currentFrame = (m_currentFrame + 1) % k_maxFramesInFlight; }

    /**
     * @brief Destroys and recreates the swapchain for the current window size.
     *
     * Called when AcquireNextImage or Present return eErrorOutOfDateKHR,
     * or when a framebuffer resize event is detected.
     */
    void Recreate();


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    vk::Format              GetFormat()       const { return m_format; }
    vk::Extent2D            GetExtent()       const { return m_extent; }
    uint32_t                GetImageCount()   const { return static_cast<uint32_t>(m_imageViews.size()); }
    uint32_t                GetCurrentFrame() const { return m_currentFrame; }

    /// Returns the image view for a given swapchain image index.
    vk::ImageView           GetImageView(uint32_t index) const { return *m_imageViews[index]; }

    /// Returns the swapchain image for a given index (needed for layout transitions).
    vk::Image               GetImage(uint32_t index)     const { return m_images[index]; }

    /// Semaphore signaled when the acquired image is ready for rendering (per frame-in-flight slot).
    vk::Semaphore           GetImageAvailableSemaphore() const { return *m_presentCompleteSemaphores[m_currentFrame]; }

    /// Semaphore signaled when rendering is complete and the image is ready for presentation
    /// (per swapchain image — must be indexed by the acquired image index, not the frame slot).
    vk::Semaphore           GetRenderFinishedSemaphore(uint32_t imageIndex) const { return *m_renderFinishedSemaphores[imageIndex]; }

    /// Fence used to prevent CPU from outrunning GPU for the current frame slot.
    vk::Fence               GetInFlightFence()           const { return *m_inFlightFences[m_currentFrame]; }


private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    // The Swap Chain is where the final images appear. If we have double buffering, the swap chain will contain the visible image and the one being drawn.
    // In each frame, we get the next free image from the swap chain, render there and present it
    // This exists because GPU and monitor work at different velocities
    void CreateSwapchain();
    
    // Defines how to interpret the Swap Chain images
    // Creates an ImageView for each Image and prepares everything to render on them
    // For Vulkan, it is not enough having the memory, it needs to know how to read it
    void CreateImageViews();

    // This function creates the objects that allow us to synchronize CPU, GPU and presentation
    // Semaphores. Signals GPU -> GPU
    // Fences. CPU <-> GPU Synchronization
    // 
    // Complete flow:
    // CPU -> wait fence (wait for GPU) -> acquire image -> presentCompleteSemaphore -> record + submit command buffer -> 
    // -> renderFinishedSemaphore -> present -> GPU finished, fence signaled
    // Idempotent: clears and re-creates all sync primitives. Safe to call from Recreate()
    // since renderFinished semaphores are per swapchain image and the image count may change.
    void CreateSyncObjects();

    void DestroySwapchainResources();

    vk::SurfaceFormatKHR    ChooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const;
    vk::PresentModeKHR      ChoosePresentMode(const std::vector<vk::PresentModeKHR>& modes) const;
    vk::Extent2D            ChooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;
    uint32_t                ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities) const;


    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&                          m_context;
    GLFWwindow*                             m_window;

    vk::raii::SwapchainKHR                  m_swapchain     = nullptr;
    
    // Owned by the swapchain, not RAII-wrapped
    std::vector<vk::Image>                  m_images;

    std::vector<vk::raii::ImageView>        m_imageViews;
    vk::Format                              m_format        = vk::Format::eUndefined;
    vk::Extent2D                            m_extent        = {};
    uint32_t                                m_currentFrame  = 0;

    // Synchronization.
    // presentComplete and inFlightFences are per frame-in-flight (MAX_FRAMES_IN_FLIGHT entries).
    // renderFinished is per swapchain image (m_images.size() entries), because a submit's signal
    // semaphore may still be pending on the previous present of that same image; only re-acquiring
    // the image guarantees the semaphore is safe to reuse.
    std::vector<vk::raii::Semaphore>        m_presentCompleteSemaphores;   // Per frame-in-flight: signaled by acquire, waited on by submit.
    std::vector<vk::raii::Semaphore>        m_renderFinishedSemaphores;    // Per swapchain image:  signaled by submit,  waited on by present.
    std::vector<vk::raii::Fence>            m_inFlightFences;              // Per frame-in-flight: CPU gate for the slot's previous submission.
};
