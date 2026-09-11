#pragma once

#include "render/RenderTypes.h"
#include "render/passes/RenderPass.h"

// Forward declarations
class VulkanContext;
class ResourceManager;
class RenderTarget;
class Shader;
class EnvironmentMap;

/**
 * @brief Skybox render pass: fills the MSAA color target with the environment cubemap.
 *
 * Runs first in the frame, so it is the pass that clears the color attachment and
 * transitions it for rendering; GeometryPass then loads what this leaves behind.
 */
class SkyboxPass : public RenderPass
{
public:

    /**
     * @brief Static configuration provided at construction time.
     */
    struct CreateInfo
    {
        RenderTarget*   renderTarget;       // Shared color attachment this pass renders into
        EnvironmentMap* environmentMap;     // Owner of the environment cubemap this pass samples
    };

    /**
     * @brief Per-frame inputs pushed by the Renderer before Execute().
     */
    struct FrameData
    {
        vk::Extent2D    extent{};                   // Actual render area for this frame.
        glm::mat4       invViewProj{ 1.0f };        // Inverse of the projection times the view without translation, so the fragment stage can turn NDC back into a world-space direction.
    };

    /**
     * @brief Builds the pass.
     *
     * @param name             Name the pass is registered under.
     * @param context          Vulkan context used for all GPU operations.
     * @param resourceManager  Manager to load the shader for this pass: skybox.slang.
     * @param info             Render target and environment map this pass works with.
     */
    SkyboxPass(const std::string& name, VulkanContext& context, ResourceManager& resourceManager, const CreateInfo& info);

    ~SkyboxPass() = default;

    // Non-copyable / non-movable (owns vk::raii handles)
    SkyboxPass(const SkyboxPass&)            = delete;
    SkyboxPass& operator=(const SkyboxPass&) = delete;
    SkyboxPass(SkyboxPass&&)                 = delete;
    SkyboxPass& operator=(SkyboxPass&&)      = delete;

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
     * @brief Creates the set-0 layout: a single combined image sampler, read from the fragment stage.
     */
    void CreateDescriptorSetLayout();

    /**
     * @brief Creates the pipeline layout and the graphics pipeline.
     *
     * The layout declares set 0 (m_descriptorSetLayout) plus the inverse view-projection push
     * constant, which the fragment stage consumes. The pipeline is the builder's
     * fullscreen archetype almost untouched: no vertex input and no depth state,
     * with only the sample count and color format taken from the RenderTarget so
     * it stays compatible with the attachment it shares with GeometryPass.
     *
     * @param shader  Shader providing the vertMain / fragMain entry points.
     */
    void CreatePipeline(const Shader& shader);

    /**
     * @brief Creates the descriptor pool backing the single set: one combined image sampler.
     */
    void CreateDescriptorPool();

    /**
     * @brief Allocates the set-0 descriptor set and points it at the environment cubemap.
     *
     * A single set rather than one per frame in flight: nothing in it is rewritten
     * from frame to frame, so there is no per-frame resource.
     * Written once here, and the cube is already left in eShaderReadOnlyOptimal by
     * the generation step, so sampling it needs no per-frame barrier either.
     */
    void CreateDescriptorSets();

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Non-owned references / config
    VulkanContext&      m_context;            // Vulkan context used for all GPU operations. Not owned by this class.
    CreateInfo          m_info;               // Static configuration this pass was constructed with (render target, environment map).
    FrameData           m_frame{};            // Most recent per-frame data pushed via SetFrameData(), consumed by BeginPass() and Render().

    // Pipeline stack
    vk::raii::PipelineLayout             m_pipelineLayout       { nullptr };        // Set 0 (m_descriptorSetLayout) + the inverse view-projection push constant.
    vk::raii::Pipeline                   m_pipeline             { nullptr };        // Graphics pipeline drawing the fullscreen triangle.

    // Descriptor set for the environment cubemap.
    vk::raii::DescriptorSetLayout        m_descriptorSetLayout  { nullptr };        // Set-0 layout (environment cubemap sampler).
    vk::raii::DescriptorPool             m_descriptorPool       { nullptr };
    vk::raii::DescriptorSet              m_descriptorSet        { nullptr };
};