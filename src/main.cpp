#include "core/VulkanContext.h"
#include "io/InputManager.h"
#include "io/Window.h"
#include "render/Renderer.h"
#include "resources/Mesh.h"
#include "resources/ResourceManager.h"
#include "resources/Shader.h"
#include "resources/Texture.h"
#include "scene/CameraComponent.h"
#include "scene/CameraControllerComponent.h"
#include "scene/Entity.h"
#include "scene/MeshComponent.h"
#include "scene/Scene.h"
#include "scene/TransformComponent.h"
#include "scene/LightComponent.h"
#include "materials/PBRMaterial.h"

#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

const uint32_t WIDTH  = 1920;
const uint32_t HEIGHT = 1080;
const char*	   TITLE  = "JalapenoVK";

class JalapenoVK
{
  public:

	void Run()
	{
		InitWindow();
		InitInput();
		InitVulkan();
		InitResources();
		InitScene();
		InitRenderer();
		MainLoop();
		Cleanup();
	}

  private:

	std::unique_ptr<Window>				m_window;			// Created first; every subsystem below needs its GLFWwindow handle.

	std::unique_ptr<VulkanContext>		m_context;			// MUST be the first member declared / last destroyed: every other Vulkan-holding member depends on its Device.
	std::unique_ptr<Renderer>			m_renderer;			// Holds the swapchain + per-pass GPU state; must be destroyed before m_context (see Cleanup()).
	std::unique_ptr<Scene>				m_scene;			// Entities + active camera; content beyond the default camera is built externally in InitScene().
	std::unique_ptr<InputManager>		m_inputManager;		// Drives the active camera controller; wired up in InitInput() / InitRenderer().
	std::unique_ptr<ResourceManager>	m_resourceManager;	// Owns loaded textures/meshes/shaders; must be unloaded before m_context is destroyed (see Cleanup()).

	vk::raii::DescriptorSetLayout		m_pbrMaterialLayout{ nullptr };

	TransformComponent*					m_modelTransform{ nullptr }; // Non-owning, cached for the demo Y-spin. Not owned by this class.

	void InitWindow()
	{
		m_window = std::make_unique<Window>(TITLE, WIDTH, HEIGHT);

		glfwSetWindowUserPointer(m_window->GetHandle(), this);
		glfwSetFramebufferSizeCallback(m_window->GetHandle(), FramebufferResizeCallback);
	}

	void InitInput()
	{
		m_inputManager = std::make_unique<InputManager>(m_window->GetHandle());

		glfwSetCursorPosCallback(m_window->GetHandle(), CursorPosCallback);
	}

	void InitVulkan()
	{
		m_context = std::make_unique<VulkanContext>(m_window->GetHandle());

		// Create Material layouts, mandatory to load resources correctly
		m_pbrMaterialLayout = PBRMaterial::CreateSetLayout(*m_context);
	}

	void InitResources()
	{
		m_resourceManager = std::make_unique<ResourceManager>();

		// Load placeholder textures to support correct model loading when no textures available.
		m_resourceManager->LoadResource<Texture>(*m_context, "placeholder_white", glm::vec4(1.0f), Texture::sRGB);
		m_resourceManager->LoadResource<Texture>(*m_context, "placeholder_normal", glm::vec4(0.5f, 0.5f, 1.0f, 1.0f), Texture::Linear);

		// Load the resources the current scene needs. Ownership stays in the ResourceManager;
		// the returned handles are used only to validate that the load succeeded.
		auto mesh	 = m_resourceManager->LoadResource<Mesh>(*m_context, "DamagedHelmet/glTF/DamagedHelmet", *m_resourceManager, *m_pbrMaterialLayout);
		auto shader  = m_resourceManager->LoadResource<Shader>(*m_context, "pbr.slang", vk::ShaderStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment));

		if (!mesh || !shader)
		{
			throw std::runtime_error("Failed to load required resources");
		}
	}

	void InitScene()
	{
		// Create the scene with the default camera
		m_scene = std::make_unique<Scene>();

		// Directional Light
		Entity* light = m_scene->AddEntity("Light");

		auto* lightTransform = light->AddComponent<TransformComponent>();
		lightTransform->SetRotation({ glm::radians(-50.0f), glm::radians(30.0f), 0.0f });

		light->AddComponent<LightComponent>();

		m_scene->SetActiveLight(light);

		// Models...
		Entity* model = m_scene->AddEntity("Model");

		auto* modelTransform = model->AddComponent<TransformComponent>();
		modelTransform->SetPosition({ 0.0f, 0.0f, 0.0f });
		modelTransform->SetRotation({ glm::radians(90.0f), 0.0f, 0.0f });
		modelTransform->SetScale({ 1.0f, 1.0f, 1.0f });
		m_modelTransform = modelTransform;

		model->AddComponent<MeshComponent>()->SetMesh(m_resourceManager->GetResource<Mesh>("DamagedHelmet/glTF/DamagedHelmet"));
	}

	void InitRenderer()
	{
		m_renderer = std::make_unique<Renderer>(*m_context, *m_resourceManager, *m_scene, m_window->GetHandle(), *m_pbrMaterialLayout);

		// Cache a non-owning handle to the camera controller so the input layer can drive it.
		if (Entity* camera = m_scene->GetActiveCamera())
		{
			if (auto* cameraComponent = camera->GetComponent<CameraComponent>())
			{
				const vk::Extent2D& extent = m_renderer->GetSwapchain().GetExtent();
				cameraComponent->SetAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));
			}

			m_inputManager->SetCameraController(camera->GetComponent<CameraControllerComponent>());
		}
	}

	void MainLoop()
	{
		auto lastFrameTime = std::chrono::steady_clock::now();

		while (!m_window->ShouldClose())
		{
			m_window->PollEvents();

			const auto now = std::chrono::steady_clock::now();
			const auto deltaTime = std::chrono::duration<float>(now - lastFrameTime);
			lastFrameTime = now;

			m_window->DisplayFrameTimes(deltaTime.count());

			m_inputManager->ProcessInput();

			// Rotate model 45 degrees per second, just for demo
			if (m_modelTransform)
			{
				glm::vec3 rotation = m_modelTransform->GetRotation();
				rotation.y += glm::radians(45.0f) * deltaTime.count();
				m_modelTransform->SetRotation(rotation);
			}

			m_scene->Update(deltaTime);
			m_renderer->Render();
		}

		// Ensure the GPU is idle before we start tearing GPU-side resources down.
		m_renderer->WaitIdle();
	}

	void Cleanup()
	{
		// Release Vulkan-side objects before shutting down the platform layer.
		// Order matters: the renderer holds the swapchain + per-pass state, resources
		// hold GPU buffers/images, and all of them require the VulkanContext (device)
		// to be alive during their destructors.
		m_resourceManager->UnloadAllResources();
		m_renderer.reset();
		m_pbrMaterialLayout = nullptr;

		m_context.reset();
	}

	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
	{
		auto* app = static_cast<JalapenoVK*>(glfwGetWindowUserPointer(window));
		if (app && app->m_inputManager)
		{
			app->m_inputManager->OnCursorMoved(xpos, ypos);
		}
	}

	static void FramebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] int width, [[maybe_unused]] int height)
	{
		auto* app = static_cast<JalapenoVK*>(glfwGetWindowUserPointer(window));
		if (app && app->m_renderer)
		{
			app->m_renderer->OnFramebufferResized();
		}
	}
};

int main()
{
	try
	{
		JalapenoVK app;
		app.Run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
