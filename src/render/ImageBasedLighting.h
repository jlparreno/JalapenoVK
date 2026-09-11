#pragma once

#include "core/VulkanIncludes.h"

#include <cstdint>

// Forward declarations
class VulkanContext;
class ResourceManager;
class Shader;
class EnvironmentMap;

/**
 * @brief Image-based lighting derived from an EnvironmentMap.
 *
 * Owns the three resources the PBR shader reads for ambient light: 
 * the irradiance cube for the diffuse term, and the prefiltered cube plus the BRDF 
 * lookup table for the specular.
 * Also owns the descriptor set exposing them (set 2), bound once per frame by the
 * geometry pass.
 *
 * Everything a generation step needs (descriptor set, pipeline, per-face image views)
 * is local to that step and gone by the time the constructor returns. What outlives
 * startup is the three resources, their samplers, and set 2.
 */
class ImageBasedLighting
{
public:

    /**
     * @brief Constructor. Creates the three resources, runs every generation step and writes set 2.
     *
     * The generation runs to completion before this returns, and every resource is
     * left in eShaderReadOnlyOptimal ready to be sampled. Everything it builds derives from the environment.
     *
     * @param context          The Vulkan context used for all GPU resource operations.
     * @param resourceManager  Manager that loads the shaders from disk. Used only here, not kept.
     * @param environment      Environment cube to use as source of lighting.
     */
    ImageBasedLighting(VulkanContext& context, ResourceManager& resourceManager, const EnvironmentMap& environment);

    /**
     * @brief Destructor. Owned vk::raii handles release themselves.
     */
    ~ImageBasedLighting() = default;

    // Non-copyable / non-movable (owns vk::raii handles)
    ImageBasedLighting(const ImageBasedLighting&)            = delete;
    ImageBasedLighting& operator=(const ImageBasedLighting&) = delete;
    ImageBasedLighting(ImageBasedLighting&&)                 = delete;
    ImageBasedLighting& operator=(ImageBasedLighting&&)      = delete;

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the layout of the IBL descriptor set.
     */
    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

    /**
     * @brief Returns the IBL descriptor set itself, bound once per frame by whoever shades with it.
     *
     * It holds the environment lighting, which is global to the frame and identical for every entity and every material.
     */
    vk::DescriptorSet       GetDescriptorSet() const { return m_descriptorSet; }

private:

    // ----------------------------------------------
    // INITIALIZATION HELPERS
    // ----------------------------------------------

    /**
     * @brief Allocates the irradiance cube image and its cube image view.
     *
     * The image is created eCubeCompatible with six array layers, usable both as a
     * color attachment (it is rendered into, face by face) and as a sampled texture.
     */
    void CreateIrradianceCube();

    /**
     * @brief Allocates the prefiltered cube image and its cube image view.
     *
     * The image is created eCubeCompatible with six array layers and a mip chain, one
     * level per roughness step, and the view spans the whole chain so a shader can
     * sample between levels.
     */
    void CreatePrefilterCube();

    /**
     * @brief Allocates the BRDF lookup table image and its view.
     *
     * The only generated resource that is not a cube: a plain two-channel 2D texture,
     * rendered into once and then sampled.
     */
    void CreateBrdfLut();

    /**
     * @brief Creates the samplers used to read the generated resources.
     *
     * Linear filtering and clamped addressing throughout. They differ only in maxLod,
     * since the resources they read have different numbers of mip levels.
     */
    void CreateSamplers();

    /**
     * @brief Creates the descriptor set 2 layout: the three IBL resources, read from the fragment stage.
     * Binding 0 is the irradiance cube, 1 the prefiltered cube and 2 the BRDF lookup table.
     */
    void CreateDescriptorSetLayout();

    /**
     * @brief Creates the descriptor pool backing the IBL set.
     */
    void CreateDescriptorPool();

    /**
     * @brief Allocates the IBL set and points it at the three generated resources.
     * Written once here: they are all generated at startup and never change.
     */
    void CreateDescriptorSet();


    // ----------------------------------------------
    // LIGHTING INFO GENERATION
    // ----------------------------------------------

    /**
     * @brief Convolves the environment cube into the irradiance cube.
     *
     * Reads the environment cube as its source. Each output texel integrates the cosine-weighted hemisphere
     * around its own direction, which is the incoming diffuse light a surface facing that way receives.
     *
     * @param environment  Environment cube to convolve, mip chain included.
     * @param shader       Shader performing the convolution.
     */
    void ConvolveIrradiance(const EnvironmentMap& environment, const Shader& shader);

    /**
     * @brief Convolves the environment cube into the prefiltered cube's mip chain.
     *
     * One draw per (level, face) pair, each targeting a single-level single-layer view.
     * Every level integrates the GGX lobe for one roughness, from a mirror at level 0
     * to fully rough at the last, so shading recovers any roughness in between with a
     * single fractional-level lookup.
     *
     * @param environment  Environment cube to convolve, mip chain included.
     * @param shader       Shader performing the prefilter convolution.
     */
    void PrefilterSpecular(const EnvironmentMap& environment, const Shader& shader);

    /**
     * @brief Integrates the environment BRDF into the lookup table.
     *
     * The integral depends solely on the view angle and the roughness, never on the
     * environment, so it needs no descriptor set and no push constants.
     *
     * @param shader  Shader performing the integration.
     */
    void IntegrateBrdfLut(const Shader& shader);


    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&                  m_context;                              // Vulkan context used for all GPU operations.

    // Irradiance cube
    vk::raii::Image                 m_irradianceImage       { nullptr };    // Six-layer cube image the convolution renders into.
    vk::raii::DeviceMemory          m_irradianceMemory      { nullptr };    // GPU memory allocation backing the cube.
    vk::raii::ImageView             m_irradianceView        { nullptr };    // Cube view covering all six layers, for sampling.
    vk::raii::Sampler               m_irradianceSampler     { nullptr };    // Sampling configuration for the cube.

    // Prefilter cube
    vk::raii::Image                 m_prefilterImage        { nullptr };    // Six-layer cube image with one mip level per roughness step.
    vk::raii::DeviceMemory          m_prefilterMemory       { nullptr };    // GPU memory allocation backing the cube.
    vk::raii::ImageView             m_prefilterView         { nullptr };    // Cube view covering all six layers and the whole chain, for sampling.
    vk::raii::Sampler               m_prefilterSampler      { nullptr };    // Sampling configuration for the cube, mip filtering included.

    // BRDF lookup table
    vk::raii::Image                 m_brdfLutImage          { nullptr };    // Two-channel 2D image the integration renders into.
    vk::raii::DeviceMemory          m_brdfLutMemory         { nullptr };    // GPU memory allocation backing the table.
    vk::raii::ImageView             m_brdfLutView           { nullptr };    // Plain 2D view, for sampling.
    vk::raii::Sampler               m_brdfLutSampler        { nullptr };    // Sampling configuration for the table, single level.

    // IBL descriptor set (set 2). Declared layout first, set last: members are destroyed in reverse,
    // so the set goes before its pool and both before the layout they were built from.
    vk::raii::DescriptorSetLayout   m_descriptorSetLayout   { nullptr };    // Set-2 layout, also handed to the pipeline layouts that declare it.
    vk::raii::DescriptorPool        m_descriptorPool        { nullptr };    // Pool the set is allocated from. Must outlive it.
    vk::raii::DescriptorSet         m_descriptorSet         { nullptr };    // The environment lighting, bound once per frame.

    // ----------------------------------------------
    // CONSTANTS
    // ----------------------------------------------

    // Resolution of one irradiance face.
    // Convolving over the hemisphere throws high frequency, so the result has no detail left for more texels to hold.
    static constexpr uint32_t   k_irradianceSize    { 32 };

    // Resolution of the prefiltered cube's base level.
    // The rougher levels hold progressively less detail, so the chain starts small.
    static constexpr uint32_t   k_prefilterSize     { 128 };

    // Levels in the prefiltered cube, one roughness step each.
    static constexpr uint32_t   k_prefilterMips     { 5 };

    // Size of the BRDF lookup table.
    static constexpr uint32_t   k_brdfLutSize       { 512 };

    // Half float keeps the cubes small in size and is guaranteed to support linear filtering.
    static constexpr vk::Format k_cubeFormat    { vk::Format::eR16G16B16A16Sfloat };

    // Two channels (RG), not four.
    static constexpr vk::Format k_brdfLutFormat { vk::Format::eR16G16Sfloat };
};