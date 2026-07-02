#include "core/VulkanContext.h"
#include "render/Renderer.h"
#include "resources/Mesh.h"
#include "resources/ResourceManager.h"
#include "resources/Shader.h"
#include "resources/Texture.h"
#include "scene/Entity.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

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

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_window = glfwCreateWindow(WIDTH, HEIGHT, "JalapenoVK", nullptr, nullptr);

		// Route framebuffer resize events back into the renderer via a static trampoline.
		glfwSetWindowUserPointer(m_window, this);
		glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
	{
		auto* app = static_cast<JalapenoVK*>(glfwGetWindowUserPointer(window));
		if (app && app->m_renderer)
		{
			app->m_renderer->OnFramebufferResized();
		}
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
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();
			m_renderer->Render(std::vector<Entity*>());
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
