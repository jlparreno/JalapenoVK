#pragma once

#include "core/VulkanIncludes.h"

// By default, GLM uses OpenGL depth range from -1.0 to 1.0.
// Here we configure it to use the Vulkan range from 0.0 to 1.0
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <array>
#include <cstdint>

// Number of frames the CPU is allowed to prepare ahead of the GPU (double buffering).
static constexpr uint32_t k_maxFramesInFlight = 2;

// Forward declarations
class Mesh;

/**
 * @brief Per-vertex data layout passed to the Vulkan pipeline.
 *
 * Defines the attributes sent to the vertex shader for each vertex.
 * The static helpers describe this layout to Vulkan so the pipeline
 * knows how to read the vertex buffer at draw time.
 */
struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 normal;
	glm::vec4 tangent;

	/**
	 * @brief Returns the binding description for this vertex layout.
	 *
	 * Describes how the GPU should step through the vertex buffer:
	 * one Vertex per vertex (as opposed to per-instance).
	 */
	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	/**
	 * @brief Returns the attribute descriptions for this vertex layout.
	 *
	 * Maps each member to its shader location, format, and byte offset
	 * within the Vertex struct, so the pipeline can unpack each attribute.
	 */
	static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions()
	{
		return { {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,		.offset = offsetof(Vertex, position)},
				  {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,		.offset = offsetof(Vertex, color)},
				  {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,		.offset = offsetof(Vertex, texCoord)},
				  {.location = 3, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,		.offset = offsetof(Vertex, normal)},
				  {.location = 4, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat,	.offset = offsetof(Vertex, tangent)}} };
	}

	/**
	 * @brief Equality operator.
	 */
	bool operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};

/**
 * @brief One contiguous index sub-range of a Mesh, drawn with a single Material.
 *
 * A glTF mesh is a list of primitives, each referencing its own material. Mesh
 * concatenates every primitive's data into one shared vertex/index buffer pair,
 * so a Primitive only needs to record where its own slice of the index buffer
 * starts and which material to bind while drawing it.
 */
struct Primitive
{
	uint32_t	firstIndex{ 0 };		// Offset into the mesh's index buffer where this primitive's indices start.
	uint32_t	indexCount{ 0 };		// Number of indices belonging to this primitive.
	int			materialIndex{ -1 };	// Index into the mesh's material list, or -1 if the primitive declares no material.
};


struct Renderable
{
	Mesh*		mesh{ nullptr };
	glm::mat4   model{ 1.0f };
};

/**
 * @brief Per-frame scene data (set 0): camera view/projection + active light, shared by every draw this frame.
 *
 * Layout matches the std140-aligned UBO expected by the shader; the alignas(16)
 * qualifiers keep each element on a 16-byte boundary independent of the host
 * compiler's packing rules.
 */
struct SceneData
{
	alignas(16) glm::mat4 view;						// View matrix supplied by the camera.
	alignas(16) glm::mat4 proj;						// Perspective projection matrix.
	alignas(16) glm::vec4 lightDirection;			// xyz = world-space direction the light points towards; w unused.
	alignas(16) glm::vec4 lightColorIntensity;		// rgb = light color, w = intensity multiplier.
	alignas(16) glm::vec4 cameraPosition;			// xyz = world-space camera position, needed for the BRDF's view vector; w unused.
};

/**
 * @brief GPU-side uniform buffer paired with a persistent host-visible mapping.
 *
 * Owns the vk::Buffer and its backing memory, and keeps the mapping alive for
 * the lifetime of the buffer so per-frame updates can be written directly without repeated map/unmap calls.
 */
struct UBOBuffer
{
	vk::raii::Buffer		buffer{ nullptr };  // GPU buffer containing the uniform block.
	vk::raii::DeviceMemory	memory{ nullptr };  // GPU memory allocation backing the buffer.
	void*					mapped{ nullptr };  // Persistent host mapping used for per-frame writes.
};