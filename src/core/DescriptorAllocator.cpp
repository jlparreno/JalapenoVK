#include "core/DescriptorAllocator.h"

#include "core/VulkanContext.h"

vk::raii::DescriptorPool DescriptorAllocator::CreatePool(VulkanContext& context, uint32_t maxSets, const std::vector<vk::DescriptorPoolSize>& poolSizes)
{
    vk::DescriptorPoolCreateInfo poolInfo
    {
        .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets       = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    return vk::raii::DescriptorPool(context.GetDevice(), poolInfo);
}

std::vector<vk::raii::DescriptorSet> DescriptorAllocator::AllocateSets(VulkanContext& context, const vk::raii::DescriptorPool& pool, vk::DescriptorSetLayout layout, uint32_t count)
{
    std::vector<vk::DescriptorSetLayout> layouts(count, layout);

    vk::DescriptorSetAllocateInfo allocInfo
    {
        .descriptorPool     = pool,
        .descriptorSetCount = count,
        .pSetLayouts        = layouts.data()
    };

    return context.GetDevice().allocateDescriptorSets(allocInfo);
}
