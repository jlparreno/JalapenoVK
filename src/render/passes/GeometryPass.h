#pragma once

#include "render/RenderTypes.h"
#include "render/passes/RenderPass.h"

#include <vector>

// Forward declarations
class VulkanContext;
class ResourceManager;
class RenderTarget;
class Shader;
class Mesh;
class ImageBasedLighting;

/**
 * @brief Geometry render pass: draws opaque geometry into the shared MSAA color target and resolves it into the current swapchain image.
 *
 * Owns only what makes sense for this pass alone: the descriptor set layout, the
 * pipeline layout, the graphics pipeline, its descriptor pool + per-frame
 * descriptor sets, and the per-frame uniform buffers. The color and depth
 * attachments it renders into belong to the RenderTarget it is handed, since
 * those are shared with the other passes of the frame.
 *
 * Consumes from the outside only what depends on the current frame (target
 * swapchain view, renderables to draw) via SetFrameData().
 */
class GeometryPass : public RenderPass
{
public:

    /**
     * @brief Static configuration provided at construction time.
     */
    struct CreateInfo
    {
        RenderTarget*           renderTarget;       // Shared color + depth attachments this pass renders into (borrowed)
        vk::DescriptorSetLayout materialLayout;     // Shared PBR material set layout
        ImageBasedLighting*     imageBasedLighting; // Owner of the IBL set (set 2) this pass samples
    };

    /**
     * @brief Per-frame inputs pushed by the Renderer before Execute().
     */
    struct FrameData
    {
        vk::ImageView swapchainImageView{ nullptr }; // Target for the MSAA resolve.
        vk::Extent2D  extent{};                      // Actual render area for this frame.
        glm::mat4     view { 1.0f };                 // View matrix supplied by the (still external) camera.
        glm::mat4     proj { 1.0f };                 // Projection matrix supplied by the camera.

        glm::vec3     lightDirection{};              // World-space direction the active light points towards.
        glm::vec3     lightColor{};                  // Active light's color (RGB, not intensity-scaled).
        float         lightIntensity{ 0.0f };        // Active light's intensity multiplier.
        glm::vec3     cameraPosition{};              // World-space position of the active camera.

        std::vector<Renderable> renderables;
    };

    /**
     * @brief Builds the pass.
     *
     * @param name             Name the pass is registered under.
     * @param context          Vulkan context used for all GPU operations.
     * @param resourceManager  Manager to load the shader for this pass: pbr.slang.
     * @param info             Render target, material layout and IBL this pass works with.
     */
    GeometryPass(const std::string& name, VulkanContext& context, ResourceManager& resourceManager, const CreateInfo& info);

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

    /**
     * @brief Creates the set-0 layout: a single uniform buffer binding, read from both the vertex and fragment stages.
     */
    void CreateDescriptorSetLayout();

    /**
     * @brief Creates the pipeline layout and the graphics pipeline.
     *
     * The layout declares set 0 (m_descriptorSetLayout) + set 1 (m_info.materialLayout) + set 2 (the IBL) 
     * plus the model matrix push constant. The pipeline itself uses dynamic rendering
     * (no VkRenderPass), takes its vertex input from Vertex, and reads sample
     * count and attachment formats from the RenderTarget.
     *
     * @param shader  Shader providing the vertMain / fragMain entry points.
     */
    void CreatePipeline(const Shader& shader);

    /**
     * @brief Creates the descriptor pool backing the per-frame sets: one uniform buffer per frame in flight.
     */
    void CreateDescriptorPool();

    /**
     * @brief Creates one host-visible SceneData buffer per frame in flight, mapped once and kept mapped.
     */
    void CreateUniformBuffers();

    /**
     * @brief Allocates one set-0 descriptor set per frame in flight and points each at its own uniform buffer.
     *
     * Written once here: the binding never changes, only the buffer's contents do.
     */
    void CreateDescriptorSets();

    /**
     * @brief Copies the current frame's SceneData into the persistently mapped UBO for the given frame slot.
     *
     * @param frameIndex  Frame-in-flight slot to write into.
     */
    void UpdateUniformBuffer(uint32_t frameIndex);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Non-owned references / config
    VulkanContext&      m_context;                                                  // Vulkan context used for all GPU operations. Not owned by this class.
    CreateInfo          m_info;                                                     // Static configuration this pass was constructed with (render target, material layout, IBL).
    FrameData           m_frame{};                                                  // Most recent per-frame data pushed via SetFrameData(), consumed by BeginPass() and Render().

    // Pipeline stack
    vk::raii::PipelineLayout             m_pipelineLayout           { nullptr };    // Set 0 (m_descriptorSetLayout) + set 1 (m_info.materialLayout) + set 2 (IBL) + model push constants.
    vk::raii::Pipeline                   m_pipeline                 { nullptr };    // Graphics pipeline. PBR at the moment.

    // Descriptor + per-frame data (one entry per frame-in-flight)
    vk::raii::DescriptorSetLayout        m_descriptorSetLayout      { nullptr };    // Set-0 layout (per-frame UBO: view/proj/light/camera).
    vk::raii::DescriptorPool             m_descriptorPool           { nullptr };
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;

    std::vector<UBOBuffer>               m_uniformBuffers;
};
