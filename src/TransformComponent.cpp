#include "TransformComponent.h"

void TransformComponent::Translate(const glm::vec3& translation)
{
	m_position += translation;
	m_modelMatrixDirty = true;
}

void TransformComponent::Rotate(const glm::vec3& eulerAngles)
{
	m_rotation += eulerAngles;
	m_modelMatrixDirty = true;
}

void TransformComponent::Scale(const glm::vec3& scaleFactors)
{
	m_scale *= scaleFactors;
	m_modelMatrixDirty = true;
}

void TransformComponent::SetPosition(const glm::vec3& newPosition)
{
	m_position = newPosition;
	m_modelMatrixDirty = true;
}

void TransformComponent::SetRotation(const glm::vec3& newRotation)
{
	m_rotation = newRotation;
	m_modelMatrixDirty = true;
}

void TransformComponent::SetScale(const glm::vec3& newScale)
{
	m_scale = newScale;
	m_modelMatrixDirty = true;
}

void TransformComponent::SetUniformScale(float uniformScale)
{
	m_scale = glm::vec3(uniformScale);
	m_modelMatrixDirty = true;
}

const glm::mat4& TransformComponent::GetModelMatrix()
{
	if (m_modelMatrixDirty)
	{
		UpdateModelMatrix();
	}

	return m_modelMatrix;
}

void TransformComponent::UpdateModelMatrix()
{
	// Translation
	glm::mat4 T = glm::translate(glm::mat4(1.0f), m_position);

	// Compose rotation with quaternions for stability and to avoid rad/deg ambiguity
	glm::quat qx = glm::angleAxis(m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat qy = glm::angleAxis(m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat qz = glm::angleAxis(m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

	glm::quat q = qz * qy * qx;        // ZYX order is conventional for Euler composition
	glm::mat4 R = glm::mat4_cast(q);

	// Scale
	glm::mat4 S = glm::scale(glm::mat4(1.0f), m_scale);

	// Update values
	m_modelMatrix = T * R * S;
	m_modelMatrixDirty = false;
}

