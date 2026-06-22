#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Component.h"

/**
 * @brief Component that handles the position, rotation, and scale of an entity.
 */
class TransformComponent final : public Component
{

public:

	/**
	 * @brief Constructor with an optional name.
	 * 
	 * @param componentName The name of the component.
	 */
	explicit TransformComponent(const std::string& componentName = "TransformComponent") : Component(componentName) {}


	// ----------------------------------------------
	// MODEL MATRIX RELATIVE TRANSFORMATIONS
	// ----------------------------------------------

	/**
	 * @brief Translate the entity relative to its current position.
	 *
	 * @param translation The translation to apply.
	 */
	void Translate(const glm::vec3& translation);

	/**
	 * @brief Rotate the entity relative to its current rotation.
	 *
	 * @param eulerAngles The rotation to apply in radians.
	 */
	void Rotate(const glm::vec3& eulerAngles);

	/**
	 * @brief Scale the entity relative to its current scale.
	 *
	 * @param scaleFactors The scale factors to apply.
	 */
	void Scale(const glm::vec3& scaleFactors);


	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Set the position of the entity.
	 * 
	 * @param newPosition The new position.
	 */
	void SetPosition(const glm::vec3& newPosition);

	/**
	 * @brief Get the position of the entity.
	 * 
	 * @return The position.
	 */
	const glm::vec3& GetPosition() const { return m_position; }

	/**
	 * @brief Set the rotation of the entity using Euler angles.
	 * 
	 * @param newRotation The new rotation in radians.
	 */
	void SetRotation(const glm::vec3& newRotation);

	/**
	 * @brief Get the rotation of the entity as Euler angles.
	 * 
	 * @return The rotation in radians.
	 */
	const glm::vec3& GetRotation() const { return m_rotation; }

	/**
	 * @brief Set the scale of the entity.
	 * 
	 * @param newScale The new scale.
	 */
	void SetScale(const glm::vec3& newScale);

	/**
	 * @brief Set the uniform scale of the entity.
	 *
	 * @param uniformScale The new uniform scale.
	 */
	void SetUniformScale(float uniformScale);

	/**
	 * @brief Get the scale of the entity.
	 * 
	 * @return The scale.
	 */
	const glm::vec3& GetScale() const {	return m_scale; }

	/**
	 * @brief Get the model matrix for this transform, updating it if necessary
	 *
	 * @return The model matrix.
	 */
	const glm::mat4& GetModelMatrix();

private:

	/**
	 * @brief Update the model matrix based on position, rotation, and scale.
	 */
	void UpdateModelMatrix();

private:

	glm::vec3 m_position = { 0.0f, 0.0f, 0.0f };
	glm::vec3 m_rotation = { 0.0f, 0.0f, 0.0f };        // Euler angles in radians
	glm::vec3 m_scale = { 1.0f, 1.0f, 1.0f };

	glm::mat4 m_modelMatrix = glm::mat4(1.0f);

	bool      m_modelMatrixDirty = true;
};

