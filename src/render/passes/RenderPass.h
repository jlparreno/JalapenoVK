#pragma once

#include "core/VulkanIncludes.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Per-frame indexing information passed to every RenderPass during Execute().
 *
 * Carries the transient state a pass needs to know about the current frame
 * without coupling the pass to the Swapchain or Renderer directly.
 */
struct FrameInfo
{
    uint32_t imageIndex;   // Swapchain image index acquired for this frame.
    uint32_t frameIndex;   // Current frame-in-flight slot (used to index per-frame resources).
};


class RenderPass
{
public:

    explicit RenderPass(const std::string& passName);

    virtual ~RenderPass() = default;


    void AddDependency(const std::string& dependency);

    // Non-Virtual Interface: dispatches to BeginPass/Render/EndPass if the pass is enabled.
    void Execute(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame);

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    const std::string& GetName() const { return m_name; }

    const std::vector<std::string>& GetDependencies() const { return m_dependencies; }

    bool IsEnabled() const { return m_enabled; }

    void SetEnabled(bool isEnabled) { m_enabled = isEnabled; }

protected:

    // ----------------------------------------------
    // VIRTUAL METHODS FOR ALL RENDERPASSES
    // ----------------------------------------------

    // With dynamic rendering, BeginPass typically calls vkCmdBeginRendering
    // instead of vkCmdBeginRenderPass
    virtual void BeginPass(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame) = 0;

    virtual void Render(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame) = 0;

    // With dynamic rendering, EndPass typically calls vkCmdEndRendering
    // instead of vkCmdEndRenderPass
    virtual void EndPass(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame) = 0;

private:

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    std::string                 m_name;
    std::vector<std::string>    m_dependencies;

    bool                        m_enabled{ true };
};
