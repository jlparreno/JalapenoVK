#pragma once

#include "materials/Material.h"
#include "render/RenderTypes.h"
#include "resources/Texture.h"

#include <array>

// Forward declarations
class VulkanContext;

/**
 * @brief Metallic-roughness PBR material.
 *
 * Owns its own descriptor set: a PBRFactors UBO plus the 5 PBR texture slots (albedo/normal/metallicRoughness/occlusion/emissive), 
 * written once at construction and never touched again.
 * Unlike set 0 (view/proj/light, owned by GeometryPass), material data doesn't change per frame.
 *
 * Textures are borrowed (non-owning Texture*), same pattern as elsewhere:
 * ownership stays with ResourceManager, so textures shared across materials
 * aren't duplicated.
 */
class PBRMaterial : public Material
{

public:

    /**
     * @brief Scalar/color factors uploaded to the material's UBO.
     *
     * All vec4 (not vec3) to sidestep std140's vec3 alignment padding.
     */
    struct PBRFactors
    {
        alignas(16) glm::vec4 baseColor;           // RGBA multiplier applied to the albedo texture sample.
        alignas(16) glm::vec4 metallicRoughness;   // x = metallic factor, y = roughness factor (zw unused, padding).
        alignas(16) glm::vec4 emissive;            // RGB multiplier applied to the emissive texture sample (w unused, padding).
    };

    /**
     * @brief Constructs a PBRMaterial: uploads its factors and writes its own descriptor set.
     *
     * Builds one descriptor set (not per-frame-in-flight - factors/textures don't change per frame): 
     * binding 0 is the PBRFactors UBO, bindings 1-5 are the combined image samplers for albedo/normal/metallicRoughness/occlusion/emissive.
     *
     * @param context   The Vulkan context used for all GPU resource operations.
     * @param layout    Shared material layout, created once.
     * @param textures  5 textures, in albedo/normal/metallicRoughness/occlusion/emissive order.
     * @param factors   Scalar/color factors uploaded to the UBO.
     */
    explicit PBRMaterial(VulkanContext& context, vk::DescriptorSetLayout layout, const std::array<Texture*, 5>& textures, const PBRFactors& factors);

    /**
     * @brief Destructor. vk::raii handles (pool, set, UBO buffer/memory) release themselves.
     */
    ~PBRMaterial() = default;

    // Non-copyable / non-movable (owns vk::raii handles)
    PBRMaterial(const PBRMaterial&)            = delete;
    PBRMaterial& operator=(const PBRMaterial&) = delete;
    PBRMaterial(PBRMaterial&&)                 = delete;
    PBRMaterial& operator=(PBRMaterial&&)      = delete;


    // ----------------------------------------------
    // MATERIAL OVERRIDES
    // ----------------------------------------------

    /**
     * @brief Returns the shading model for this material: PBR.
     */
    ShadingModel GetShadingModel() const override { return ShadingModel::PBR; };

    /**
     * @brief Returns this material's descriptor set.
     *
     * @return Raw vk::DescriptorSet handle, valid for the lifetime of this PBRMaterial.
     */
    vk::DescriptorSet GetDescriptorSet() const override { return m_descriptorSet; };

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Creates m_factorsBuffer, maps it, and uploads m_factors.
     *
     * A single instance, factors never change per frame.
     */
    void CreateUniformBuffer();

    /**
     * @brief Creates m_descriptorPool, sized for exactly one set: 1 UBO + 5 combined image samplers.
     */
    void CreateDescriptorPool();

    /**
     * @brief Allocates m_descriptorSet from m_descriptorPool and writes it once.
     *
     * Requires m_factorsBuffer to already be created (see CreateUniformBuffer).
     *
     * @param layout  Shared layout.
     */
    void CreateDescriptorSet(vk::DescriptorSetLayout layout);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&           m_context;                          // Vulkan context used for all GPU operations. Not owned by this class.

    PBRFactors               m_factors;                          // Copy of the factors passed at construction; source of truth for m_factorsBuffer's contents.
    std::array<Texture*, 5>  m_textures;                         // Borrowed (ResourceManager-owned), in albedo/normal/metallicRoughness/occlusion/emissive order.

    vk::raii::DescriptorPool m_descriptorPool{ nullptr };        // Sized for exactly the one set below.
    vk::raii::DescriptorSet  m_descriptorSet { nullptr };        // Set 1: binding 0 = m_factorsBuffer, bindings 1-5 = m_textures.
    UBOBuffer                m_factorsBuffer;                    // GPU-side PBRFactors, mapped and written once at construction.
};
