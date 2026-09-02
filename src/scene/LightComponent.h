#pragma once

#include "scene/Component.h"

#include <glm/vec3.hpp>

/**
 * @brief Directional Light component.
 *
 * The light itself carries only color/intensity, its direction 
 * is derived from the owning entity's TransformComponent rotation
 */
class LightComponent final : public Component
{

public:

	/**
	 * @brief Constructor with an optional name.
	 *
	 * @param componentName The name of the component.
	 */
	explicit LightComponent(const std::string& componentName = "LightComponent") : Component(componentName) {}

	// ----------------------------------------------
	// COMPONENT OVERRIDES
	// ----------------------------------------------

	/**
	 * @brief Initialize the light component.
	 *
	 * No-op yet.
	 */
	void Init() override {};

	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Get the light's direction in world space.
	 *
	 * Derived from the TransformComponent's rotation, rotating the local forward (0,0,-1).
	 * Falls back to straight down if the owning entity has no TransformComponent.
	 *
	 * @return Normalized direction the light points towards.
	 */
	glm::vec3 GetDirection() const;

	/**
	 * @brief Get the light color.
	 *
	 * @return The light color (RGB, not intensity-scaled).
	 */
	glm::vec3 GetColor() const { return m_color; };

	/**
	 * @brief Get the light intensity.
	 *
	 * @return The light intensity, multiplies the color when shading.
	 */
	float GetIntensity() const { return m_intensity; };

	/**
	 * @brief Set the light color.
	 *
	 * @param color The light color (RGB, not intensity-scaled).
	 */
	void SetColor(const glm::vec3& color) { m_color = color; };

	/**
	 * @brief Set the light intensity.
	 *
	 * @param intensity The light intensity, multiplies the color when shading.
	 */
	void SetIntensity(float intensity) { m_intensity = intensity; };

private:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	glm::vec3	m_color		{ 1.0f };	// Light color (RGB), white by default.
	float		m_intensity	{ 3.0f };	// Intensity multiplier applied to m_color when shading.
};
