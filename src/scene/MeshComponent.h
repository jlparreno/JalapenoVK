#pragma once

#include "resources/Mesh.h"
#include "scene/Component.h"

/**
 * @brief Component that handles the mesh data for rendering.
 */
class MeshComponent : public Component 
{

public:

    /**
     * @brief Constructor with an optional name.
     *
     * @param componentName The name of the component.
     */
    explicit MeshComponent(const std::string& componentName = "MeshComponent") : Component(componentName) {}

    // ----------------------------------------------
    // COMPONENT OVERRIDES
    // ----------------------------------------------

    void Render() override;

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    void SetMesh(Mesh* m) { m_mesh = m; }
    //void SetMaterial(Material* mat) { material = mat; }

    Mesh* GetMesh() const { return m_mesh; }
    //Material* GetMaterial() const { return material; }

private:

    Mesh*       m_mesh{ nullptr };
    //Material* material = nullptr;
};

