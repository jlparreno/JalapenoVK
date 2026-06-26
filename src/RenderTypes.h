#pragma once

// By default, GLM uses OpenGL depth range from -1.0 to 1.0.
// Here we configure it to use the Vulkan range from 0.0 to 1.0
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulkanIncludes.h"

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return { {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,	.offset = offsetof(Vertex, position)},
				  {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat,	.offset = offsetof(Vertex, color)},
				  {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,	.offset = offsetof(Vertex, texCoord)}} };
	}

	// Needed for comparisons when inserting vertices in a map
	bool operator==(const Vertex& other) const
	{
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};