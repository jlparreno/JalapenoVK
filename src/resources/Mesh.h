#pragma once

#include "core/VulkanIncludes.h"
#include "render/RenderTypes.h"
#include "resources/Resource.h"
#include "resources/Texture.h"
#include "materials/Material.h"

// Forward declarations
class VulkanContext;
class ResourceManager;
namespace tinygltf { class Model; }

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
     * @param context           The Vulkan context used for all GPU resource operations.
     * @param id                Unique resource identifier, used to resolve the file path.
     * @param resourceManager   Used to load per-material textures (and their placeholders) as the glTF is parsed.
     * @param layout            Shared PBR material descriptor set layout (see PBRMaterial::CreateSetLayout), needed to build this mesh's materials.
     */
    explicit Mesh(VulkanContext& context, const std::string& id, ResourceManager& resourceManager, vk::DescriptorSetLayout layout);

    /**
     * @brief Destructor. Ensures GPU resources are released via Unload().
     */
    ~Mesh() { Unload(); }

    // Non-copyable / non-movable (owns vk::raii handles)
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&)                 = delete;
    Mesh& operator=(Mesh&&)      = delete;


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

    /**
     * @brief Returns this mesh's primitives (index sub-ranges + material index), in glTF order.
     */
    const std::vector<Primitive>& GetPrimitives() const { return m_primitives; }

    /**
     * @brief Returns the material at the given index, or nullptr if out of range.
     *
     * @param materialIndex  Index into this mesh's material list, typically a Primitive::materialIndex (may be -1).
     *
     * @return Raw Material pointer, valid for the lifetime of this Mesh, or nullptr.
     */
    Material* GetMaterial(int materialIndex) const;

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Parses a glTF file, extracts vertex/index/primitive data into CPU vectors, and builds this mesh's materials.
     *
     * Reads positions, normals, UVs, and tangents from the glTF accessors and populates vertices, indices, and primitives. 
     * Also walks model.materials and builds one PBRMaterial per entry, resolving each texture slot.
     *
     * @param filePath  Path to the .glb / .gltf file to load.
     * @param isBinary  True to parse as .glb (LoadBinaryFromFile), false as .gltf (LoadASCIIFromFile).
     * @param vertices  Output vector populated with vertex data.
     * @param indices   Output vector populated with triangle indices.
     * @param primitives Output vector populated with one Primitive per glTF primitive.
     *
     * @return True if parsing succeeded, false otherwise.
     */
    bool LoadMeshData(const std::string& filePath, bool isBinary, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<Primitive>& primitives);

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

    /**
     * @brief Resolves one glTF material texture slot to a Texture*, falling back to a placeholder when absent.
     *
     * If textureIndex is -1 (slot not used by this material), returns the shared placeholder
     * Otherwise resolves the referenced image's URI. Then loads that Texture through m_resourceManager.
     *
     * @param model         The glTF model currently being parsed (owns the textures/images arrays).
     * @param textureIndex  Index into model.textures, or -1 if this material has no texture for this slot.
     * @param colorSpace    sRGB for albedo/emissive, Linear for normal/metallicRoughness/occlusion.
     * @param placeholderId Id of the shared placeholder Texture to use when textureIndex is -1.
     *
     * @return Non-owning Texture*, valid for the lifetime of the ResourceManager.
     */
    Texture* ResolveTextureSlot(const tinygltf::Model& model, int textureIndex, Texture::ColorSpace colorSpace, const std::string& placeholderId);

    /**
     * @brief Resolves a glTF image URI to this mesh's texture-id convention.
     *
     * Ids are relative to the folder the glTF file itself lives in, with the extension stripped
     * Texture::Load() re-appends whichever extension it actually finds on disk.
     *
     * @param uri  The image's uri as declared in the glTF (e.g. "Default_albedo.jpg").
     * @return     The resolved texture id (e.g. "DamagedHelmet/glTF/Default_albedo").
     */
    std::string ResolveTextureId(const std::string& uri) const;

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    VulkanContext&          m_context;                          // Vulkan context used for all GPU operations. Not owned by this class.
    ResourceManager&        m_resourceManager;                  // Used to load per-material textures. Not owned by this class.

    vk::DescriptorSetLayout                 m_materialLayout;   // Shared PBR material set layout.
    std::vector<Primitive>                  m_primitives;       // One entry per glTF primitive, in glTF order.
    std::vector<std::unique_ptr<Material>>  m_materials;        // One entry per glTF material, indexed by Primitive::materialIndex. Owned by this class.

    // Vertex data management - stores per-vertex attributes like position, normal, UV coordinates
    vk::raii::Buffer        m_vertexBuffer      { nullptr };    // GPU buffer containing vertex attribute data
    vk::raii::DeviceMemory  m_vertexBufferMemory{ nullptr };    // GPU memory backing the vertex buffer
    uint32_t                m_vertexCount       { 0 };          // Number of vertices in this mesh

    // Index data management - defines triangle connectivity using vertex indices
    vk::raii::Buffer        m_indexBuffer       { nullptr };    // GPU buffer containing triangle index data
    vk::raii::DeviceMemory  m_indexBufferMemory { nullptr };    // GPU memory backing the index buffer
    uint32_t                m_indexCount        { 0 };          // Number of indices in this mesh (typically 3 per triangle)
};