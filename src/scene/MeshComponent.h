#pragma once

#include "resources/Mesh.h"
#include "scene/Component.h"

/**
 * @brief Component that binds a Mesh resource to an entity for rendering.
 *
 * MeshComponent holds a non-owning pointer to a Mesh resource resolved from
 * the ResourceManager and forwards it to the geometry pass at render time.
 * It does not manage the mesh's lifetime — the ResourceManager remains the
 * owner and the mesh must outlive every component referencing it.
 *
 * Once a Material system exists, this component will also carry the material
 * used to render the mesh; for now only geometry is exposed.
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

    /**
     * @brief Render the mesh. Called during the entity's render phase.
     */
    void Render() override;

    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Binds a Mesh resource to this component.
     *
     * @param m  Mesh resource to render. Not owned by this component.
     */
    void SetMesh(Mesh* m) { m_mesh = m; }

    //void SetMaterial(Material* mat) { material = mat; }

    /**
     * @brief Returns the currently bound Mesh, or nullptr if none has been set.
     */
    Mesh* GetMesh() const { return m_mesh; }
    
    //Material* GetMaterial() const { return material; }

private:

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    Mesh*       m_mesh{ nullptr };      // Mesh resource to render. Not owned by this component.
    //Material* material = nullptr;
};

