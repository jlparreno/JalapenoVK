#include "materials/PBRMaterial.h"

#include "core/DescriptorAllocator.h"
#include "core/VulkanContext.h"

#include <cstring>
#include <vector>

PBRMaterial::PBRMaterial(VulkanContext& context, vk::DescriptorSetLayout layout, const std::array<Texture*, 5>& textures, const PBRFactors& factors) :
    m_context(context),
    m_factors(factors),
    m_textures(textures)
{
    CreateDescriptorPool();
    CreateUniformBuffer();
    CreateDescriptorSet(layout);
}

void PBRMaterial::CreateUniformBuffer()
{
    auto [buffer, memory] = m_context.CreateBuffer(sizeof(PBRFactors), 
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    m_factorsBuffer.buffer = std::move(buffer);
    m_factorsBuffer.memory = std::move(memory);
    m_factorsBuffer.mapped = m_factorsBuffer.memory.mapMemory(0, sizeof(PBRFactors));
    memcpy(m_factorsBuffer.mapped, &m_factors, sizeof(PBRFactors));
}

void PBRMaterial::CreateDescriptorPool()
{
    std::vector<vk::DescriptorPoolSize> poolSizes
    {
        {.type = vk::DescriptorType::eUniformBuffer,        .descriptorCount = 1},
        {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = static_cast<uint32_t>(m_textures.size())}
    };

    m_descriptorPool = DescriptorAllocator::CreatePool(m_context, 1, poolSizes);
}

void PBRMaterial::CreateDescriptorSet(vk::DescriptorSetLayout layout)
{
    // Create descriptor set
    auto sets = DescriptorAllocator::AllocateSets(m_context, m_descriptorPool, layout, 1);
    m_descriptorSet = std::move(sets[0]);

    // Prepare data
    vk::DescriptorBufferInfo bufferInfo
    {
        .buffer = m_factorsBuffer.buffer,
        .offset = 0,
        .range = sizeof(PBRFactors)
    };

    std::array<vk::DescriptorImageInfo, 5> imageInfos;
    for (size_t i = 0; i < m_textures.size(); i++)
    {
        imageInfos[i] = vk::DescriptorImageInfo
        {
            .sampler = m_textures[i]->GetSampler(),
            .imageView = m_textures[i]->GetImageView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
    }

    // Write UBO
    std::array<vk::WriteDescriptorSet, 6> writes;
    writes[0] = vk::WriteDescriptorSet
    {
        .dstSet = m_descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &bufferInfo
    };

    // Write image samplers
    for (size_t i = 0; i < imageInfos.size(); i++)
    {
        writes[i + 1] = vk::WriteDescriptorSet
        {
            .dstSet = m_descriptorSet,
            .dstBinding = static_cast<uint32_t>(i + 1),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfos[i]
        };
    }

    m_context.GetDevice().updateDescriptorSets(writes, {});
}
