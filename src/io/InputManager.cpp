#include "io/InputManager.h"

#include "scene/CameraControllerComponent.h"

#include <glm/vec3.hpp>

InputManager::InputManager(GLFWwindow* window) : 
	m_window(window)
{
}

void InputManager::ProcessInput()
{
	if (!m_cameraController)
	{
		return;
	}

	// Keyboard: WASD builds a {right, up, forward} axes vector; Y stays unmapped for now.
	glm::vec3 axes{ 0.0f };
	if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) axes.x += 1.0f;
	if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) axes.x -= 1.0f;
	if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) axes.z += 1.0f;
	if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) axes.z -= 1.0f;
	m_cameraController->SetMoveInput(axes);

	// Rotation: Maya-style — Alt + left mouse button captures the cursor and streams mouse deltas into the controller.
	const bool altPressed = glfwGetKey(m_window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;
	const bool mousePressed = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	const bool shouldRotate = altPressed && mousePressed;

	if (shouldRotate && !m_rotating)
	{
		// Enter rotation mode: hide the cursor and snapshot the current position so the first
		// delta computed by the callback is (0, 0) instead of jumping from a stale value.
		glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwGetCursorPos(m_window, &m_lastCursorX, &m_lastCursorY);
		m_rotating = true;
	}
	else if (!shouldRotate && m_rotating)
	{
		glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		m_rotating = false;
	}
}

void InputManager::OnCursorMoved(double xpos, double ypos)
{
	if (!m_rotating || !m_cameraController)
	{
		return;
	}

	const double dx = xpos - m_lastCursorX;
	const double dy = ypos - m_lastCursorY;
	m_lastCursorX = xpos;
	m_lastCursorY = ypos;

	m_cameraController->SetRotateInput(static_cast<float>(dx), static_cast<float>(dy));
}
