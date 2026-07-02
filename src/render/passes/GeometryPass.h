#pragma once

#include "render/RenderTypes.h"
#include "render/passes/RenderPass.h"

#include <vector>

// Forward declarations
class VulkanContext;
class Shader;
class Texture;
class Mesh;

/**
 * @brief Geometry render pass: draws opaque geometry into an MSAA color target
 *        and resolves it into the current swapchain image.
 *
 * Owns all resources that only make sense for this pass: the MSAA color and
 * depth attachments, the descriptor set layout, the pipeline layout, the
 * graphics pipeline, its descriptor pool + per-frame descriptor sets, and the
 * per-frame uniform buffers.
 *
 * Consumes from the outside only what depends on the current frame (target
 * swapchain view, mesh to draw) via SetFrameData().
 */
class GeometryPass : public RenderPass
{
public:

    /**
     * @brief Static configuration provided at construction time.
     */
    struct CreateInfo
    {
        vk::Format    colorFormat;  // Swapchain color format (used for pipeline + MSAA target).
        vk::Extent2D  extent;       // Initial render target size.
        Shader*       shader;       // Shader resource used by this pass (borrowed).
        Texture*      texture;      // Texture bound in the descriptor sets (borrowed, temporary).
    };

    /**
     * @brief Per-frame inputs pushed by the Renderer before Execute().
     */
    struct FrameData
    {
        vk::ImageView swapchainImageView{ nullptr }; // Target for the MSAA resolve.
        vk::Extent2D  extent{};                      // Actual render area for this frame.
        Mesh*         mesh{ nullptr };               // Mesh drawn this frame (temporary until draw-list exists).
        glm::mat4     model{ 1.0f };                 // Model matrix of the object being drawn.
        glm::mat4     view { 1.0f };                 // View matrix supplied by the (still external) camera.
        glm::mat4     proj { 1.0f };                 // Projection matrix supplied by the camera.
    };

    GeometryPass(const std::string& name, VulkanContext& context, const CreateInfo& info);

    ~GeometryPass() = default;

    // Non-copyable / non-movable (owns vk::raii handles)
    GeometryPass(const GeometryPass&)            = delete;
    GeometryPass& operator=(const GeometryPass&) = delete;
    GeometryPass(GeometryPass&&)                 = delete;
    GeometryPass& operator=(GeometryPass&&)      = delete;

    /**
     * @brief Feed the pass with the transient data needed to render one frame.
     */
    void SetFrameData(const FrameData& data) { m_frame = data; }

    /**
     * @brief Recreate the pass-owned render targets at a new size.
     *
     * Called by the Renderer after the swapchain has been recreated. Assumes the
     * caller has already ensured the GPU is idle (Swapchain::Recreate() calls
     * waitIdle() internally, so this is safe to invoke right after).
     */
    void OnResize(vk::Extent2D newExtent);

protected:

    // ----------------------------------------------
    // RenderPass overrides
    // ----------------------------------------------

    void BeginPass(vk::raii::CommandBuffer& cmd, const FrameInfo& frame) override;
    void Render   (vk::raii::CommandBuffer& cmd, const FrameInfo& frame) override;
    void EndPass  (vk::raii::CommandBuffer& cmd, const FrameInfo& frame) override;

private:

    // ----------------------------------------------
    // INITIALIZATION HELPERS
    // ----------------------------------------------

    void CreateAttachments();
    void CreateDescriptorSetLayout();
    void CreatePipeline();
    void CreateDescriptorPool();
    void CreateUniformBuffers();
    void CreateDescriptorSets();

    // Copies m_frame.{model,view,proj} into the persistently mapped UBO for the given frame slot.
    void UpdateUniformBuffer(uint32_t frameIndex);

    vk::SampleCountFlagBits QueryMaxUsableSampleCount() const;
    vk::Format              FindDepthFormat() const;

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Non-owned references / config
    VulkanContext&      m_context;
    CreateInfo          m_info;
    FrameData           m_frame{};

    // Derived at construction
    vk::SampleCountFlagBits m_samples     { vk::SampleCountFlagBits::e1 };
    vk::Format              m_depthFormat { vk::Format::eUndefined };

    // Pipeline stack
    vk::raii::DescriptorSetLayout        m_setLayout      { nullptr };
    vk::raii::PipelineLayout             m_pipelineLayout { nullptr };
    vk::raii::Pipeline                   m_pipeline       { nullptr };

    // Attachments (MSAA color + depth). Rendered into, then color is resolved to swapchain.
    vk::raii::Image                      m_colorImage  { nullptr };
    vk::raii::DeviceMemory               m_colorMemory { nullptr };
    vk::raii::ImageView                  m_colorView   { nullptr };

    vk::raii::Image                      m_depthImage  { nullptr };
    vk::raii::DeviceMemory               m_depthMemory { nullptr };
    vk::raii::ImageView                  m_depthView   { nullptr };

    // Descriptor + per-frame data (one entry per frame-in-flight)
    vk::raii::DescriptorPool             m_descriptorPool { nullptr };
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;
    std::vector<UBOBuffer>               m_uniformBuffers;
};
