#include "resources/Mesh.h"

#include "core/VulkanContext.h"
#include "resources/ResourceManager.h"
#include "materials/PBRMaterial.h"

#include <tiny_gltf.h>
#include <filesystem>

Mesh::Mesh(VulkanContext& context, const std::string& id, ResourceManager& resourceManager, vk::DescriptorSetLayout layout) :
	Resource(id),
	m_context(context),
	m_resourceManager(resourceManager),
	m_materialLayout(layout)
{
}

bool Mesh::Load()
{
    // Construct file path using standardized naming convention
	std::string filePath = "assets/" + GetId() + ".glb";
	bool        isBinary = true;

	// .glb first, fall back to .gltf; fail if neither exists.
	if (!std::filesystem::exists(filePath))
	{
		filePath = "assets/" + GetId() + ".gltf";
		isBinary = false;

		if (!std::filesystem::exists(filePath))
		{
			return false;
		}
	}

    // Parse geometric data from file format into CPU-accessible structures
    std::vector<Vertex>     vertices;   // Temporary CPU storage for vertex attributes
    std::vector<uint32_t>   indices;    // Temporary CPU storage for triangle indices

    if (!LoadMeshData(filePath, isBinary, vertices, indices, m_primitives)) 
    {
        return false;                   // Failed to parse file - abort loading
    }

    // Transform CPU data into optimized GPU buffer resources
    CreateVertexBuffer(vertices);       // Upload vertex attributes to GPU
    CreateIndexBuffer(indices);         // Upload triangle connectivity to GPU

    // Cache metadata for efficient rendering operations
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());

    return Resource::Load();            // Mark resource as successfully loaded
}

void Mesh::Unload()
{
    // Only proceed with cleanup if resources are currently loaded
    if (IsLoaded()) 
    {
        // Destroy buffers and free GPU memory in proper sequence
        // Index resources cleaned up first to maintain clear dependency order
        m_indexBuffer = nullptr;			// Destroy index buffer object
        m_indexBufferMemory = nullptr;      // Release index buffer memory

        // Vertex resources cleaned up second
        m_vertexBuffer = nullptr;			// Destroy vertex buffer object
        m_vertexBufferMemory = nullptr;     // Release vertex buffer memory

        // Update base class state to reflect unloaded condition
        Resource::Unload();
    }
}

Material* Mesh::GetMaterial(int materialIndex) const
{
	if (materialIndex >= 0 && materialIndex < static_cast<int>(m_materials.size()))
	{
		return m_materials[materialIndex].get();
	}

	return nullptr;
}

bool Mesh::LoadMeshData(const std::string& filePath, bool isBinary, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<Primitive>& primitives)
{
	tinygltf::Model    model;
	tinygltf::TinyGLTF loader;
	std::string        err;
	std::string        warn;

	// Load with the correct method depending on the input file
	bool ret = false;
	if (isBinary)
	{
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
	}
	else
	{
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
	}

	if (!warn.empty())
	{
		std::cout << "glTF warning: " << warn << std::endl;
	}

	if (!err.empty())
	{
		std::cout << "glTF error: " << err << std::endl;
	}

	if (!ret)
	{
		std::cout << "Failed to load glTF model" << std::endl;
		return false;
	}

	vertices.clear();
	indices.clear();
	primitives.clear();
	m_materials.clear();

	// Process all meshes in the model
	for (const auto& mesh : model.meshes)
	{
		// Reserve primitives space
		primitives.reserve(mesh.primitives.size());

		for (const auto& primitive : mesh.primitives)
		{
			// Get indices
			const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

			// Get vertex positions
			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

			// Get normals
			const tinygltf::Accessor& normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];;
			const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
			const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];

			// Get texture coordinates if available
			bool hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
			const tinygltf::Accessor* texCoordAccessor = nullptr;
			const tinygltf::BufferView* texCoordBufferView = nullptr;
			const tinygltf::Buffer* texCoordBuffer = nullptr;

			// Get tangents if available
			bool hasTangents = primitive.attributes.find("TANGENT") != primitive.attributes.end();
			const tinygltf::Accessor* tangentAccessor = nullptr;
			const tinygltf::BufferView* tangentBufferView = nullptr;
			const tinygltf::Buffer* tangentBuffer = nullptr;

			if (hasTexCoords)
			{
				texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
			}

			if (hasTangents)
			{
				tangentAccessor = &model.accessors[primitive.attributes.at("TANGENT")];
				tangentBufferView = &model.bufferViews[tangentAccessor->bufferView];
				tangentBuffer = &model.buffers[tangentBufferView->buffer];
			}

			// Vertices
			vertices.reserve(vertices.size() + posAccessor.count);
			uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

			for (size_t i = 0; i < posAccessor.count; i++)
			{
				Vertex vertex{};

				// POS
				const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]);
				// glTF is Y-up. We keep the model in its native Y-up world space and rely on the
				// projection matrix (proj[1][1] *= -1 in the renderer) to handle Vulkan's Y-down NDC.
				vertex.position = { pos[0], pos[1], pos[2] };

				// TEXCOORD
				if (hasTexCoords)
				{
					const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
					vertex.texCoord = { texCoord[0], texCoord[1] };
				}
				else
				{
					vertex.texCoord = { 0.0f, 0.0f };
				}

				// COLOR
				vertex.color = { 1.0f, 1.0f, 1.0f };

				// NORMAL
				const float* normal = reinterpret_cast<const float*>(&normalBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset + i * 12]);
				vertex.normal = { normal[0], normal[1], normal[2] };

				// TANGENT
				if (hasTangents)
				{
					const float* tangent = reinterpret_cast<const float*>(&tangentBuffer->data[tangentBufferView->byteOffset + tangentAccessor->byteOffset + i * 16]);
					vertex.tangent = { tangent[0], tangent[1], tangent[2], tangent[3] };
				}
				else
				{
					// No TANGENT in glTF: compute an arbitrary-but-valid tangent perpendicular to the normal, 
					const glm::vec3 helper = (glm::abs(vertex.normal.z) < 0.999f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
					const glm::vec3 t = glm::normalize(glm::cross(helper, vertex.normal));
					vertex.tangent = glm::vec4(t, 1.0f);
				}

				vertices.push_back(vertex);
			}

			// Indices
			const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
			size_t               indexStride = 0;

			// Determine index stride based on component type
			if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				indexStride = sizeof(uint16_t);
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			{
				indexStride = sizeof(uint32_t);
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				indexStride = sizeof(uint8_t);
			}
			else
			{
				std::cout << "Unsupported index component type" << std::endl; 
				return false;
				
			}

			indices.reserve(indices.size() + indexAccessor.count);
			uint32_t baseIndex = static_cast<uint32_t>(indices.size());

			for (size_t i = 0; i < indexAccessor.count; i++)
			{
				uint32_t index = 0;

				if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					index = *reinterpret_cast<const uint16_t*>(indexData + i * indexStride);
				}
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					index = *reinterpret_cast<const uint32_t*>(indexData + i * indexStride);
				}
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					index = *reinterpret_cast<const uint8_t*>(indexData + i * indexStride);
				}

				indices.push_back(baseVertex + index);
			}

			// Add primitive data
			primitives.push_back({ static_cast<uint32_t>(baseIndex), static_cast<uint32_t>(indexAccessor.count), primitive.material });
		}
	}

	// Create all required materials
	for (const auto& material : model.materials)
	{
		// Load textures
		Texture* albedoTex		= ResolveTextureSlot(model, material.pbrMetallicRoughness.baseColorTexture.index,			Texture::sRGB,		"placeholder_white");
		Texture* normalTex		= ResolveTextureSlot(model, material.normalTexture.index,									Texture::Linear,	"placeholder_normal");
		Texture* metRoughTex	= ResolveTextureSlot(model, material.pbrMetallicRoughness.metallicRoughnessTexture.index,	Texture::Linear,	"placeholder_white");
		Texture* occlusionTex	= ResolveTextureSlot(model, material.occlusionTexture.index,								Texture::Linear,	"placeholder_white");
		Texture* emissiveTex	= ResolveTextureSlot(model, material.emissiveTexture.index,									Texture::sRGB,		"placeholder_white");

		std::array<Texture*, 5> pbrTextures{ albedoTex, normalTex, metRoughTex, occlusionTex, emissiveTex };

		// Load factors
		const auto& baseColorFactor = material.pbrMetallicRoughness.baseColorFactor;
		glm::vec4 albedoFactors(baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], baseColorFactor[3]);
		glm::vec4 metallicRoughnessFactors(material.pbrMetallicRoughness.metallicFactor, material.pbrMetallicRoughness.roughnessFactor, 1.0, 1.0);
		glm::vec4 emissiveFactors(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2], 1.0);

		PBRMaterial::PBRFactors pbrFactors{ albedoFactors, metallicRoughnessFactors, emissiveFactors };

		auto new_material = std::make_unique<PBRMaterial>(m_context, m_materialLayout, pbrTextures, pbrFactors);
		m_materials.push_back(std::move(new_material));
	}

	return true;
}

void Mesh::CreateVertexBuffer(const std::vector<Vertex>& vertices)
{
	// Define buffer size using the source data
	vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// Create the staging buffer, temporal buffer where CPU can write directly
																				   //Source of a transfer (staging)   	  // CPU can access the memory			     // CPU Coherent
	auto [stagingBuffer, stagingBufferMemory] = m_context.CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// Here we ask Vulkan a pointer to write to and copy the vertices data into it. Vertices CPU -> Staging buffer
	void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(dataStaging, vertices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	// Create the actual buffer in GPU memory, that allows to receive a copy. Here CPU cannot write directly
																						// Will be a vertex buffer   			 // We will copy info to it			    // GPU local (faster)
	std::tie(m_vertexBuffer, m_vertexBufferMemory) = m_context.CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Copy the contents of the staging into GPU local memory
	m_context.CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
}

void Mesh::CreateIndexBuffer(const std::vector<uint32_t>& indices)
{
	// Define buffer size using the source data
	vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	// Create the staging buffer, temporal buffer where CPU can write directly
																				   // Source of a transfer (staging)   	  // CPU can access the memory			     // CPU Coherent
	auto [stagingBuffer, stagingBufferMemory] = m_context.CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// Here we ask Vulkan a pointer to write to and copy the vertices data into it. Vertices CPU -> Staging buffer
	void* data = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(data, indices.data(), (size_t)bufferSize);
	stagingBufferMemory.unmapMemory();

	// Create the actual buffer in GPU memory, that allows to receive a copy. Here CPU cannot write directly
																					  // Will be an index buffer   			  // We will copy info to it			 // GPU local (faster)
	std::tie(m_indexBuffer, m_indexBufferMemory) = m_context.CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Copy the contents of the staging into GPU local memory
	m_context.CopyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
}

Texture* Mesh::ResolveTextureSlot(const tinygltf::Model& model, int textureIndex, Texture::ColorSpace colorSpace, const std::string& placeholderId)
{
	if (textureIndex < 0)
	{
		// Slot absent in this material, use the shared placeholder
		return m_resourceManager.GetResource<Texture>(placeholderId);
	}

	const tinygltf::Image& image = model.images[model.textures[textureIndex].source];

	// If texture path is missing, reuse the mesh's own id.
	std::string textureId = image.uri.empty() ? GetId() : ResolveTextureId(image.uri);

	return m_resourceManager.LoadResource<Texture>(m_context, textureId, colorSpace).Get();
}

std::string Mesh::ResolveTextureId(const std::string& uri) const
{
	std::filesystem::path meshFolder = std::filesystem::path(GetId()).parent_path();
	std::filesystem::path textureName = std::filesystem::path(uri).stem();

	return (meshFolder / textureName).generic_string();
}
