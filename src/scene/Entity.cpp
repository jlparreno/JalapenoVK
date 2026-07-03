#include "scene/Entity.h"

void Entity::Init()
{
	for (auto& component : m_components)
	{
		component->Init();
	}
}

void Entity::Update(std::chrono::duration<float> deltaTime)
{
	if (!m_active)
		return;

	for (auto& component : m_components)
	{
		if (component->IsActive())
		{
			component->Update(deltaTime);
		}
	}
}

void Entity::Render()
{
	if (!m_active)
		return;

	for (auto& component : m_components)
	{
		if (component->IsActive())
		{
			component->Render();
		}
	}
}
