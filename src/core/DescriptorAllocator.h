#pragma once

#include "core/VulkanIncludes.h"

#include <vector>

// Forward declarations
class VulkanContext;

/**
 * @brief Shared helpers for descriptor pool/set creation.
 *
 * Writing the allocated sets stays with the caller: the binding layout differs
 * per use case (a pass's per-frame UBO+texture vs. a material's factors UBO + several textures).
 * The one exception is CreateSingleImageDescriptor, whose layout is always the same.
 */
class DescriptorAllocator
{
public:

    /**
     * @brief A descriptor set together with the layout and the pool it was built from.
     */
    struct SingleImageDescriptor
    {
        vk::raii::DescriptorSetLayout   layout  { nullptr };    // One combined image sampler at binding 0, read by the fragment stage.
        vk::raii::DescriptorPool        pool    { nullptr };    // Pool the set is allocated from. Must outlive it.
        vk::raii::DescriptorSet         set     { nullptr };    // The set, already pointing at the image.
    };

    /**
     * @brief Create a descriptor pool sized for the given pool sizes.
     *
     * @param context   Vulkan context (device owner).
     * @param maxSets   Maximum number of sets that can be allocated from the pool.
     * @param poolSizes Descriptor type/count pairs the pool must accommodate.
     */
    static vk::raii::DescriptorPool CreatePool(VulkanContext& context, uint32_t maxSets, const std::vector<vk::DescriptorPoolSize>& poolSizes);

    /**
     * @brief Allocate one or more descriptor sets sharing the same layout.
     *
     * @param context Vulkan context (device owner).
     * @param pool    Pool to allocate from.
     * @param layout  Layout applied to every allocated set.
     * @param count   Number of sets to allocate.
     */
    static std::vector<vk::raii::DescriptorSet> AllocateSets(VulkanContext& context, const vk::raii::DescriptorPool& pool, vk::DescriptorSetLayout layout, uint32_t count);

    /**
     * @brief Builds a set exposing a single image to the fragment stage, and writes it.
     *
     * For short-lived sets that sample a source image
     *
     * @param context Vulkan context (device owner).
     * @param sampler Sampler the image is read with.
     * @param view    View of the image. Must be in eShaderReadOnlyOptimal whenever the set is used.
     */
    static SingleImageDescriptor CreateSingleImageDescriptor(VulkanContext& context, vk::Sampler sampler, vk::ImageView view);
};
