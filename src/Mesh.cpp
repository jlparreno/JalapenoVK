#include "Mesh.h"
#include "VulkanContext.h"

#include <tiny_gltf.h>

bool Mesh::Load()
{
    // Construct file path using standardized naming convention
    std::string filePath = "assets/" + GetId() + ".glb";

    // Parse geometric data from file format into CPU-accessible structures
    std::vector<Vertex>     vertices;   // Temporary CPU storage for vertex attributes
    std::vector<uint32_t>   indices;    // Temporary CPU storage for triangle indices

    if (!LoadMeshData(filePath, vertices, indices)) 
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

bool Mesh::LoadMeshData(const std::string& filePath, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	tinygltf::Model    model;
	tinygltf::TinyGLTF loader;
	std::string        err;
	std::string        warn;

	bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);

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

	// Process all meshes in the model
	for (const auto& mesh : model.meshes)
	{
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

			// Get texture coordinates if available
			bool                        hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
			const tinygltf::Accessor* texCoordAccessor = nullptr;
			const tinygltf::BufferView* texCoordBufferView = nullptr;
			const tinygltf::Buffer* texCoordBuffer = nullptr;

			if (hasTexCoords)
			{
				texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
			}

			uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

			for (size_t i = 0; i < posAccessor.count; i++)
			{
				Vertex vertex{};

				const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]);
				// glTF uses a right-handed coordinate system with Y-up
				// Vulkan uses a right-handed coordinate system with Y-down
				// We need to flip the Y coordinate
				vertex.position = { pos[0], -pos[1], pos[2] };

				if (hasTexCoords)
				{
					const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
					vertex.texCoord = { texCoord[0], texCoord[1] };
				}
				else
				{
					vertex.texCoord = { 0.0f, 0.0f };
				}

				vertex.color = { 1.0f, 1.0f, 1.0f };

				vertices.push_back(vertex);
			}

			const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
			size_t               indexCount = indexAccessor.count;
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

			indices.reserve(indices.size() + indexCount);

			for (size_t i = 0; i < indexCount; i++)
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
		}
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
