#pragma once

// By default, GLM uses OpenGL depth range from -1.0 to 1.0.
// Here we configure it to use the Vulkan range from 0.0 to 1.0
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulkanIncludes.h"

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
	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return { {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,	.offset = offsetof(Vertex, position)},
				  {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,	.offset = offsetof(Vertex, color)},
				  {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,	.offset = offsetof(Vertex, texCoord)}} };
	}

	/**
	 * @brief Equality operator.
	 */
	bool operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};