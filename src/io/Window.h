#pragma once

#include <GLFW/glfw3.h>
#include <string>

/**
 * @brief Owns the platform window
 *
 * Creates and destroys the GLFWwindow, and owns everything cosmetic that
 * hangs off it: the taskbar/title-bar icon and the FPS/frame-time readout
 * shown in the title.
 *
 * The returned handle (GetHandle) is non-owning and meant to be handed to
 * subsystems that need it.
 */
class Window final
{

public:

	/**
	 * @brief Constructor. Creates the GLFW window, sets its icon, and shows the initial title.
	 *
	 * @param title  The base window title (frame-time stats are appended to this).
	 * @param width  Initial window width, in pixels.
	 * @param height Initial window height, in pixels.
	 */
	Window(const std::string& title, uint32_t width, uint32_t height);

	/**
	 * @brief Destructor. Destroys the GLFW window and terminates GLFW.
	 */
	~Window();

	// ----------------------------------------------
	// WINDOW MANIPULATION
	// ----------------------------------------------

	/**
	 * @brief Loads the window icon from m_icon and applies it to the window.
	 *
	 * Logs to stderr and leaves the default icon in place if loading fails.
	 */
	void SetIcon();

	/**
	 * @brief Sets the window title to the base title plus the given frame-time stats.
	 *
	 * @param fps    Frames per second to display.
	 * @param timeMs Frame time in milliseconds to display.
	 */
	void SetFrameTimesTitle(int fps, float timeMs);

	/**
	 * @brief Pumps pending GLFW window/input events.
	 *
	 * Expected to be called once per frame, before input is polled.
	 */
	void PollEvents();

	/**
	 * @brief Accumulates frame time and refreshes the title's FPS readout once per second.
	 *
	 * @param deltaTime Time elapsed since the previous frame, in seconds.
	 */
	void DisplayFrameTimes(float deltaTime);


	// ----------------------------------------------
	// GETTERS & SETTERS
	// ----------------------------------------------

	/**
	 * @brief Get the raw GLFW window handle.
	 *
	 * @return The GLFW window handle.
	 */
	GLFWwindow* GetHandle() const { return m_window; }

	/**
	 * @brief Check whether the window has received a close request.
	 *
	 * @return True if the window should close.
	 */
	bool ShouldClose() const;

private:

	// ----------------------------------------------
	// MEMBERS
	// ----------------------------------------------

	GLFWwindow*					m_window{ nullptr };					// Owned by this class.

	std::string					m_title{ "" };							// Base title; frame-time stats are appended when displayed.
	uint32_t					m_width{ 1920 };						// Initial window width, in pixels.
	uint32_t					m_height{ 1080 };						// Initial window height, in pixels.

	float						m_titleTimeCounter{ 0.0f };				// Accumulates seconds between window-title refreshes.

	const char*					m_icon{ "assets/jalapeno_logo.png" };	// Path to the window/taskbar icon.
};

