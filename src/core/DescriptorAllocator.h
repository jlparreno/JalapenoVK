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
 */
class DescriptorAllocator
{
public:

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
};
