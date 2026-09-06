#include "scene/Scene.h"

#include "scene/CameraComponent.h"
#include "scene/CameraControllerComponent.h"
#include "scene/Entity.h"
#include "scene/TransformComponent.h"

#include <algorithm>

Scene::Scene()
{
	// Minimal Camera Setup
	Entity* camera = AddEntity("Camera");
	SetActiveCamera(camera);

	// Tranform component
	// Order matters: CameraComponent::Init and CameraControllerComponent::Init both read the Transform, 
	// so the Transform (with its initial pose) must be added first.
	auto* cameraTransform = m_activeCamera->AddComponent<TransformComponent>();
	cameraTransform->SetPosition({ 3.0f, 2.0f, 3.0f });

	// Orient from (2,0,2) towards the origin.
	// Convention: local forward = -Z, so yaw rotates around world Y and yaw=0 looks at -Z.
	constexpr float initialYawDeg = 45.0f;
	constexpr float initialPitchDeg = -25.0f;
	cameraTransform->SetRotation({ glm::radians(initialPitchDeg), glm::radians(initialYawDeg), 0.0f });

	// Camera component
	m_activeCamera->AddComponent<CameraComponent>();
	m_activeCamera->AddComponent<CameraControllerComponent>();
}

void Scene::Update(std::chrono::duration<float> deltaTime)
{
	for (auto& entity : m_entities)
	{
		entity->Update(deltaTime);
	}
}

Entity* Scene::AddEntity(const std::string& name)
{
	auto new_entity = std::make_unique<Entity>(name);
	Entity* entity_ptr = new_entity.get();

	if (entity_ptr)
	{
		m_entities.push_back(std::move(new_entity));
		return entity_ptr;
	}
	
	return nullptr;
}

Entity* Scene::GetEntity(const std::string& name) const
{
	auto it = std::find_if(m_entities.begin(), m_entities.end(), [&name](const std::unique_ptr<Entity>& entity) { return entity->GetName() == name; });

	if (it != m_entities.end())
	{
		return it->get();
	}
	else
	{
		return nullptr;
	}
}
