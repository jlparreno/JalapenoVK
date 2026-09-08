#pragma once

#include "core/VulkanIncludes.h"

#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
#include <utility>

// Forward declarations
class VulkanContext;
class Texture;
class Shader;

/**
 * @brief Environment lighting built at startup from a single equirectangular HDR image.
 *
 * Owns the cubemap the skybox draws and the IBL steps consume. Nothing here is
 * read from disk as a cubemap: the cube is rendered, one face per draw, by
 * projecting the equirectangular source onto each of its six faces.
 *
 * Everything the projection needs (descriptor set, pipeline, per-face image views)
 * is local to the generation and gone by the time the constructor returns, so the
 * only GPU state that outlives startup is the cube itself and its sampler.
 */
class EnvironmentMap
{
public:

    /**
     * @brief Inputs consumed during construction.
     */
    struct CreateInfo
    {
        const Texture*  equirect;   // Equirectangular HDR source projected onto the cube.
        const Shader*   shader;     // Shader performing the equirect to cube projection.
    };

    /**
     * @brief Orientation of one cube face, in world space.
     *
     * A texel at normalized device coordinates (u, v) within the face looks along
     * forward + u * right + v * up. Pushed to the fragment stage as a push constant,
     * one face per draw.
     */
    struct CubeFaceOrientation
    {
        glm::vec4 right;
        glm::vec4 up;
        glm::vec4 forward;
    };

    /**
     * @brief Creates the cubemap and renders the equirectangular source into its six faces.
     *
     * The generation runs to completion before this returns: the cube is left in
     * eShaderReadOnlyOptimal and is ready to be sampled.
     *
     * @param context  The Vulkan context used for all GPU resource operations.
     * @param info     Equirectangular source and projection shader.
     */
    EnvironmentMap(VulkanContext& context, const CreateInfo& info);

    /**
     * @brief Destructor. Owned vk::raii handles release themselves.
     */
    ~EnvironmentMap() = default;

    // Non-copyable / non-movable (owns vk::raii handles)
    EnvironmentMap(const EnvironmentMap&)            = delete;
    EnvironmentMap& operator=(const EnvironmentMap&) = delete;
    EnvironmentMap(EnvironmentMap&&)                 = delete;
    EnvironmentMap& operator=(EnvironmentMap&&)      = delete;

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the environment cube for the skybox and the IBL.
     */
    vk::ImageView   GetEnvironmentView()    const { return m_environmentView; }

    /**
     * @brief Returns the environment sampler paired with the environment cube.
     */
    vk::Sampler     GetEnvironmentSampler() const { return m_environmentSampler; }

private:

    // ----------------------------------------------
    // INITIALIZATION HELPERS
    // ----------------------------------------------

    /**
     * @brief Allocates the cube image and the cube image view.
     *
     * The image is created eCubeCompatible with six array layers, usable both as a
     * color attachment (it is rendered into, face by face) and as a sampled texture.
     */
    void CreateEnvironmentCube();

    /**
     * @brief Creates the sampler used to read the environment cube.
     *
     * Linear filtering, clamped addressing and a single mip level.
     */
    void CreateSamplers();

    /**
     * @brief Renders the equirectangular source into the six faces of the cube.
     *
     * Records one draw per face into a single one-shot command buffer: a whole-cube
     * transition to eColorAttachmentOptimal, six fullscreen-triangle draws each
     * targeting a single-layer view of one face, and a final transition to
     * eShaderReadOnlyOptimal.
     *
     * @param equirect  Equirectangular HDR source to project.
     * @param shader    Shader performing the projection.
     */
    void ProjectEquirectToCube(const Texture& equirect, const Shader& shader);

    /**
     * @brief Builds the pipeline driving the cube projection draws.
     *
     * Declares no vertex input (the vertex stage generates a fullscreen triangle
     * from SV_VertexID), no depth state, and a single-sampled color attachment.
     *
     * @param shader     Shader providing the vertMain / fragMain entry points.
     * @param setLayout  Layout of the set holding the equirectangular source.
     *
     * @return The pipeline layout and the pipeline, in that order.
     */
    std::pair<vk::raii::PipelineLayout, vk::raii::Pipeline> CreateEquirectToCubePipeline(const Shader& shader, vk::DescriptorSetLayout setLayout);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&                  m_context;                              // Vulkan context used for all GPU operations. Not owned by this class.

    // Environment cube
    vk::raii::Image                 m_environmentImage      { nullptr };    // Six-layer cube image the projection renders into.
    vk::raii::DeviceMemory          m_environmentMemory     { nullptr };    // GPU memory allocation backing the cube.
    vk::raii::ImageView             m_environmentView       { nullptr };    // Cube view covering all six layers, for sampling.
    vk::raii::Sampler               m_environmentSampler    { nullptr };    // Sampling configuration for the cube.

    // ----------------------------------------------
    // CONSTANTS
    // ----------------------------------------------

    // Resolution of one cube face. 
    // 4096 / 4 is exactly one face's worth of a 4k equirectangular source at 90 degrees per face.
    static constexpr uint32_t   k_environmentSize   { 1024 };

    // Half float keeps the cube small in size and is guaranteed to support linear filtering.
    static constexpr vk::Format k_environmentFormat { vk::Format::eR16G16B16A16Sfloat };

    // Orientation of each cube face, derived from the Vulkan specification. 
    // The order below is the cube's layer order.
    static constexpr std::array<CubeFaceOrientation, 6> k_cubeFacesOrientation
    { 
        {
            { {  0,  0, -1, 0 }, { 0, -1,  0, 0 }, {  1,  0,  0, 0 } }, // +X
            { {  0,  0,  1, 0 }, { 0, -1,  0, 0 }, { -1,  0,  0, 0 } }, // -X
            { {  1,  0,  0, 0 }, { 0,  0,  1, 0 }, {  0,  1,  0, 0 } }, // +Y
            { {  1,  0,  0, 0 }, { 0,  0, -1, 0 }, {  0, -1,  0, 0 } }, // -Y
            { {  1,  0,  0, 0 }, { 0, -1,  0, 0 }, {  0,  0,  1, 0 } }, // +Z
            { { -1,  0,  0, 0 }, { 0, -1,  0, 0 }, {  0,  0, -1, 0 } }, // -Z
        } 
    };
};
