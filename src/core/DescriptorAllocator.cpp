#include "core/DescriptorAllocator.h"

#include "core/VulkanContext.h"

#include <utility>

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

DescriptorAllocator::SingleImageDescriptor DescriptorAllocator::CreateSingleImageDescriptor(VulkanContext& context, vk::Sampler sampler, vk::ImageView view)
{
    SingleImageDescriptor descriptor;

    // DescriptorSet Layout -> Binding 0: one combined image sampler, read by the fragment stage.
    vk::DescriptorSetLayoutBinding binding
    {
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = 1,
        .pBindings = &binding
    };

    descriptor.layout = vk::raii::DescriptorSetLayout(context.GetDevice(), layoutInfo);

    // DescriptorPool
    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1 }
    };

    descriptor.pool = CreatePool(context, 1, poolSizes);

    // Descriptor Set pointing at the image view received
    std::vector<vk::raii::DescriptorSet> sets = AllocateSets(context, descriptor.pool, descriptor.layout, 1);
    descriptor.set = std::move(sets[0]);

    vk::DescriptorImageInfo imageInfo
    {
        .sampler = sampler,
        .imageView = view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    vk::WriteDescriptorSet write
    {
        .dstSet = descriptor.set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &imageInfo
    };

    context.GetDevice().updateDescriptorSets(write, {});

    return descriptor;
}