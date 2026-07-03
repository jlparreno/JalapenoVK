#pragma once

#include "scene/Component.h"

#include <glm/glm.hpp>

/**
 * @brief Free-fly camera controller component.
 *
 * Translates externally provided input (keyboard axes and mouse deltas) into
 * yaw / pitch orientation and into positional integration on the owner's
 * TransformComponent. While attached, the controller is the sole writer of
 * the transform's rotation, and marks the sibling CameraComponent's view
 * matrix dirty whenever the transform changes.
 *
 * The owning entity is expected to also carry a TransformComponent and a
 * CameraComponent; without them the controller is a no-op.
 */
class CameraControllerComponent : public Component
{

public:

    /**
     * @brief Constructor with an optional name.
     *
     * @param componentName The name of the component.
     */
    explicit CameraControllerComponent(const std::string& componentName = "CameraController") : Component(componentName) {}


    // ----------------------------------------------
    // COMPONENT OVERRIDES
    // ----------------------------------------------

    /**
     * @brief Initialize the camera controller component.
     *
     * Derives the initial yaw / pitch from the owner's TransformComponent so the
     * controller starts in sync with the pose configured on the transform.
     */
    void Init() override;

    /**
     * @brief Integrate movement input and sync rotation into the owner's transform.
     *
     * @param deltaTime The time elapsed since the last frame, in seconds.
     */
    void Update(std::chrono::duration<float> deltaTime) override;


    // ----------------------------------------------
    // INPUT INTERFACE
    // ----------------------------------------------

    /**
     * @brief Sets the current per-axis movement input, replacing the previous value.
     *
     * Expected to be called every frame by the caller after polling keys.
     * Not accumulated - the caller owns the state, the controller integrates it.
     *
     * @param localAxes  {right, up, forward} components, each in [-1..1].
     */
    void SetMoveInput(const glm::vec3& localAxes);

    /**
     * @brief Applies mouse-look deltas immediately to yaw / pitch.
     *
     * Scaled by m_mouseSensitivity; pitch is clamped to [m_minPitch, m_maxPitch].
     *
     * @param mouseDeltaX Horizontal cursor delta in pixels since the previous sample.
     * @param mouseDeltaY Vertical cursor delta in pixels since the previous sample (screen-Y-down).
     */
    void SetRotateInput(float mouseDeltaX, float mouseDeltaY);


    // ----------------------------------------------
    // GETTERS & SETTERS
    // ----------------------------------------------

    /**
     * @brief Set the yaw angle around world Y.
     *
     * @param yaw The new yaw in radians.
     */
    void  SetYaw(float yaw) { m_yaw = yaw; }

    /**
     * @brief Set the pitch angle above the horizontal plane.
     *
     * @param pitch The new pitch in radians.
     */
    void  SetPitch(float pitch) { m_pitch = pitch; }

    /**
     * @brief Set the minimum and maximum pitch values used to clamp mouse-look.
     *
     * @param minVal Minimum pitch in radians.
     * @param maxVal Maximum pitch in radians.
     */
    void  SetPitchClamp(float minVal, float maxVal);

    /**
     * @brief Set the linear movement speed applied to keyboard input.
     *
     * @param s Speed in world units per second at nominal input magnitude.
     */
    void  SetMoveSpeed(float s) { m_moveSpeed = s; }

    /**
     * @brief Set the mouse-look sensitivity.
     *
     * @param s Radians of rotation per pixel of mouse movement.
     */
    void  SetMouseSensitivity(float s) { m_mouseSensitivity = s; }

    /**
     * @brief Get the current yaw angle.
     *
     * @return The yaw in radians.
     */
    float GetYaw()   const { return m_yaw; }

    /**
     * @brief Get the current pitch angle.
     *
     * @return The pitch in radians.
     */
    float GetPitch() const { return m_pitch; }

private:

    // ----------------------------------------------
    // MEMBERS
    // ----------------------------------------------

    // Orientation state
    float       m_yaw{ 0.0f };                  // Radians around world Y.
    float       m_pitch{ 0.0f };                // Radians above horizontal plane.

    // Per-frame input
    glm::vec3   m_moveInput{ 0.0f };            // {right, up, forward} in [-1..1].

    // Speed properties
    float       m_moveSpeed{ 5.0f };            // World units per second at nominal input.
    float       m_mouseSensitivity{ 0.002f };   // Radians per pixel of mouse movement.

    // Pitch clamps to prevent flipping through the poles. In radians.
    float       m_minPitch{ -1.553f };          // ~-89 degrees.
    float       m_maxPitch{ 1.553f };           // ~+89 degrees.
};

