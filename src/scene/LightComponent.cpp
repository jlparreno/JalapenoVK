#include "scene/LightComponent.h"

#include "scene/Entity.h"
#include "scene/TransformComponent.h"

#include <glm/gtc/quaternion.hpp>


glm::vec3 LightComponent::GetDirection() const
{
	auto* transform = GetOwner()->GetComponent<TransformComponent>();
	if (!transform)
	{
		return glm::vec3(0.0f, -1.0f, 0.0f); //straight down light
	}

	const glm::vec3 rotation = transform->GetRotation();
	const glm::quat q = glm::angleAxis(rotation.z, glm::vec3(0, 0, 1))
					  * glm::angleAxis(rotation.y, glm::vec3(0, 1, 0))
					  * glm::angleAxis(rotation.x, glm::vec3(1, 0, 0));

	return glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f)); // Convention. No rotation is the light looking forward (-Z)
}
