#pragma once

#include "core/VulkanIncludes.h"
#include "render/Swapchain.h"
#include "render/RenderTarget.h"
#include "render/passes/RenderPassManager.h"
#include "scene/CameraComponent.h"
#include "scene/CameraControllerComponent.h"
#include "scene/Entity.h"
#include "scene/Scene.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <vector>

// Forward declarations
class VulkanContext;
class ResourceManager;
class GeometryPass;
class SkyboxPass;
class EnvironmentMap;
class ImageBasedLighting;

/**
 * @brief High-level per-frame orchestrator that drives acquire/render/present.
 *
 * Renderer owns the swapchain, the render target the passes draw into, the
 * render pass manager, and the per-frame command buffers. Each frame it
 * acquires a swapchain image, records the ordered pass sequence built by the
 * RenderPassManager, submits the command buffer, and presents the result.
 * Scene state is owned externally by a Scene instance, passed in through the CreateInfo.
 *
 * When the surface becomes incompatible with the swapchain (window resize),
 * the Renderer coordinates a full swapchain recreation and resizes the render
 * target to match, so the passes never deal with size changes themselves.
 */
class Renderer
{
public:

    /**
     * @brief Everything the renderer is built with, apart from the context and the resource manager.
     *
     * Only the scene is kept by the renderer. The rest is handed to the passes during construction.
     */
    struct CreateInfo
    {
        GLFWwindow*             window;                 // Window providing the presentation surface.
        Scene*                  scene;                  // Scene rendered every frame. Must outlive the renderer.
        vk::DescriptorSetLayout pbrMaterialLayout;      // Shared PBR material descriptor set layout (set 1), declared by GeometryPass's pipeline layout.
        EnvironmentMap*         environmentMap;         // Environment cube sampled by SkyboxPass. Must outlive the renderer.
        ImageBasedLighting*     imageBasedLighting;     // IBL resources whose set 2 GeometryPass binds. Must outlive the renderer.
    };

    /**
     * @brief Construct the renderer and initialise its owned subsystems.
     *
     * Builds the swapchain, allocates per-frame command buffers, and configures
     * the initial render pass list.
     *
     * @param context          Vulkan backend used for all GPU allocations.
     * @param resourceManager  Registry the passes load their shaders through. Used only here, not kept.
     * @param info             Window, scene, and what the passes are built with.
     */
    Renderer(VulkanContext& context, ResourceManager& resourceManager, const CreateInfo& info);

    /**
     * @brief Destructor. Owned vk::raii handles release themselves.
     */
    ~Renderer() = default;

    // Non-copyable, non-movable (owns vk::raii handles)
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;


    // ----------------------------------------------
    // RENDER INTERFACE
    // ----------------------------------------------

    /**
     * @brief Draws a single frame using the current pass sequence.
     *
     * Acquires the next swapchain image, records and submits the command buffer
     * for every enabled pass, and presents the result. Handles eErrorOutOfDateKHR
     * transparently by recreating the swapchain and skipping the frame.
     */
    void Render();

    /**
     * @brief Blocks the calling thread until the GPU has finished all outstanding work.
     *
     * Required before destroying any GPU resource still in use: during shutdown or before a swapchain recreation.
     */
    void WaitIdle() const;

    /**
     * @brief Marks the framebuffer as resized so the next frame triggers swapchain recreation.
     */
    void OnFramebufferResized() { m_framebufferResized = true; }


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns a reference to the owned swapchain.
     */
    Swapchain& GetSwapchain() { return m_swapchain; }


private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Instantiates and registers the concrete render passes for this renderer.
     *
     * @param resourceManager  Registry handed to the passes that load their own resources.
     * @param info             What the passes are built with.
     */
    void SetupRenderPasses(ResourceManager& resourceManager, const CreateInfo& info);

    /**
     * @brief Allocates one primary command buffer per frame-in-flight slot.
     */
    void CreateCommandBuffers();

    /**
     * @brief Records the full pass sequence for the given swapchain image into a command buffer.
     *
     * @param commandBuffer  Command buffer to record into. Assumed to be reset.
     * @param imageIndex     Index of the acquired swapchain image being rendered to.
     */
    void RecordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    /**
     * @brief Submits a recorded command buffer with the correct per-frame semaphore/fence sync.
     *
     * @param commandBuffer  Command buffer to submit.
     * @param imageIndex     Index of the swapchain image the command buffer targets.
     */
    void SubmitCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    /**
     * @brief Recreates the swapchain and resizes the render target to match.
     *
     * Called after eErrorOutOfDateKHR or a framebuffer resize. Waits for the
     * device to become idle before rebuilding the swapchain to avoid touching
     * in-flight resources.
     */
    void HandleSwapchainRecreation();


    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&                          m_context;                          // Vulkan backend used for GPU allocations. Not owned by this class.
    Scene&                                  m_scene;                            // Scene supplying the entities, active camera, and active light to render. Not owned by this class.

    Swapchain                               m_swapchain;                        // Owned swapchain and its per-frame synchronization primitives.
    RenderTarget                            m_renderTarget;                     // Owned MSAA color + depth attachments every pass renders into.
    RenderPassManager                       m_renderPassManager;                // Owned manager that orders and executes the pass sequence each frame.

    std::vector<vk::raii::CommandBuffer>    m_commandBuffers;                   // One primary command buffer per frame-in-flight slot.

    SkyboxPass*                             m_skyboxPass        { nullptr };    // Non-owning pointer, so Renderer can push per-frame data.
    GeometryPass*                           m_geometryPass      { nullptr };    // Non-owning pointer, so Renderer can push per-frame data.

    bool                                    m_framebufferResized{ false };      // Flag to trigger swapchain recreation on the next frame.
};
