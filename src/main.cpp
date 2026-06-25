#include "VulkanContext.h"

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
