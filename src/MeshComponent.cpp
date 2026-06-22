#include "MeshComponent.h"

#include "Entity.h"
#include "TransformComponent.h"

void MeshComponent::Render()
{
    // if (!mesh || !material) return;

    // Get transform component
    auto transform = GetOwner()->GetComponent<TransformComponent>();

    if (!transform) 
        return;

    // Render mesh with material and transform
    //material->Bind();
    //material->SetUniform("modelMatrix", transform->GetTransformMatrix());
    //mesh->Render();
}
