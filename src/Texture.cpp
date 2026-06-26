#include "Texture.h"
#include "VulkanContext.h"

bool Texture::Load()
{
    // Construct file path using resource ID and expected format
    std::string filePath = "assets/" + GetId() + ".ktx2";

    // Load raw image data from disk with format detection
    LoadImageData(filePath);

    return Resource::Load();    // Mark resource as successfully loaded
}

void Texture::Unload()
{
    // Only perform cleanup if resource is currently loaded
    if (IsLoaded()) 
    {
        // vk::raii handles destroy themselves — reset to release GPU resources explicitly
        m_sampler   = nullptr;
        m_imageView = nullptr;
        m_image     = nullptr;
        m_memory    = nullptr;

        // Update base class state to reflect unloaded status
        Resource::Unload();
    }
}

void Texture::LoadImageData(const std::string& filePath)
{
    // Load KTX2 texture
    ktxTexture* kTexture;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(filePath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);

    if (result != KTX_SUCCESS)
    {
        throw std::runtime_error("failed to load ktx texture image!");
    }

    // Get texture dimensions and data
    m_width = kTexture->baseWidth;
    m_height = kTexture->baseHeight;
    ktx_size_t   imageSize = ktxTexture_GetImageSize(kTexture, 0);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);

    // Create staging buffer and memory
    auto [stagingBuffer, stagingBufferMemory] = m_context.CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Copy image data to staging buffer
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, ktxTextureData, imageSize);
    stagingBufferMemory.unmapMemory();

    // Get mipmap levels. For now, we will use only one
    //mipLevels = kTexture->numLevels;

    // Check if the KTX texture has a format
    if (kTexture->classId == ktxTexture2_c)
    {
        // For KTX2 files, we can get the format directly
        auto* ktx2 = reinterpret_cast<ktxTexture2*>(kTexture);
        m_format = static_cast<vk::Format>(ktx2->vkFormat);
        if (m_format == vk::Format::eUndefined)
        {
            // If the format is undefined, fall back to a reasonable default
            m_format = vk::Format::eR8G8B8A8Unorm;
        }
    }
    else
    {
        // For KTX1 files or if we can't determine the format, use a reasonable default
        m_format = vk::Format::eR8G8B8A8Unorm;
    }

    // GPU image
    CreateVulkanImage(stagingBuffer);

    // Cleanup KTX resources
    ktxTexture_Destroy(kTexture);
}

void Texture::CreateVulkanImage(const vk::raii::Buffer& stagingBuffer)
{
    // Create the actual Image in GPU memory, that allows to receive a copy. Here CPU cannot write directly
    std::tie(m_image, m_memory) = m_context.CreateImage(
        m_width,
        m_height,
        m_mipLevels,
        vk::SampleCountFlagBits::e1,                                                // No multisample, its a texture
        m_format,																    // Image format
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,    // We will use image as destination (to copy info on it), and it will be sampled on shaders
        vk::MemoryPropertyFlagBits::eDeviceLocal);								    // GPU local (faster)

    // Perform the copy from the staging buffer to the GPU image
    m_context.TransitionImageLayout(m_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_mipLevels);
    m_context.CopyBufferToImage(stagingBuffer, m_image, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height));

    // Transition image layout for shader reading optimal.
    m_context.TransitionImageLayout(m_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, m_mipLevels);
}
