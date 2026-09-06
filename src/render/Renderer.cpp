#include "render/Renderer.h"

#include "core/VulkanContext.h"
#include "render/RenderTypes.h"
#include "render/passes/GeometryPass.h"
#include "resources/Mesh.h"
#include "resources/Shader.h"
#include "resources/Texture.h"
#include "scene/TransformComponent.h"
#include "scene/MeshComponent.h"
#include "scene/LightComponent.h"

#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer(VulkanContext& context, ResourceManager& resourceManager, Scene& scene, GLFWwindow* window, vk::DescriptorSetLayout pbrMaterialLayout) :
    m_context(context),
    m_window(window),
    m_scene(scene),
    m_resourceManager(resourceManager),
    m_swapchain(context, window),
    m_pbrMaterialLayout(pbrMaterialLayout)
{
    CreateCommandBuffers();
    SetupRenderPasses();
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

    // Feed the geometry pass with the transient state it needs for this frame
    if (m_geometryPass)
    {
        // View / projection from the active camera
        auto* camera = m_scene.GetActiveCamera() ? m_scene.GetActiveCamera()->GetComponent<CameraComponent>() : nullptr;
        glm::mat4 view = camera ? camera->GetViewMatrix()       : glm::mat4(1.0f);
        glm::mat4 proj = camera ? camera->GetProjectionMatrix() : glm::mat4(1.0f);
        proj[1][1] *= -1.0f; // GLM assumes OpenGL NDC (Y up); Vulkan is Y down.
        glm::vec3 cameraPos = camera ? camera->GetPosition()    : glm::vec3(0.0f);

        // Active light
        auto* light = m_scene.GetActiveLight() ? m_scene.GetActiveLight()->GetComponent<LightComponent>() : nullptr;
        glm::vec3 lightDirection = light ? light->GetDirection() : glm::vec3(0.0f, -1.0f, 0.0f);
        glm::vec3 lightColor = light ? light->GetColor()         : glm::vec3(1.0f);
        float lightIntensity = light ? light->GetIntensity()     : 1.0f;

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

    if (m_geometryPass)
    {
        m_geometryPass->OnResize(extent);
    }

    // Update the active camera's aspect ratio so the projection matches the new surface.
    if (m_scene.GetActiveCamera())
    {
        if (auto* camera = m_scene.GetActiveCamera()->GetComponent<CameraComponent>())
        {
            camera->SetAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        }
    }
}

void Renderer::SetupRenderPasses()
{
    GeometryPass::CreateInfo info
    {
        .colorFormat = m_swapchain.GetFormat(),
        .extent = m_swapchain.GetExtent(),
        .shader = m_resourceManager.GetResource<Shader>("pbr.slang"),
        .materialLayout = m_pbrMaterialLayout
    };

    m_geometryPass = m_renderPassManager.AddRenderPass<GeometryPass>("GeometryPass", m_context, info);
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
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
               .setSrcAccessMask(vk::AccessFlagBits2::eNone)
               .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setOldLayout(vk::ImageLayout::eUndefined)
               .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
               .setImage(swapchainImage)
               .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        commandBuffer.pipelineBarrier2(depInfo);
    }

    // Passes manage their own attachment transitions (color MSAA + depth).
    m_renderPassManager.Execute(commandBuffer, { imageIndex, m_swapchain.GetCurrentFrame() });

    // Swapchain: ColorAttachmentOptimal -> PresentSrcKHR
    {
        vk::ImageMemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
               .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
               .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
               .setDstAccessMask(vk::AccessFlagBits2::eNone)
               .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
               .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
               .setImage(swapchainImage)
               .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        vk::DependencyInfo depInfo;
        depInfo.setImageMemoryBarriers(barrier);
        commandBuffer.pipelineBarrier2(depInfo);
    }
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
