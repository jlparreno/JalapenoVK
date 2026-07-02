#pragma once

#include "scene/Component.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

/**
 * @brief Component that represents and manages an entity's local transform.
 *
 * The TransformComponent stores position, rotation (in Euler angles), and scale,
 * and is responsible for computing the resulting model matrix used for rendering.
 *
 * It acts as the spatial foundation for an entity within the scene graph or ECS
 * system, providing both relative transformation operations (Translate/Rotate/Scale)
 * and absolute setters for direct control of the transform state.
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

	// ----------------------------------------------
	// INTERNAL HELPERS
	// ----------------------------------------------

	/**
	 * @brief Update the model matrix based on position, rotation, and scale.
	 */
	void UpdateModelMatrix();


	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	glm::vec3 m_position = { 0.0f, 0.0f, 0.0f };		// Local position of the entity in its parent space.
	glm::vec3 m_rotation = { 0.0f, 0.0f, 0.0f };        // Local rotation expressed as Euler angles in radians.
	glm::vec3 m_scale = { 1.0f, 1.0f, 1.0f };			// Local scale of the entity.

	glm::mat4 m_modelMatrix{ glm::mat4(1.0f) };			// Cached model matrix combining position, rotation, and scale.

	bool      m_modelMatrixDirty{ true };				// Dirty flag indicating whether the model matrix needs to be recomputed.
};

