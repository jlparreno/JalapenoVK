#include "resources/Texture.h"
#include "core/VulkanContext.h"
#include "core/VulkanTypes.h"

// stb_image is header-only: the implementation must be emitted in exactly one TU.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <filesystem>

Texture::Texture(VulkanContext& context, const std::string& id, ColorSpace colorSpace) : 
    Resource(id), 
    m_context(context),
    m_colorSpace(colorSpace)
{
}

Texture::Texture(VulkanContext& context, const std::string& id, const glm::vec4& solidColor, ColorSpace colorSpace) :
    Resource(id),
    m_context(context),
    m_colorSpace(colorSpace),
    m_isProcedural(true),
    m_solidColor(solidColor)
{
}

bool Texture::Load()
{
    // Procedural placeholder: no file to resolve, build the 1x1 image directly.
    if (m_isProcedural)
    {
        LoadSolidColor();
        return Resource::Load();
    }

    // Construct file path using resource ID and expected format. First, try KTX2
    const std::string ktxPath = "assets/" + GetId() + ".ktx2";
    if (std::filesystem::exists(ktxPath))
    {
        LoadImageDataKTX(ktxPath);
        return Resource::Load();// Mark resource as successfully loaded
    }

    // Then, try jpg or png
    for (const char* ext : { ".jpg", ".png" })
    {
        const std::string path = "assets/" + GetId() + ext;
        if (std::filesystem::exists(path))
        {
            LoadImageDataSTB(path);
            return Resource::Load();// Mark resource as successfully loaded
        }
    }

    throw std::runtime_error("Texture not found for id: " + GetId());
}

void Texture::Unload()
{
    // Only perform cleanup if resource is currently loaded
    if (IsLoaded()) 
    {
        // vk::raii handles destroy themselves � reset to release GPU resources explicitly
        m_sampler   = nullptr;
        m_imageView = nullptr;
        m_image     = nullptr;
        m_memory    = nullptr;

        // Update base class state to reflect unloaded status
        Resource::Unload();
    }
}

void Texture::LoadImageDataKTX(const std::string& filePath)
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
    m_mipLevels = kTexture->numLevels;

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

    CreateGPUResources(stagingBuffer);

    // Cleanup KTX resources
    ktxTexture_Destroy(kTexture);
}

void Texture::LoadImageDataSTB(const std::string& filePath)
{
    // Load STB texture
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        throw std::runtime_error("Failed to load STB texture image: " + filePath);
    }

    // Get texture dimensions and data. STBI_rgb_alpha forces 4 output channels regardless of the
    // source file's own channel count, which is what `channels` reports here - use 4, not `channels`.
    m_width = width;
    m_height = height;
    size_t imageSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    // Create staging buffer and memory
    auto [stagingBuffer, stagingBufferMemory] = m_context.CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Copy image data to staging buffer
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    // Get mipmap levels. For now, we will use only one for JPG or PNG
    m_mipLevels = 1;

    // Select format, sRGB by default
    switch (m_colorSpace)
    {
        case ColorSpace::sRGB:
            m_format = vk::Format::eR8G8B8A8Srgb;
            break;
        case ColorSpace::Linear:
            m_format = vk::Format::eR8G8B8A8Unorm;
            break;
        default:
            m_format = vk::Format::eR8G8B8A8Srgb;
            break;
    }

    CreateGPUResources(stagingBuffer);

    // Cleanup STB resources
    stbi_image_free(pixels);
}

void Texture::LoadSolidColor()
{
    m_width     = 1;
    m_height    = 1;
    m_mipLevels = 1;

    // Select format, sRGB by default
    switch (m_colorSpace)
    {
        case ColorSpace::sRGB:
            m_format = vk::Format::eR8G8B8A8Srgb;
            break;
        case ColorSpace::Linear:
            m_format = vk::Format::eR8G8B8A8Unorm;
            break;
        default:
            m_format = vk::Format::eR8G8B8A8Srgb;
            break;
    }

    // Create pixel color data
    const std::array<uint8_t, 4> pixel
    {
        static_cast<uint8_t>(std::clamp(m_solidColor.r, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(m_solidColor.g, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(m_solidColor.b, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(m_solidColor.a, 0.0f, 1.0f) * 255.0f)
    };

    // Create staging buffer and memory
    auto [stagingBuffer, stagingBufferMemory] = m_context.CreateBuffer(pixel.size(), vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Copy the single pixel into the staging buffer
    void* data = stagingBufferMemory.mapMemory(0, pixel.size());
    memcpy(data, pixel.data(), pixel.size());
    stagingBufferMemory.unmapMemory();

    CreateGPUResources(stagingBuffer);
}

void Texture::CreateGPUResources(const vk::raii::Buffer& stagingBuffer)
{
    CreateVulkanImage(stagingBuffer);
    CreateTextureImageView();
    CreateTextureSampler();
}

void Texture::CreateVulkanImage(const vk::raii::Buffer& stagingBuffer)
{
    // Create the actual Image in GPU memory, that allows to receive a copy. Here CPU cannot write directly
    ImageDescription imageDesc =
    {
        .width = m_width,
        .height = m_height,
        .format = m_format,
        .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        .mipLevels = m_mipLevels
    };

    std::tie(m_image, m_memory) = m_context.CreateImage(imageDesc);

    // Perform the copy from the staging buffer to the GPU image
    m_context.TransitionImageLayout(m_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, m_mipLevels);
    m_context.CopyBufferToImage(stagingBuffer, m_image, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height));

    // Transition image layout for shader reading optimal.
    m_context.TransitionImageLayout(m_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, m_mipLevels);
}

void Texture::CreateTextureImageView()
{
    ImageViewDescription viewDesc =
    {
        .image = *m_image,
        .format = m_format,
        .levelCount = m_mipLevels  // Specify miplevels because it is a texture and can have more than 1 (default)
    };

    m_imageView = m_context.CreateImageView(viewDesc);
}

void Texture::CreateTextureSampler()
{
    vk::PhysicalDeviceProperties properties = m_context.GetPhysicalDevice().getProperties();

    // This sampler is configured to:
    //   - Use linear filtering for magnification and minification.
    //   - Use linear interpolation between mip levels.
    //   - Repeat texture coordinates outside the [0,1] range.
    //   - Enable anisotropic filtering using the maximum level supported
    //     by the physical device.
    //   - Enable full range of mipmap levels to be used with minLod and maxLod
    vk::SamplerCreateInfo samplerInfo
    {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = vk::LodClampNone
    };

    m_sampler = vk::raii::Sampler(m_context.GetDevice(), samplerInfo);
}
