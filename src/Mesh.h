#pragma once

#include "VulkanIncludes.h"
#include "RenderTypes.h"
#include "Resource.h"

#include <ktx.h>

// Forward declarations
class VulkanContext;

class Mesh : public Resource
{

public:

    /**
     * @brief Constructs a Texture bound to a Vulkan context.
     *
     * @param context   The Vulkan context used for all GPU resource operations.
     * @param id        Unique resource identifier, used to resolve the file path.
     */
    explicit Mesh(VulkanContext& context, const std::string& id) : Resource(id), m_context(context) {}

    /**
     * @brief Destructor. Ensures GPU resources are released via Unload().
     */
    ~Mesh() { Unload(); }


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

    vk::Buffer  GetVertexBuffer() const { return m_vertexBuffer; }
    vk::Buffer  GetIndexBuffer() const { return m_indexBuffer; }
    uint32_t    GetVertexCount() const { return m_vertexCount; }
    uint32_t    GetIndexCount() const { return m_indexCount; }

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    bool LoadMeshData(const std::string& filePath, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

    void CreateVertexBuffer(const std::vector<Vertex>& vertices);

    void CreateIndexBuffer(const std::vector<uint32_t>& indices);

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Vulkan context used for all GPU operations. Not owned by this class.
    VulkanContext&          m_context;

    // Vertex data management - stores per-vertex attributes like position, normal, UV coordinates
    vk::raii::Buffer        m_vertexBuffer      { nullptr };    // GPU buffer containing vertex attribute data
    vk::raii::DeviceMemory  m_vertexBufferMemory{ nullptr };    // GPU memory backing the vertex buffer
    uint32_t                m_vertexCount       { 0 };          // Number of vertices in this mesh

    // Index data management - defines triangle connectivity using vertex indices
    vk::raii::Buffer        m_indexBuffer       { nullptr };    // GPU buffer containing triangle index data
    vk::raii::DeviceMemory  m_indexBufferMemory { nullptr };    // GPU memory backing the index buffer
    
    uint32_t                m_indexCount        { 0 };          // Number of indices in this mesh (typically 3 per triangle)
};