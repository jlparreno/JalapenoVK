#pragma once

#include <GLFW/glfw3.h>

// Forward declarations
class CameraControllerComponent;

/**
 * @brief Polls GLFW input and drives the active camera controller.
 *
 * Owns every GLFW-specific input concern (key/button polling, Maya-style Alt+mouse rotation mode, cursor capture) 
 * so that GLFW stays out of the scene/ECS layer. Translates raw input into calls on the sibling
 * CameraControllerComponent (SetMoveInput / SetRotateInput)
 *
 * The window is non-owning and must outlive the manager. 
 * The camera controller is also non-owning and resolved late via SetCameraController
 * once the scene has created the camera entity.
 */
class InputManager final
{

public:

	/**
	 * @brief Constructor.
	 *
	 * @param window The GLFW window to poll input from.
	 */
	explicit InputManager(GLFWwindow* window);

	// ----------------------------------------------
	// INPUT PROCESSING
	// ----------------------------------------------

	/**
	 * @brief Polls keyboard and mouse-button state and forwards it to the camera controller.
	 *
	 * Expected to be called once per frame. Builds the WASD movement axes and
	 * drives the Alt+left-mouse-button rotation mode (cursor capture on enter,
	 * release on exit).
	 */
	void ProcessInput();

	/**
	 * @brief Handles a GLFW cursor-position event.
	 *
	 * Computes the delta against the last known cursor position and streams it
	 * into the camera controller. Only has an effect while rotation mode
	 * (Alt + left mouse button) is active.
	 *
	 * @param xpos Current cursor X position, in screen coordinates.
	 * @param ypos Current cursor Y position, in screen coordinates.
	 */
	void OnCursorMoved(double xpos, double ypos);

	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Set the camera controller that receives input from this manager.
	 *
	 * @param controller The camera controller to drive.
	 */
	void SetCameraController(CameraControllerComponent* controller) { m_cameraController = controller; };

private:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	GLFWwindow*					m_window{ nullptr };            // Not owned by this class.

	CameraControllerComponent*	m_cameraController{ nullptr };  // Non-owning; resolved after the renderer creates the camera entity.

	// Input state
	double                      m_lastCursorX{ 0.0 };           // Cursor X position at the last sample, in screen coordinates.
	double                      m_lastCursorY{ 0.0 };           // Cursor Y position at the last sample, in screen coordinates.
	bool                        m_rotating{ false };            // True while Alt + mouse button is held (Maya-style camera rotation).
};

