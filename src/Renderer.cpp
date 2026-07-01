#include "Renderer.h"
#include "GeometryPass.h"
#include "VulkanContext.h"
#include "Shader.h"
#include "Texture.h"
#include "Mesh.h"

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

void Renderer::Render(const std::vector<Entity*>& entities)
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
        const vk::Extent2D extent = m_swapchain.GetExtent();
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

        // Temporary camera — will move to a scene CameraComponent in a future step.
        // World is Y-up to match the glTF asset convention we load.
        glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
        proj[1][1] *= -1.0f; // GLM assumes OpenGL NDC (Y up); Vulkan is Y down.

        // Simple time-based rotation around the world Y axis. Lives here (not inside the pass)
        // so GeometryPass stays agnostic of scene animation logic.
        auto* transform = m_entities[0].GetComponent<TransformComponent>();
        if (transform)
        {
            const float time = static_cast<float>(glfwGetTime());
            transform->SetRotation({ 0.0f, time * glm::radians(90.0f), 0.0f });
        }
        glm::mat4 model = transform ? transform->GetModelMatrix() : glm::mat4(1.0f);

        GeometryPass::FrameData frameData
        {
            .swapchainImageView = m_swapchain.GetImageView(imageIndex),
            .extent             = extent,
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

    if (presentResult == vk::Result::eErrorOutOfDateKHR
     || presentResult == vk::Result::eSuboptimalKHR
     || m_framebufferResized)
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

    if (m_geometryPass)
    {
        m_geometryPass->OnResize(m_swapchain.GetExtent());
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
    auto* transformComponent = m_entities[0].AddComponent<TransformComponent>();
    transformComponent->SetPosition({ 0.0f, 0.0f, 0.0f });
    transformComponent->SetRotation({ 0.0f, 0.0f, 0.0f });
    transformComponent->SetScale   ({ 1.0f, 1.0f, 1.0f });
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
