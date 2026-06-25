#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Component.h"

/**
 * @brief Perspective camera component.
 *
 * Manages the view and projection matrices for a camera attached to an entity.
 * Both matrices are computed lazily: they are only recalculated when their
 * parameters change, indicated by the corresponding dirty flag.
 *
 * The view matrix is derived from the owning entity's transform.
 * The projection matrix is perspective, parameterised by FOV, aspect ratio,
 * and near / far clip planes.
 */
class CameraComponent : public Component
{

public:

    /**
     * @brief Constructor with an optional name.
     *
     * @param componentName The name of the component.
     */
    explicit CameraComponent(const std::string& componentName = "CameraComponent") : Component(componentName) {}


    // ----------------------------------------------
    // COMPONENT OVERRIDES
    // ----------------------------------------------

    /**
     * @brief Initialize the camera component
     */
    void Init() override;


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Set the field of view for perspective projection.
     *
     * @param fov The field of view in degrees.
     */
    void SetFieldOfView(float fov);

    /**
     * @brief Set the aspect ratio for perspective projection.
     *
     * @param ratio The aspect ratio (width / height).
     */
    void SetAspectRatio(float ratio);

    /**
     * @brief Set the near and far planes.
     *
     * @param near The near plane distance.
     * @param far The far plane distance.
     */
    void SetClipPlanes(float near, float far);

    /**
     * @brief Get the field of view.
     *
     * @return The field of view in degrees.
     */
    float GetFieldOfView() const { return m_fov; }

    /**
     * @brief Get the aspect ratio.
     *
     * @return The aspect ratio.
     */
    float GetAspectRatio() const { return m_aspect; }

    /**
     * @brief Get the near plane distance.
     *
     * @return The near plane distance.
     */
    float GetNearPlane() const { return m_near; }

    /**
     * @brief Get the far plane distance.
     *
     * @return The far plane distance.
     */
    float GetFarPlane() const { return m_far; }

    /**
     * @brief Get the camera position.
     *
     * @return The camera position.
     */
    glm::vec3 GetPosition() const;

    /**
     * @brief Get the view matrix, updating it if necessary
     *
     * @return The view matrix.
     */
    const glm::mat4& GetViewMatrix();

    /**
     * @brief Get the projection matrix, updating it if necessary
     *
     * @return The projection matrix.
     */
    const glm::mat4& GetProjectionMatrix();

private:

    // ----------------------------------------------
    // INTERNAL HELPERS
    // ----------------------------------------------

    /**
     * @brief Update the view matrix based on the camera position and target.
     */
    void UpdateViewMatrix();

    /**
     * @brief Update the projection matrix based on the projection type and parameters.
     */
    void UpdateProjectionMatrix();


    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    float       m_fov{ 45.0f };                         // Vertical field of view in degrees.
    float       m_aspect{ 16.0f / 9.0f };               // Aspect ratio (width / height).
    float       m_near{ 0.1f };                         // Near clip plane distance.
    float       m_far{ 1000.0f };                       // Far clip plane distance.

    glm::mat4   m_viewMatrix{ glm::mat4(1.0f) };        // Cached view matrix.
    glm::mat4   m_projectionMatrix{ glm::mat4(1.0f) };  // Cached projection matrix.

    bool        m_viewMatrixDirty{ true };              // True when the view matrix needs recomputing.
    bool        m_projectionMatrixDirty{ true };        // True when the projection matrix needs recomputing.
};

