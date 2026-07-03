#include "core/VulkanContext.h"
#include "render/Renderer.h"
#include "resources/Mesh.h"
#include "resources/ResourceManager.h"
#include "resources/Shader.h"
#include "resources/Texture.h"
#include "scene/CameraControllerComponent.h"
#include "scene/Entity.h"

#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

const uint32_t WIDTH  = 1920;
const uint32_t HEIGHT = 1080;

class JalapenoVK
{
  public:

	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:

	GLFWwindow*                     m_window{ nullptr };

	std::unique_ptr<VulkanContext>  m_context;			// MUST be the first member declared / last destroyed: every other Vulkan-holding member depends on its Device.
	std::unique_ptr<Renderer>       m_renderer;
	ResourceManager                 m_resourceManager;

	// Input state
	CameraControllerComponent*      m_cameraController{ nullptr };  // Non-owning; resolved after the renderer creates the camera entity.
	double                          m_lastCursorX{ 0.0 };
	double                          m_lastCursorY{ 0.0 };
	bool                            m_rotating{ false };            // True while Alt + mouse button is held (Maya-style camera rotation).

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_window = glfwCreateWindow(WIDTH, HEIGHT, "JalapenoVK", nullptr, nullptr);

		// Route window events back into this instance via static trampolines.
		glfwSetWindowUserPointer(m_window, this);
		glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
		glfwSetCursorPosCallback(m_window, cursorPosCallback);
	}

	void initVulkan()
	{
		m_context = std::make_unique<VulkanContext>(m_window);

		// Load the resources the current scene needs. Ownership stays in the ResourceManager;
		// the returned handles are used only to validate that the load succeeded.
		auto texture = m_resourceManager.LoadResource<Texture>(*m_context, "viking_room");
		auto mesh    = m_resourceManager.LoadResource<Mesh>(*m_context, "viking_room");
		auto shader  = m_resourceManager.LoadResource<Shader>(*m_context, "shader.slang", vk::ShaderStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment));

		if (!texture || !mesh || !shader)
		{
			throw std::runtime_error("Failed to load required resources");
		}

		m_renderer = std::make_unique<Renderer>(*m_context, m_resourceManager, m_window);

		// Cache a non-owning handle to the camera controller so the input layer can drive it.
		if (Entity* camera = m_renderer->GetActiveCamera())
		{
			m_cameraController = camera->GetComponent<CameraControllerComponent>();
		}
	}

	void mainLoop()
	{
		auto lastFrameTime = std::chrono::steady_clock::now();

		while (!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();

			const auto now = std::chrono::steady_clock::now();
			const auto deltaTime = std::chrono::duration<float>(now - lastFrameTime);
			lastFrameTime = now;

			ProcessInput();

			m_renderer->UpdateEntities(deltaTime);
			m_renderer->Render();
		}

		// Ensure the GPU is idle before we start tearing GPU-side resources down.
		m_renderer->WaitIdle();
	}

	void cleanup()
	{
		// Release Vulkan-side objects before shutting down the platform layer.
		// Order matters: the renderer holds the swapchain + per-pass state, resources
		// hold GPU buffers/images, and all of them require the VulkanContext (device)
		// to be alive during their destructors.
		m_resourceManager.UnloadAllResources();
		m_renderer.reset();
		m_context.reset();

		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	void ProcessInput()
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

	static void framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
	{
		auto* app = static_cast<JalapenoVK*>(glfwGetWindowUserPointer(window));
		if (app && app->m_renderer)
		{
			app->m_renderer->OnFramebufferResized();
		}
	}

	static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
	{
		auto* app = static_cast<JalapenoVK*>(glfwGetWindowUserPointer(window));
		if (!app || !app->m_rotating || !app->m_cameraController)
		{
			return;
		}

		const double dx = xpos - app->m_lastCursorX;
		const double dy = ypos - app->m_lastCursorY;
		app->m_lastCursorX = xpos;
		app->m_lastCursorY = ypos;

		app->m_cameraController->SetRotateInput(static_cast<float>(dx), static_cast<float>(dy));
	}
};

int main()
{
	try
	{
		JalapenoVK app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
