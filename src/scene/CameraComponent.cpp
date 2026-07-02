#include "scene/CameraComponent.h"

#include "scene/Entity.h"
#include "scene/TransformComponent.h"

void CameraComponent::Init()
{
	UpdateViewMatrix();
	UpdateProjectionMatrix();
}

void CameraComponent::SetFieldOfView(float fov)
{
	m_fov = fov;
	m_projectionMatrixDirty = true;
}

void CameraComponent::SetAspectRatio(float ratio)
{
	m_aspect = ratio;
	m_projectionMatrixDirty = true;
}

void CameraComponent::SetClipPlanes(float near, float far)
{
	m_near = near;
	m_far = far;
	m_projectionMatrixDirty = true;
}

glm::vec3 CameraComponent::GetPosition() const
{
	auto transform = GetOwner()->GetComponent<TransformComponent>();
	return transform ? transform->GetPosition() : glm::vec3(0.0f, 0.0f, 0.0f);
}

const glm::mat4& CameraComponent::GetViewMatrix()
{
    if (m_viewMatrixDirty)
    {
        UpdateViewMatrix();
    }

    return m_viewMatrix;
}

const glm::mat4& CameraComponent::GetProjectionMatrix()
{
    if (m_projectionMatrixDirty)
    {
        UpdateProjectionMatrix();
    }

    return m_projectionMatrix;
}

void CameraComponent::UpdateViewMatrix()
{
    // Get transform component
    auto transformComponent = GetOwner()->GetComponent<TransformComponent>();

	if (transformComponent)
	{
		// Build camera world transform (T * R) from the camera entity's transform
		// and compute the view matrix as its inverse. This ensures consistency
		// with rasterization and avoids relying on an external target vector.
		const glm::vec3 position = transformComponent->GetPosition();
		const glm::vec3 euler = transformComponent->GetRotation();        // radians

		const glm::quat qx = glm::angleAxis(euler.x, glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::quat qy = glm::angleAxis(euler.y, glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::quat qz = glm::angleAxis(euler.z, glm::vec3(0.0f, 0.0f, 1.0f));
		const glm::quat q = qz * qy * qx;        // match TransformComponent's ZYX composition

		const glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
		const glm::mat4 R = glm::mat4_cast(q);
		const glm::mat4 worldNoScale = T * R;

		m_viewMatrix = glm::inverse(worldNoScale);
	}
	else
	{
		// Fallback: default camera at origin looking towards +Z with Y up
		// Note: keep consistent with right-handed convention used elsewhere
		const glm::vec3 position(0.0f);
		const glm::vec3 forward(0.0f, 0.0f, 1.0f);
		const glm::vec3 upVec(0.0f, 1.0f, 0.0f);
		m_viewMatrix = glm::lookAt(position, position + forward, upVec);
	}

	m_viewMatrixDirty = false;
}

void CameraComponent::UpdateProjectionMatrix()
{
    m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
    m_projectionMatrixDirty = false;
}