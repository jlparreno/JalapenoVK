#pragma once

#include "core/VulkanIncludes.h"

#include <vector>

// Forward declarations
class VulkanContext;
class Shader;

/**
 * @brief Builds a graphics pipeline and its layout from the configured states.
 *
 * The defaults are for a fullscreen-triangle (no vertex input, no depth, single-sampled),
 * so a pipeline configure only what differs from there.
 *
 * The caller still needs to bind descriptor sets and push constants.
 */
class PipelineBuilder
{
public:

    /**
     * @brief A graphics pipeline together with the layout it was built with.
     */
    struct GraphicsPipeline
    {
        vk::raii::PipelineLayout    layout{ nullptr };      // Descriptor set layouts and push constant ranges.
        vk::raii::Pipeline          pipeline{ nullptr };    // The pipeline itself.
    };

    /**
     * @brief Creates a builder configured as the fullscreen-triangle archetype.
     *
     * @param context The Vulkan context that will create the pipeline.
     */
    explicit PipelineBuilder(VulkanContext& context);

    /**
     * @brief Creates the pipeline from the configured state.
     *
     * @return The newly created pipeline and its layout.
     */
    GraphicsPipeline Build();

    // ----------------------------------------------
    // SETTERS
    // ----------------------------------------------

    /**
     * @brief Declares the vertex buffer layout the pipeline reads from.
     *
     * @param binding    How the GPU steps through the vertex buffer.
     * @param attributes One entry per vertex attribute, mapping it to a shader location.
     */
    PipelineBuilder& SetVertexInput(const vk::VertexInputBindingDescription& binding, vk::ArrayProxy<const vk::VertexInputAttributeDescription> attributes);

    /**
     * @brief Sets the shader providing both stages, and optionally its entry points.
     *
     * @param shader Shader module holding the vertex and fragment stages.
     * @param vert   Name of the vertex entry point.
     * @param frag   Name of the fragment entry point.
     */
    PipelineBuilder& SetShader(const Shader& shader, const char* vert = "vertMain", const char* frag = "fragMain");

    /**
     * @brief Sets the multisampling level, which must match the attachments rendered into.
     *
     * @param samples          Sample count of the color attachment.
     * @param minSampleShading Fraction of samples shaded independently, 0 to disable.
     */
    PipelineBuilder& SetSamples(vk::SampleCountFlagBits samples, float minSampleShading = 0.0f);

    /**
     * @brief Enables depth testing with the given comparison.
     *
     * Requires to configure a depth format too.
     *
     * @param write     Whether passing fragments write their depth back.
     * @param compareOp Comparison a fragment's depth must pass to be kept.
     */
    PipelineBuilder& SetDepthTest(bool write, vk::CompareOp compareOp);

    /**
     * @brief Declares the descriptor set layouts, one per set, in set order.
     *
     * @param layouts Layout of set 0, set 1, and so on.
     */
    PipelineBuilder& SetDescriptorSetLayouts(vk::ArrayProxy<const vk::DescriptorSetLayout> layouts);

    /**
     * @brief Declares a single push constant range, starting at offset 0.
     *
     * @param stages Shader stages that read the push constants.
     * @param size   Size of the range in bytes.
     */
    PipelineBuilder& SetPushConstants(vk::ShaderStageFlags stages, uint32_t size);

    /**
     * @brief Sets the format of the color attachment rendered into. Required.
     */
    PipelineBuilder& SetColorFormat(vk::Format format);

    /**
     * @brief Sets the format of the depth attachment rendered into.
     */
    PipelineBuilder& SetDepthFormat(vk::Format format);

private:

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext& m_context;                                                   // Vulkan context creating the pipeline. Not owned by this class.

    const Shader*           m_shader            { nullptr };                    // Shader providing both stages.
    const char*             m_vertEntry         { "vertMain" };                 // Vertex entry point within the shader.
    const char*             m_fragEntry         { "fragMain" };                 // Fragment entry point within the shader.

    vk::Format              m_colorFormat       { vk::Format::eUndefined };     // Format of the color attachment rendered into.
    vk::Format              m_depthFormat       { vk::Format::eUndefined };     // Format of the depth attachment, undefined when depth is unused.
    bool                    m_depthTest         { false };                      // Whether depth state is declared at all.
    bool                    m_depthWrite        { false };                      // Whether passing fragments write their depth back.
    vk::CompareOp           m_depthCompare      { vk::CompareOp::eLess };       // Comparison a fragment's depth must pass to be kept.

    vk::SampleCountFlagBits m_samples           { vk::SampleCountFlagBits::e1 };// Sample count, matching the attachments rendered into.
    float                   m_minSampleShading  { 0.0f };                       // Fraction of samples shaded independently. Zero disables sample shading.

    vk::VertexInputBindingDescription                   m_vertexBinding{};      // How the GPU steps through the vertex buffer.
    std::vector<vk::VertexInputAttributeDescription>    m_vertexAttributes;     // One entry per vertex attribute. Empty means no vertex input.

    std::vector<vk::DescriptorSetLayout>                m_setLayouts;           // One layout per set, in set order. Empty means no sets.
    vk::PushConstantRange                               m_pushConstantRange{};  // Single push constant range.
};