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


/**
 * @brief Base class for a single render pass step, executed by the RenderPassManager.
 *
 * A RenderPass encapsulates one self-contained chunk of GPU work with its own
 * pipeline, attachments, and per-frame resources. The base class provides a
 * Non-Virtual Interface (Execute()) that dispatches to BeginPass / Render /
 * EndPass overrides only when the pass is enabled, giving derived passes a
 * fixed lifecycle to hook into.
 *
 * Passes are named for lookup and can declare dependencies on other passes;
 * the RenderPassManager uses these declarations to produce a topologically
 * sorted execution order.
 */
class RenderPass
{
public:

    /**
     * @brief Constructs a pass with the given name.
     *
     * @param passName  Unique identifier used by the RenderPassManager to look this pass up
     *                  and to reference it as a dependency from other passes.
     */
    explicit RenderPass(const std::string& passName);

    /**
     * @brief Virtual destructor for proper cleanup of derived passes.
     */
    virtual ~RenderPass() = default;


    // ----------------------------------------------
    // RENDERPASS MANAGEMENT
    // ----------------------------------------------

    /**
     * @brief Declares that this pass must run after another pass named @p dependency.
     *
     * Consumed by the RenderPassManager when building the topologically sorted
     * execution order. Dependencies on unknown pass names are ignored at sort time.
     *
     * @param dependency  Name of the pass this one must run after.
     */
    void AddDependency(const std::string& dependency);

    /**
     * @brief Non-Virtual Interface entry point. Dispatches to BeginPass / Render / EndPass if the pass is enabled.
     *
     * @param commandBuffer  Command buffer being recorded for this frame.
     * @param frame          Per-frame indexing information.
     */
    void Execute(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame);

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the identifier this pass was registered under.
     */
    const std::string& GetName() const { return m_name; }

    /**
     * @brief Returns the names of the passes this one depends on.
     */
    const std::vector<std::string>& GetDependencies() const { return m_dependencies; }

    /**
     * @brief Returns whether the pass will execute this frame.
     */
    bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Enables or disables this pass. Disabled passes are skipped by Execute().
     *
     * @param isEnabled  New enabled state.
     */
    void SetEnabled(bool isEnabled) { m_enabled = isEnabled; }

protected:

    // ----------------------------------------------
    // VIRTUAL METHODS FOR ALL RENDERPASSES
    // ----------------------------------------------

    /**
     * @brief Prepares the command buffer to record draw calls for this pass.
     *
     * With dynamic rendering this typically calls vkCmdBeginRendering
     * (as opposed to vkCmdBeginRenderPass with the traditional model).
     *
     * @param commandBuffer  Command buffer being recorded for this frame.
     * @param frame          Per-frame indexing information.
     */
    virtual void BeginPass(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame) = 0;

    /**
     * @brief Records the pass's actual draw calls into the command buffer.
     *
     * @param commandBuffer  Command buffer being recorded for this frame.
     * @param frame          Per-frame indexing information.
     */
    virtual void Render(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame) = 0;

    /**
     * @brief Finalizes the pass and closes its recording block on the command buffer.
     *
     * With dynamic rendering this typically calls vkCmdEndRendering
     * (as opposed to vkCmdEndRenderPass with the traditional model).
     *
     * @param commandBuffer  Command buffer being recorded for this frame.
     * @param frame          Per-frame indexing information.
     */
    virtual void EndPass(vk::raii::CommandBuffer& commandBuffer, const FrameInfo& frame) = 0;

private:

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    std::string                 m_name;             // Identifier used by the RenderPassManager to look this pass up.
    std::vector<std::string>    m_dependencies;     // Names of the passes this one must run after.

    bool                        m_enabled{ true };  // Whether the pass participates in the current frame's Execute().
};
