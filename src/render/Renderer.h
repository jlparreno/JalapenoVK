#pragma once

#include "core/VulkanIncludes.h"
#include "render/RenderTypes.h"
#include "render/Swapchain.h"
#include "render/passes/RenderPassManager.h"
#include "resources/ResourceManager.h"
#include "scene/Entity.h"
#include "scene/TransformComponent.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <vector>

// Forward declarations
class VulkanContext;
class Entity;
class GeometryPass;

class Renderer
{
public:

    Renderer(VulkanContext& context, ResourceManager& resourceManager, GLFWwindow* window);

    ~Renderer() = default;

    // ----------------------------------------------
    // FRAME INTERFACE
    // ----------------------------------------------

    void Render(const std::vector<Entity*>& entities);

    void WaitIdle() const;

    void OnFramebufferResized() { m_framebufferResized = true; }

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    Swapchain& GetSwapchain() { return m_swapchain; }


private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    void SetupRenderPasses();

    void SetupEntities();

    void CreateCommandBuffers();

    void RecordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    void SubmitCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex);

    // Recreates the swapchain and propagates the new extent to any pass that owns
    // size-dependent GPU resources (attachments).
    void HandleSwapchainRecreation();


    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&                          m_context;
    GLFWwindow*                             m_window;

    ResourceManager&                        m_resourceManager;
    RenderPassManager                       m_renderPassManager;
    Swapchain                               m_swapchain;

    // One command buffer per frame-in-flight slot
    std::vector<vk::raii::CommandBuffer>    m_commandBuffers;

    bool                                    m_framebufferResized{ false };

    // Non-owning pointer into RenderPassManager; kept so Renderer can push per-frame data.
    GeometryPass*                           m_geometryPass{ nullptr };

    // Placeholder scene until we introduce Scene/World
    std::array<Entity, 1>                   m_entities { Entity("Entity1") };
};
