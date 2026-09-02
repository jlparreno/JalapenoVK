#include "io/Window.h"

#include <stb_image.h>
#include <iomanip>
#include <iostream>
#include <sstream>

Window::Window(const std::string& title, uint32_t width, uint32_t height) :
	m_title(title),
	m_width(width),
	m_height(height)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);

	SetIcon();
	SetFrameTimesTitle(0, 0.0f);
}

void Window::SetIcon()
{
	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char* pixels = stbi_load(m_icon, &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels)
	{
		std::cerr << "Failed to load window icon: " << m_icon << std::endl;
		return;
	}

	GLFWimage icon{};
	icon.width = width;
	icon.height = height;
	icon.pixels = pixels;
	glfwSetWindowIcon(m_window, 1, &icon);

	stbi_image_free(pixels);
}

void Window::SetFrameTimesTitle(int fps, float timeMs)
{
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << m_title << "    |    FPS: " << fps << "    |    Time(ms): " << timeMs;

	glfwSetWindowTitle(m_window, oss.str().c_str());
}

void Window::DisplayFrameTimes(float deltaTime)
{
	m_titleTimeCounter += deltaTime;

	if (m_titleTimeCounter >= 1.0f)
	{
		m_titleTimeCounter = 0.0f;

		const int   fps = static_cast<int>(1.0f / deltaTime);
		const float timeMs = deltaTime * 1000.0f;

		SetFrameTimesTitle(fps, timeMs);
	}
}

bool Window::ShouldClose() const
{
	return glfwWindowShouldClose(m_window);
}

void Window::PollEvents()
{
	glfwPollEvents();
}

Window::~Window()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}
