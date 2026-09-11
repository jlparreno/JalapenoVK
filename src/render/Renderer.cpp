#include "render/Renderer.h"

#include "core/VulkanContext.h"
#include "render/RenderTypes.h"
#include "render/passes/GeometryPass.h"
#include "render/passes/SkyboxPass.h"
#include "resources/Mesh.h"
#include "resources/ResourceManager.h"
#include "resources/Texture.h"
#include "scene/TransformComponent.h"
#include "scene/MeshComponent.h"
#include "scene/LightComponent.h"

#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer(VulkanContext& context, ResourceManager& resourceManager, const CreateInfo& info) :
    m_context(context),
    m_scene(*info.scene),
    m_swapchain(context, info.window),
    m_renderTarget(context, m_swapchain.GetFormat(), m_swapchain.GetExtent())
{
    CreateCommandBuffers();
    SetupRenderPasses(resourceManager, info);
}

void Renderer::Render()
{
    uint32_t imageIndex = 0;
    vk::Result acquireResult = m_swapchain.AcquireNextImage(imageIndex);

    if (acquireResult == vk::Result::eErrorOutOfDateKHR)
    {
        HandleSwapchainRecreation();
        return;
    }

    if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Construct the list of renderables to prepare for render
    std::vector<Renderable> renderables;
    for (const auto& entity : m_scene.GetEntities())
    {
        if (!entity->IsActive())
        {
            continue;
        }

        auto* meshComponent = entity->GetComponent<MeshComponent>();
        auto* transform     = entity->GetComponent<TransformComponent>();

        if (meshComponent && meshComponent->GetMesh() && transform)
        {
            renderables.push_back({ meshComponent->GetMesh(), transform->GetModelMatrix() });
        }
    }

    // View / projection from the active camera
    auto* camera = m_scene.GetActiveCamera() ? m_scene.GetActiveCamera()->GetComponent<CameraComponent>() : nullptr;
    glm::mat4 view = camera ? camera->GetViewMatrix() : glm::mat4(1.0f);
    glm::mat4 proj = camera ? camera->GetProjectionMatrix() : glm::mat4(1.0f);
    proj[1][1] *= -1.0f; // GLM assumes OpenGL NDC (Y up); Vulkan is Y down.

    // Feed the skyboxPass pass with the transient state it needs for this frame
    if (m_skyboxPass)
    {
        glm::mat4 invViewProj = glm::inverse(proj * glm::mat4(glm::mat3(view))); // Remove translation from view to keep the skybox fixed in position

        SkyboxPass::FrameData frameData
        {
            .extent = m_swapchain.GetExtent(),
            .invViewProj = invViewProj
        };
        m_skyboxPass->SetFrameData(frameData);
    }

    // Feed the geometry pass with the transient state it needs for this frame
    if (m_geometryPass)
    {
        glm::vec3 cameraPos = camera ? camera->GetPosition() : glm::vec3(0.0f);

        // Active light
        auto* light = m_scene.GetActiveLight() ? m_scene.GetActiveLight()->GetComponent<LightComponent>() : nullptr;
        glm::vec3 lightDirection = light ? light->GetDirection() : glm::vec3(0.0f, -1.0f, 0.0f);
        glm::vec3 lightColor = light ? light->GetColor()         : glm::vec3(1.0f);
        float lightIntensity = light ? light->GetIntensity()     : 0.0f;

        GeometryPass::FrameData frameData
        {
            .swapchainImageView = m_swapchain.GetImageView(imageIndex),
            .extent             = m_swapchain.GetExtent(),
            .view               = view,
            .proj               = proj,
            
            .lightDirection     = lightDirection,
            .lightColor         = lightColor,
            .lightIntensity     = lightIntensity,
            .cameraPosition     = cameraPos,

            .renderables        = renderables
        };
        m_geometryPass->SetFrameData(frameData);
    }

    // Select the command buffer for this frame slot
    vk::raii::CommandBuffer& cmd = m_commandBuffers[m_swapchain.GetCurrentFrame()];

    cmd.reset();
    cmd.begin({});

    RecordCommandBuffer(cmd, imageIndex);

    cmd.end();

    SubmitCommandBuffer(cmd, imageIndex);

    vk::Result presentResult = m_swapchain.Present(imageIndex);

    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR || m_framebufferResized)
    {
        m_framebufferResized = false;
        HandleSwapchainRecreation();
    }
    else if (presentResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to present swapchain image");
    }

    m_swapchain.AdvanceFrame();
}

void Renderer::WaitIdle() const
{
    m_context.GetDevice().waitIdle();
}

void Renderer::HandleSwapchainRecreation()
{
    // Swapchain::Recreate() handles minimization (blocks until non-zero size) and calls
    // waitIdle() internally, so by the time we reach the pass update the GPU is idle.
    m_swapchain.Recreate();

    const vk::Extent2D extent = m_swapchain.GetExtent();
    
    // Resize render target
    m_renderTarget.OnResize(extent);

    // Update the active camera's aspect ratio so the projection matches the new surface.
    if (m_scene.GetActiveCamera())
    {
        if (auto* camera = m_scene.GetActiveCamera()->GetComponent<CameraComponent>())
        {
            camera->SetAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        }
    }
}

void Renderer::SetupRenderPasses(ResourceManager& resourceManager, const CreateInfo& info)
{
    // Add SkyboxPass
    SkyboxPass::CreateInfo skyboxInfo
    {
        .renderTarget = &m_renderTarget,
        .environmentMap = info.environmentMap
    };

    m_skyboxPass = m_renderPassManager.AddRenderPass<SkyboxPass>("SkyboxPass", m_context, resourceManager, skyboxInfo);

    // Add GeometryPass
    GeometryPass::CreateInfo geometryInfo
    {
        .renderTarget = &m_renderTarget,
        .materialLayout = info.pbrMaterialLayout,
        .imageBasedLighting = info.imageBasedLighting
    };

    m_geometryPass = m_renderPassManager.AddRenderPass<GeometryPass>("GeometryPass", m_context, resourceManager, geometryInfo);
    m_geometryPass->AddDependency("SkyboxPass");
}

void Renderer::CreateCommandBuffers()
{
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandPool(*m_context.GetCommandPool())
             .setLevel(vk::CommandBufferLevel::ePrimary)
             .setCommandBufferCount(k_maxFramesInFlight);

    m_commandBuffers = m_context.GetDevice().allocateCommandBuffers(allocInfo);
}

void Renderer::RecordCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex)
{
    const vk::Image swapchainImage = m_swapchain.GetImage(imageIndex);

    // Swapchain: Undefined -> ColorAttachmentOptimal (target for the MSAA resolve)
    m_context.TransitionImageLayout(commandBuffer,
    {
        .image          = swapchainImage,
        .oldLayout      = vk::ImageLayout::eUndefined,
        .srcStageMask   = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask  = vk::AccessFlagBits2::eNone,
        .newLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .dstStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite
    });

    // Passes manage their own attachment transitions (color MSAA + depth).
    m_renderPassManager.Execute(commandBuffer, { imageIndex, m_swapchain.GetCurrentFrame() });

    // Swapchain: ColorAttachmentOptimal -> PresentSrcKHR
    m_context.TransitionImageLayout(commandBuffer,
    {
        .image          = swapchainImage,
        .oldLayout      = vk::ImageLayout::eColorAttachmentOptimal,
        .srcStageMask   = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask  = vk::AccessFlagBits2::eColorAttachmentWrite,
        .newLayout      = vk::ImageLayout::ePresentSrcKHR,
        .dstStageMask   = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask  = vk::AccessFlagBits2::eNone
    });
}

void Renderer::SubmitCommandBuffer(vk::raii::CommandBuffer& commandBuffer, uint32_t imageIndex)
{
    const vk::Semaphore imageAvailable = m_swapchain.GetImageAvailableSemaphore();
    const vk::Semaphore renderFinished = m_swapchain.GetRenderFinishedSemaphore(imageIndex);
    const vk::Fence     inFlightFence  = m_swapchain.GetInFlightFence();

    vk::SemaphoreSubmitInfo waitSemaphoreInfo;
    waitSemaphoreInfo.setSemaphore(imageAvailable)
                     .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::SemaphoreSubmitInfo signalSemaphoreInfo;
    signalSemaphoreInfo.setSemaphore(renderFinished)
                       .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::CommandBufferSubmitInfo commandBufferInfo;
    commandBufferInfo.setCommandBuffer(*commandBuffer);

    vk::SubmitInfo2 submitInfo;
    submitInfo.setWaitSemaphoreInfos(waitSemaphoreInfo)
              .setCommandBufferInfos(commandBufferInfo)
              .setSignalSemaphoreInfos(signalSemaphoreInfo);

    m_context.GetGraphicsQueue().submit2(submitInfo, inFlightFence);
}
