#include "scene/CameraControllerComponent.h"

#include "scene/Entity.h"
#include "scene/TransformComponent.h"
#include "scene/CameraComponent.h"

#include <algorithm>

void CameraControllerComponent::Init()
{
	auto transformComponent = GetOwner()->GetComponent<TransformComponent>();

	if (!transformComponent)
		return;

	// Derive initial yaw / pitch from the transform so the caller only has to
	// configure the pose in a single place (the Transform). Roll is ignored.
	const glm::vec3 euler = transformComponent->GetRotation();
	m_pitch = euler.x;
	m_yaw   = euler.y;
}

void CameraControllerComponent::Update(std::chrono::duration<float> deltaTime)
{
	// Get transform and camera components
	auto transformComponent = GetOwner()->GetComponent<TransformComponent>();
	auto cameraComponent = GetOwner()->GetComponent<CameraComponent>();

	if (!transformComponent || !cameraComponent)
		return;

	const float deltaSeconds = deltaTime.count();
	
	// All the engine is based in world Y-up
	static constexpr glm::vec3 up{ 0.0f, 1.0f, 0.0f };

	// Calculate front vector.
	// Convention: local forward = -Z (OpenGL / Vulkan standard), so yaw=0/pitch=0 yields (0,0,-1).
	// Matches the view matrix built by CameraComponent from the Transform's Euler ZYX rotation.
	glm::vec3 front;
	front.x = -std::cos(m_pitch) * std::sin(m_yaw);
	front.y =  std::sin(m_pitch);
	front.z = -std::cos(m_pitch) * std::cos(m_yaw);

	const glm::vec3 right = glm::normalize(glm::cross(front, up));


	const glm::vec3 moveDelta = right * m_moveInput.x + up * m_moveInput.y + front * m_moveInput.z;

	transformComponent->SetPosition(transformComponent->GetPosition() + moveDelta * m_moveSpeed * deltaSeconds);
	transformComponent->SetRotation(glm::vec3(m_pitch, m_yaw, 0.0f));

	// Force view matrix recalculation
	cameraComponent->SetViewDirty();
}

void CameraControllerComponent::SetMoveInput(const glm::vec3& localAxes)
{
	m_moveInput = localAxes;
}

void CameraControllerComponent::SetRotateInput(float mouseDeltaX, float mouseDeltaY)
{
	m_yaw -= mouseDeltaX * m_mouseSensitivity;

	// Clamp to prevent flipping through the poles
	m_pitch = std::clamp(m_pitch - mouseDeltaY * m_mouseSensitivity, m_minPitch, m_maxPitch);
}

void CameraControllerComponent::SetPitchClamp(float minVal, float maxVal)
{
	m_minPitch = minVal;
	m_maxPitch = maxVal;
}

