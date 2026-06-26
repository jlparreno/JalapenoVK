#include "VulkanContext.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Shader.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

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

	GLFWwindow* window{ nullptr };
	std::unique_ptr<VulkanContext> m_context{ nullptr };
	ResourceManager resourceManager;

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "JalapenoVK", nullptr, nullptr);
	}

	void initVulkan()
	{
		m_context = std::make_unique<VulkanContext>(window);

		if (m_context)
		{
			auto texture = resourceManager.LoadResource<Texture>(*m_context, "viking_room");
			auto mesh = resourceManager.LoadResource<Mesh>(*m_context, "viking_room");
			auto shader = resourceManager.LoadResource<Shader>(*m_context, "shader.slang", vk::ShaderStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment));

			if (texture && mesh && shader)
			{
				if (texture->IsLoaded()) std::cout << "Texture is correctly loaded" << std::endl;
				if (mesh->IsLoaded()) std::cout << "Mesh is correctly loaded" << std::endl;
				if (shader->IsLoaded()) std::cout << "Shader is correctly loaded" << std::endl;
			}

			//resourceManager.UnloadResource<Texture>(texture.GetId());
			//resourceManager.UnloadResource<Mesh>(mesh.GetId());
			//resourceManager.UnloadResource<Shader>(shader.GetId());
		}
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		// Destroy context before the window
		m_context.reset();

		glfwDestroyWindow(window);
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
