#pragma once

#include "core/VulkanIncludes.h"

#include <cstdint>

/**
 * @brief Everything CreateImage() needs to build one image.
 *
 * The first group is what every call site has to state: the extent, the format,
 * and what the image is for. Only depth carries a default there, since it stays
 * 1 for anything that is not a 3D image.
 *
 * The second group describes what makes an image special, and defaults to the
 * plain case: one mip level, one array layer, 2D, not multisampled, GPU-local.
 */
struct ImageDescription
{
    uint32_t                width       { 0 };                                          // Width in pixels.
    uint32_t                height      { 0 };                                          // Height in pixels.
    uint32_t                depth       { 1 };                                          // Slices for a 3D image. Stays 1 for 2D images and cubemaps.
    vk::Format              format      { vk::Format::eUndefined };                     // Pixel format.
    vk::ImageUsageFlags     usage       { };                                            // What the image will be used for. Vulkan rejects an empty mask, so this is always set by the caller.

    uint32_t                mipLevels   { 1 };                                          // Mip levels to allocate. More than one for a chain sampled per level, such as prefiltered roughness.
    uint32_t                arrayLayers { 1 };                                          // Array layers. At least 6 for a cubemap, one per face.
    vk::ImageType           imageType   { vk::ImageType::e2D };                         // Always 2D, nothing needs 1D or 3D for now.
    vk::SharingMode         sharingMode { vk::SharingMode::eExclusive };                // Single queue family in this engine (graphics + present). eConcurrent would also need the queue family indices.
    vk::SampleCountFlagBits samples     { vk::SampleCountFlagBits::e1 };                // MSAA count. e1 means not multisampled, which is what any sampled texture wants.
    vk::ImageTiling         tiling      { vk::ImageTiling::eOptimal };                  // Memory layout. Always eOptimal unless the CPU has to read the pixels in place.
    vk::ImageCreateFlags    flags       { };                                            // Optional capabilities to opt into. eCubeCompatible is what allows an eCube view over this image.
    vk::MemoryPropertyFlags memory      { vk::MemoryPropertyFlagBits::eDeviceLocal };   // Where the backing memory lives. Device-local is fastest for the GPU and suits anything the CPU does not write every frame.
};

/**
 * @brief Everything CreateImageView() needs to build one view: which image, read as what, and over which part of it.
 */
struct ImageViewDescription
{
    vk::Image            image          { nullptr };                                    // Image this view looks into.
    vk::Format           format         { vk::Format::eUndefined };                     // How to interpret its pixels.
    vk::ImageAspectFlags aspect         { vk::ImageAspectFlagBits::eColor };            // Which aspect to read. Must be set to eDepth for a depth image: the default only suits color.

    vk::ImageViewType    viewType       { vk::ImageViewType::e2D };                     // How shaders address it.
    uint32_t             baseMipLevel   { 0 };                                          // First mip level this view exposes. Selects the level when rendering into one.
    uint32_t             levelCount     { 1 };                                          // How many levels from baseMipLevel. Must cover the image's whole chain to sample all of it.
    uint32_t             baseArrayLayer { 0 };                                          // First array layer. Selects the face when targeting one side of a cubemap.
    uint32_t             layerCount     { 1 };                                          // How many layers from baseArrayLayer. Exactly 6 for an eCube view.
};

/**
 * @brief Everything TransitionImageLayout() needs to record one image barrier.
 *
 * Grouped as the barrier reads: the image, what it was before (layout, and the stage
 * and access to wait for), what it will be after (layout, and the stage and access
 * to hold back), and the part of the image affected.
 */
struct ImageTransition
{
    vk::Image               image           { nullptr };                                // Image to transition.

    vk::ImageLayout         oldLayout       { vk::ImageLayout::eUndefined };            // Current layout. eUndefined discards the contents.
    vk::PipelineStageFlags2 srcStageMask    { vk::PipelineStageFlagBits2::eNone };      // Stages that must finish with the image before the barrier.
    vk::AccessFlags2        srcAccessMask   { vk::AccessFlagBits2::eNone };             // Writes those stages made that must be made available.

    vk::ImageLayout         newLayout       { vk::ImageLayout::eUndefined };            // Target layout. Always set by the caller.
    vk::PipelineStageFlags2 dstStageMask    { vk::PipelineStageFlagBits2::eNone };      // Stages that must wait for the barrier before using the image.
    vk::AccessFlags2        dstAccessMask   { vk::AccessFlagBits2::eNone };             // Accesses those stages will make, which the barrier makes the data visible to.

    vk::ImageAspectFlags    aspect          { vk::ImageAspectFlagBits::eColor };        // Which aspect. Must be set to eDepth for a depth image: the default only suits color.
    uint32_t                baseMipLevel    { 0 };                                      // First mip level affected.
    uint32_t                levelCount      { 1 };                                      // How many levels from baseMipLevel.
    uint32_t                baseArrayLayer  { 0 };                                      // First array layer affected.
    uint32_t                layerCount      { 1 };                                      // How many layers from baseArrayLayer. 6 for a whole cube.
};
