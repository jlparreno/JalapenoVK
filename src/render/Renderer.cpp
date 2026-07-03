#include "render/Renderer.h"

#include "core/VulkanContext.h"
#include "render/RenderTypes.h"
#include "render/passes/GeometryPass.h"
#include "resources/Mesh.h"
#include "resources/Shader.h"
#include "resources/Texture.h"
#include "scene/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer(VulkanContext& context, ResourceManager& resourceManager, GLFWwindow* window) :
    m_context(context),
    m_window(window),
    m_resourceManager(resourceManager),
    m_swapchain(context, window)
{
    SetupEntities();
    CreateCommandBuffers();
    SetupRenderPasses();
}

void Renderer::UpdateEntities(std::chrono::duration<float> deltaTime)
{
    for (Entity& entity : m_entities)
    {
        entity.Update(deltaTime);
    }
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

    // Feed the geometry pass with the transient state it needs for this frame
    if (m_geometryPass)
    {
        // View / projection from the active camera
        auto* camera = m_activeCamera ? m_activeCamera->GetComponent<CameraComponent>() : nullptr;
        glm::mat4 view = camera ? camera->GetViewMatrix()       : glm::mat4(1.0f);
        glm::mat4 proj = camera ? camera->GetProjectionMatrix() : glm::mat4(1.0f);
        proj[1][1] *= -1.0f; // GLM assumes OpenGL NDC (Y up); Vulkan is Y down.

        // Use model transform if it exists
        auto* transform = m_entities[0].GetComponent<TransformComponent>();
        glm::mat4 model = transform ? transform->GetModelMatrix() : glm::mat4(1.0f);

        GeometryPass::FrameData frameData
        {
            .swapchainImageView = m_swapchain.GetImageView(imageIndex),
            .extent             = m_swapchain.GetExtent(),
            .mesh               = m_resourceManager.GetResource<Mesh>("viking_room"),
            .model              = model,
            .view               = view,
            .proj               = proj
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
    if (m_activeCamera)
    {
        if (auto* camera = m_activeCamera->GetComponent<CameraComponent>())
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
        .extent      = m_swapchain.GetExtent(),
        .shader      = m_resourceManager.GetResource<Shader>("shader.slang"),
        .texture     = m_resourceManager.GetResource<Texture>("viking_room")
    };

    m_geometryPass = m_renderPassManager.AddRenderPass<GeometryPass>("GeometryPass", m_context, info);
}

void Renderer::SetupEntities()
{
    // Model entity: the viking_room mesh transform.
    auto* modelTransform = m_entities[0].AddComponent<TransformComponent>();
    modelTransform->SetPosition({ 0.0f, 0.0f, 0.0f });
    modelTransform->SetRotation({ 0.0f, -45.0f, 0.0f });
    modelTransform->SetScale   ({ 1.0f, 1.0f, 1.0f });

    // CAMERA ENTITY
    
    // Tranform component
    // Order matters: CameraComponent::Init and CameraControllerComponent::Init both read the Transform, 
    // so the Transform (with its initial pose) must be added first.
    auto* cameraTransform = m_entities[1].AddComponent<TransformComponent>();
    cameraTransform->SetPosition({ 2.0f, 2.0f, 2.0f });

    // Orient from (2,2,2) towards the origin so the initial view matches the previous hardcoded camera.
    // Convention: local forward = -Z, so yaw rotates around world Y and yaw=0 looks at -Z.
    constexpr float initialYawDeg   = 45.0f;
    constexpr float initialPitchDeg = -35.0f;
    cameraTransform->SetRotation({ glm::radians(initialPitchDeg), glm::radians(initialYawDeg), 0.0f });

    // Camera component
    // Match aspect to the current swapchain extent so the first frame is correct.
    auto* cameraComponent = m_entities[1].AddComponent<CameraComponent>();
    cameraComponent->SetAspectRatio(static_cast<float>(m_swapchain.GetExtent().width) / static_cast<float>(m_swapchain.GetExtent().height));

    // Camera Controlles component
    m_entities[1].AddComponent<CameraControllerComponent>();

    m_activeCamera = &m_entities[1];
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

    const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo;
    submitInfo.setWaitSemaphoreCount(1)
              .setWaitSemaphores(imageAvailable)
              .setWaitDstStageMask(waitStage)
              .setCommandBufferCount(1)
              .setCommandBuffers(*commandBuffer)
              .setSignalSemaphoreCount(1)
              .setSignalSemaphores(renderFinished);

    m_context.GetGraphicsQueue().submit(submitInfo, inFlightFence);
}
