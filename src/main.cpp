#include "core/VulkanContext.h"
#include "io/InputManager.h"
#include "io/Window.h"
#include "render/Renderer.h"
#include "render/EnvironmentMap.h"
#include "render/ImageBasedLighting.h"
#include "resources/Mesh.h"
#include "resources/ResourceManager.h"
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
		InitEnvironment();
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
	std::unique_ptr<EnvironmentMap>		m_environmentMap;	// Environment cubemap generated at startup.
	std::unique_ptr<ImageBasedLighting>	m_imageBasedLighting; // IBL resources derived from the environment cubemap at startup.

	vk::raii::DescriptorSetLayout		m_pbrMaterialLayout{ nullptr };

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

		// Load the content the current scene needs.
		auto mesh		= m_resourceManager->LoadResource<Mesh>	  (*m_context, "DamagedHelmet/glTF/DamagedHelmet", *m_resourceManager, *m_pbrMaterialLayout);
		auto hdrTex		= m_resourceManager->LoadResource<Texture>(*m_context, "venice_sunset_4k", Texture::ColorSpace::Linear);

		if (!mesh || !hdrTex)
		{
			throw std::runtime_error("Failed to load required resources");
		}
	}

	void InitEnvironment()
	{
		// Loaded and validated in InitResources(), so it is known to be there.
		const Texture* equirect = m_resourceManager->GetResource<Texture>("venice_sunset_4k");

		m_environmentMap	 = std::make_unique<EnvironmentMap>(*m_context, *m_resourceManager, *equirect);
		m_imageBasedLighting = std::make_unique<ImageBasedLighting>(*m_context, *m_resourceManager, *m_environmentMap);

		// The equirectangular source has done its job: its pixels now live in the cubemap.
		// At 4096x2048 RGBA32F it is holding 128 MB of device memory, so release it here.
		m_resourceManager->UnloadResource<Texture>("venice_sunset_4k");
	}

	void InitScene()
	{
		// Create the scene with the default camera
		m_scene = std::make_unique<Scene>();

		// Directional Light, now commented because we are applying lighting using IBL
		/*Entity* light = m_scene->AddEntity("Light");

		auto* lightTransform = light->AddComponent<TransformComponent>();
		lightTransform->SetRotation({ glm::radians(-50.0f), glm::radians(30.0f), 0.0f });

		light->AddComponent<LightComponent>();

		m_scene->SetActiveLight(light);*/

		// Models...
		Entity* model = m_scene->AddEntity("Model");

		auto* modelTransform = model->AddComponent<TransformComponent>();
		modelTransform->SetPosition({ 0.0f, 0.0f, 0.0f });
		modelTransform->SetRotation({ glm::radians(90.0f), 0.0f, 0.0f });
		modelTransform->SetScale({ 1.0f, 1.0f, 1.0f });

		model->AddComponent<MeshComponent>()->SetMesh(m_resourceManager->GetResource<Mesh>("DamagedHelmet/glTF/DamagedHelmet"));
	}

	void InitRenderer()
	{
		Renderer::CreateInfo rendererInfo
		{
			.window				= m_window->GetHandle(),
			.scene				= m_scene.get(),
			.pbrMaterialLayout	= *m_pbrMaterialLayout,
			.environmentMap		= m_environmentMap.get(),
			.imageBasedLighting	= m_imageBasedLighting.get()
		};

		m_renderer = std::make_unique<Renderer>(*m_context, *m_resourceManager, rendererInfo);

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
		m_imageBasedLighting.reset();
		m_environmentMap.reset();
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
