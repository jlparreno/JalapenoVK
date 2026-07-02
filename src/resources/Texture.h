#pragma once

#include "core/VulkanIncludes.h"
#include "resources/Resource.h"

#include <ktx.h>

// Forward declarations
class VulkanContext;

/**
 * @brief GPU texture resource backed by a KTX file.
 *
 * Manages the full lifecycle of a Vulkan texture: loading pixel data from disk,
 * uploading it to GPU memory via a staging buffer, and exposing the resulting
 * image, image view, and sampler for use in descriptor sets.
 *
 * Inherits from Resource, so Load() / Unload() follow the standard resource
 * lifecycle. Requires a valid VulkanContext for all GPU operations.
 */
class Texture : public Resource
{

public:

    /**
     * @brief Constructs a Texture bound to a Vulkan context.
     *
     * @param context   The Vulkan context used for all GPU resource operations.
     * @param id        Unique resource identifier, used to resolve the file path.
     */
    explicit Texture(VulkanContext& context, const std::string& id) : Resource(id), m_context(context) {}

    /**
     * @brief Destructor. Ensures GPU resources are released via Unload().
     */
    ~Texture() { Unload(); }


    // ----------------------------------------------
    // RESOURCE OVERRIDES
    // ----------------------------------------------

    /**
     * @brief Loads the texture from disk and uploads it to the GPU.
     *
     * Reads a KTX file, creates a staging buffer, copies pixel data to a
     * device-local image, and sets up the image view and sampler.
     *
     * @return True if all GPU resources were created successfully, false otherwise.
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
     * @brief Loads KTX pixel data and copies it into a staging buffer.
     *
     * Resolves the file path from the resource ID, reads the KTX file,
     * maps a host-visible staging buffer, and memcpy's the pixel data into it.
     * Also populates m_width, m_height, and m_format from the KTX metadata.
     *
     * @param filePath  Absolute or relative path to the .ktx / .ktx2 file.
     */
    void LoadImageData(const std::string& filePath);

    /**
     * @brief Creates the GPU device-local image and performs the staging buffer upload.
     *
     * Allocates a device-local vk::Image, transitions its layout to eTransferDstOptimal, 
     * copies from the staging buffer, and transitions again to eShaderReadOnlyOptimal.
     *
     * @param stagingBuffer  Host-visible buffer containing the pixel data to upload.
     */
    void CreateVulkanImage(const vk::raii::Buffer& stagingBuffer);

    void CreateTextureImageView();

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
    vk::Format              m_format    { vk::Format::eUndefined };   // Pixel format of the texture, resolved from KTX metadata at load time.
    uint32_t                m_width     { 0 };                        // Image width in pixels
    uint32_t                m_height    { 0 };                        // Image height in pixels
    uint32_t                m_mipLevels { 1 };                        // Number of mip levels. Currently fixed at 1.
};