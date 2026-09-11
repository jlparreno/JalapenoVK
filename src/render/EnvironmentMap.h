#pragma once

#include "core/VulkanIncludes.h"

#include <cstdint>

// Forward declarations
class VulkanContext;
class ResourceManager;
class Texture;
class Shader;

/**
 * @brief Environment cube built at startup from a single equirectangular HDR image.
 *
 * Owns the environment cube the skybox draws: the equirectangular source is projected 
 * onto it one face per draw, and its mip chain is then generated.
 *
 * Everything a generation step needs (descriptor set, pipeline, per-face image views)
 * is local to that step and gone by the time the constructor returns. What outlives
 * startup is the cube and its sampler.
 */
class EnvironmentMap
{
public:

    /**
     * @brief Creates the environment cube from a single equirectangular image.
     *
     * The generation runs to completion before this returns: the equirectangular
     * source is projected onto the environment cube, its mip chain is filled, and
     * the whole chain is left in eShaderReadOnlyOptimal ready to be sampled. The
     * source texture is not referenced afterwards, so the caller is free to unload it.
     *
     * @param context          The Vulkan context used for all GPU resource operations.
     * @param resourceManager  Manager to load the shader for this pass.
     * @param equirect         Equirectangular HDR source projected onto the cube.
     */
    EnvironmentMap(VulkanContext& context, ResourceManager& resourceManager, const Texture& equirect);

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
     * @brief Creates the sampler used to read the cube.
     *
     * Linear filtering and clamped addressing, with maxLod spanning the whole mip chain.
     */
    void CreateSampler();

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
     * @brief Fills the environment cube's mip chain from its top level.
     *
     * Must run after ProjectEquirectToCube, which writes only mip 0.
     * Blits each level from the one above it, six layers at a time, 
     * and leaves every level in eShaderReadOnlyOptimal.
     */
    void GenerateEnvironmentMips();

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

    // Levels in the environment cube's mip chain: floor(log2(1024)) + 1, down to 1x1.
    // The prefilter step reads a level per sample rather than always the sharpest one,
    // which is what keeps the sun from resolving as fireflies in the rough mips.
    static constexpr uint32_t   k_environmentMips   { 11 };

    // Half float keeps the cube small in size and is guaranteed to support linear filtering.
    static constexpr vk::Format k_cubeFormat        { vk::Format::eR16G16B16A16Sfloat };
};