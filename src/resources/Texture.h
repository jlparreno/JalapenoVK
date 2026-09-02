#pragma once

#include "core/VulkanIncludes.h"
#include "resources/Resource.h"

#include <glm/vec4.hpp>
#include <ktx.h>

// Forward declarations
class VulkanContext;

/**
 * @brief GPU texture resource.
 *
 * Manages the full lifecycle of a Vulkan texture: loading pixel data (from a
 * KTX2 file, a JPG/PNG via stb_image, or a procedural 1x1 solid color), uploading
 * it to GPU memory via a staging buffer, and exposing the resulting image, image
 * view, and sampler for use in descriptor sets.
 *
 * Inherits from Resource, so Load() / Unload() follow the standard resource
 * lifecycle. Requires a valid VulkanContext for all GPU operations.
 */
class Texture : public Resource
{

public:

    enum TextureColorSpace
    {
        sRGB,
        Linear
    };

    /**
     * @brief Constructs a Texture backed by a file on disk.
     *
     * Load() resolves the file from the resource ID: it tries assets/<id>.ktx2
     * first, then falls back to assets/<id>.jpg / .png via stb_image.
     *
     * @param context    The Vulkan context used for all GPU resource operations.
     * @param id         Unique resource identifier, used to resolve the file path (without extension).
     * @param colorSpace How to interpret JPG/PNG pixel data (ignored for KTX2, which carries its own format).
     *                   sRGB for albedo/emissive, Linear for normal/metallicRoughness/occlusion.
     */
    Texture(VulkanContext& context, const std::string& id, TextureColorSpace colorSpace = TextureColorSpace::sRGB);

    /**
     * @brief Constructs a procedural 1x1 solid-color Texture. No file is read.
     *
     * Used for material slot placeholders (e.g. white for a missing albedo/AO
     * map, flat normal for a missing normal map) where no source texture exists.
     *
     * @param context    The Vulkan context used for all GPU resource operations.
     * @param id         Unique resource identifier (used for ResourceManager lookup only, not a file path).
     * @param solidColor Pixel color, components in [0,1].
     * @param colorSpace sRGB or Linear interpretation of solidColor when picking the GPU format.
     */
    Texture(VulkanContext& context, const std::string& id, const glm::vec4& solidColor, TextureColorSpace colorSpace);

    /**
     * @brief Destructor. Ensures GPU resources are released via Unload().
     */
    ~Texture() { Unload(); }


    // ----------------------------------------------
    // RESOURCE OVERRIDES
    // ----------------------------------------------

    /**
     * @brief Loads the texture and uploads it to the GPU.
     *
     * Procedural textures (built via the solid-color constructor) skip the
     * filesystem entirely and go straight to LoadSolidColor(). File-backed
     * textures try assets/<id>.ktx2, then assets/<id>.jpg / .png, in that order;
     * throws if neither exists.
     *
     * @return True if all GPU resources were created successfully.
     */
    bool Load() override;

    /**
     * @brief Releases all GPU resources associated with this texture.
     *
     * Destroys the sampler, image view, image, and device memory in reverse
     * creation order. Safe to call if the texture was never loaded.
     */
    void Unload() override;


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the underlying Vulkan image handle.
     *
     * @return Raw vk::Image handle, valid only while the texture is loaded.
     */
    vk::Image       GetImage()      const { return m_image; }

    /**
     * @brief Returns the image view used to bind this texture in descriptor sets.
     *
     * @return Raw vk::ImageView handle, valid only while the texture is loaded.
     */
    vk::ImageView   GetImageView()  const { return m_imageView; }

    /**
     * @brief Returns the sampler defining filtering and wrapping behaviour.
     *
     * @return Raw vk::Sampler handle, valid only while the texture is loaded.
     */
    vk::Sampler     GetSampler()    const { return m_sampler; }

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Loads KTX2 pixel data and copies it into a staging buffer.
     *
     * Reads the KTX file, maps a host-visible staging buffer, and memcpy's the
     * pixel data into it. Populates m_width, m_height, m_mipLevels, and m_format
     * from the KTX2 metadata (m_colorSpace is not consulted — KTX2 carries its
     * own vkFormat, tagged correctly at asset-generation time).
     *
     * @param filePath  Path to the .ktx2 file.
     */
    void LoadImageDataKTX(const std::string& filePath);

    /**
     * @brief Loads JPG/PNG pixel data via stb_image and copies it into a staging buffer.
     *
     * Always decodes as RGBA8 (STBI_rgb_alpha), so a single mip level. Picks
     * m_format from m_colorSpace, since stb_image can't infer sRGB vs. linear
     * from the file itself.
     *
     * @param filePath  Path to the .jpg / .png file.
     */
    void LoadImageDataSTB(const std::string& filePath);

    /**
     * @brief Builds a procedural 1x1 image from m_solidColor. No file is read.
     *
     * Picks m_format from m_colorSpace, same as LoadImageDataSTB.
     */
    void LoadSolidColor();

    /**
     * @brief Runs the shared tail end of every Load*() path: image + view + sampler.
     *
     * Thin wrapper around CreateVulkanImage() / CreateTextureImageView() /
     * CreateTextureSampler(), in that order — identical across all three sources
     * once m_width/m_height/m_format/m_mipLevels and the staging buffer are ready.
     *
     * @param stagingBuffer  Host-visible buffer containing the pixel data to upload.
     */
    void CreateGPUResources(const vk::raii::Buffer& stagingBuffer);

    /**
     * @brief Creates the GPU device-local image and performs the staging buffer upload.
     *
     * Allocates a device-local vk::Image, transitions its layout to eTransferDstOptimal,
     * copies from the staging buffer, and transitions again to eShaderReadOnlyOptimal.
     *
     * @param stagingBuffer  Host-visible buffer containing the pixel data to upload.
     */
    void CreateVulkanImage(const vk::raii::Buffer& stagingBuffer);

    /**
     * @brief Creates the shader-accessible image view for the loaded texture.
     *
     * Wraps m_image in a 2D vk::ImageView with the format and mip level count
     * populated by whichever Load*() path ran, ready to be bound in descriptor sets.
     */
    void CreateTextureImageView();

    /**
     * @brief Creates the sampler used to read this texture in shaders.
     *
     * Configures filtering, addressing mode, and anisotropy according to the
     * engine's default sampling parameters.
     */
    void CreateTextureSampler();

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Vulkan context used for all GPU operations. Not owned by this class.
    VulkanContext&          m_context;

    // Core Vulkan GPU resources for textures
    vk::raii::Image         m_image     { nullptr };      // GPU image object containing pixel data
    vk::raii::DeviceMemory  m_memory    { nullptr };      // GPU memory allocation backing the image
    vk::raii::ImageView     m_imageView { nullptr };      // Shader-accessible view into the image
    vk::raii::Sampler       m_sampler   { nullptr };      // Sampling configuration (filtering, wrapping, etc.)

    // Texture metadata
    TextureColorSpace       m_colorSpace  { TextureColorSpace::sRGB };  // sRGB vs. linear; only consulted by the JPG/PNG and solid-color paths, KTX2 carries its own format.
    vk::Format              m_format      { vk::Format::eUndefined };   // Pixel format.
    uint32_t                m_width       { 0 };                        // Image width in pixels.
    uint32_t                m_height      { 0 };                        // Image height in pixels.
    uint32_t                m_mipLevels   { 1 };                        // Number of mip levels (always 1 outside the KTX2 path).

    // Procedural attributes, when no file backs the texture, Load() builds a 1x1 image from m_solidColor instead.
    bool                    m_isProcedural{ false };
    glm::vec4               m_solidColor  { 1.0f };
};