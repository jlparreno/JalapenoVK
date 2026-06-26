#pragma once

#include "VulkanIncludes.h"
#include "RenderTypes.h"
#include "Resource.h"

#include <ktx.h>

// Forward declarations
class VulkanContext;

/**
 * @brief GPU mesh resource loaded from a glTF file.
 *
 * Manages the full lifecycle of a Vulkan mesh: parsing vertex and index data
 * from disk, uploading it to device-local GPU buffers via staging, and exposing
 * the resulting handles for use during rendering.
 *
 * Inherits from Resource, so Load() / Unload() follow the standard resource
 * lifecycle. Requires a valid VulkanContext for all GPU operations.
 */
class Mesh : public Resource
{

public:

    /**
     * @brief Constructs a Mesh bound to a Vulkan context.
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
     * @brief Loads the mesh from disk and uploads it to the GPU.
     *
     * Parses a glTF file, extracts vertex and index data, and uploads both
     * to device-local GPU buffers via staging buffers.
     *
     * @return True if all GPU resources were created successfully, false otherwise.
     */
    bool Load() override;

    /**
     * @brief Releases all GPU resources associated with this mesh.
     *
     * Destroys vertex and index buffers and their backing memory in reverse
     * creation order. Safe to call if the mesh was never loaded.
     */
    void Unload() override;


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Returns the GPU buffer containing per-vertex attribute data.
     */
    vk::Buffer GetVertexBuffer() const { return m_vertexBuffer; }

    /**
     * @brief Returns the GPU buffer containing triangle index data.
     */
    vk::Buffer GetIndexBuffer()  const { return m_indexBuffer; }

    /**
     * @brief Returns the number of vertices in this mesh.
     */
    uint32_t   GetVertexCount()  const { return m_vertexCount; }

    /**
     * @brief Returns the number of indices in this mesh (typically 3 per triangle).
     */
    uint32_t   GetIndexCount()   const { return m_indexCount; }

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Parses a glTF file and extracts vertex and index data into CPU vectors.
     *
     * Reads positions, normals, and UV coordinates from the glTF accessors and
     * populates @p vertices and @p indices. Deduplicates vertices using a hash map.
     *
     * @param filePath  Path to the .glb / .gltf file to load.
     * @param vertices  Output vector populated with unique vertex data.
     * @param indices   Output vector populated with triangle indices.
     * 
     * @return True if parsing succeeded, false otherwise.
     */
    bool LoadMeshData(const std::string& filePath, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

    /**
    * @brief Creates a device-local vertex buffer and uploads vertex data to it.
    *
    * Allocates a host-visible staging buffer, copies @p vertices into it,
    * then transfers the data to a device-local buffer for optimal GPU access.
    *
    * @param vertices  CPU-side vertex data to upload.
    */
    void CreateVertexBuffer(const std::vector<Vertex>& vertices);

    /**
     * @brief Creates a device-local index buffer and uploads index data to it.
     *
     * Allocates a host-visible staging buffer, copies @p indices into it,
     * then transfers the data to a device-local buffer for optimal GPU access.
     *
     * @param indices   CPU-side index data to upload.
     */
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