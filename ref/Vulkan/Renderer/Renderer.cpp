#include "Core/Core.h"

#include "Renderer.h"

namespace ref::vulkan
{

Renderer::Renderer(RendererSpec spec)
    : m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.MainQueue), m_CurrentFrameGraph(spec.FrameGraph)
{
}

void Renderer::OnResize(const Swapchain *swapchain)
{
    m_Swapchain = swapchain;

    m_RenderingResources.clear();
    for (uint32_t i = 0; i < m_Swapchain->GetInFlightCount(); i++)
    {
        RenderingResources resources = {};

        vk::CommandPoolCreateInfo createInfo(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_MainQueue.FamilyIndex
        );
        resources.CommandPool = m_LogicalDevice.createCommandPool(createInfo);

        vk::CommandBufferAllocateInfo allocateInfo(
            resources.CommandPool, vk::CommandBufferLevel::ePrimary, 1
        );
        resources.CommandBuffer = m_LogicalDevice.allocateCommandBuffers(allocateInfo)[0];

        m_RenderingResources.push_back(resources);
    }

    m_CurrentFrameGraph->UpdateImage(FrameGraph::g_SwapchainImageResourceName);
}

void Renderer::OnUpdate(float timeStep)
{
    FrameGraph::FrameInfo frameInfo = {
        .FrameInFlightIndex = m_Swapchain->GetCurrentFrameInFlightIndex(),
        .FrameInFlightCount = m_Swapchain->GetInFlightCount(),
        .Image = m_Swapchain->GetCurrentFrame().Image,
        .ImageView = m_Swapchain->GetCurrentFrame().ImageView,
        .Extent = m_Swapchain->GetExtent(),
    };

    m_CurrentFrameGraph->SetFrame(frameInfo);
    m_CurrentFrameGraph->OnUpdate(timeStep);
}

void Renderer::OnRender()
{
    const auto &resources = m_RenderingResources.at(m_Swapchain->GetCurrentFrameInFlightIndex());
    m_CurrentFrameGraph->OnRender(resources.CommandBuffer);

    const auto &sync = m_Swapchain->GetCurrentSynchronizationObjects();

    vk::CommandBufferSubmitInfo cmdInfo(resources.CommandBuffer);
    vk::SemaphoreSubmitInfo waitInfo(
        sync.ImageAcquiredSemaphore, 0, vk::PipelineStageFlagBits2::eAllCommands
    );
    vk::SemaphoreSubmitInfo signalInfo(
        sync.RenderCompleteSemaphore, 0, vk::PipelineStageFlagBits2::eAllCommands
    );
    vk::SubmitInfo2 submitInfo(vk::SubmitFlags(), waitInfo, cmdInfo, signalInfo);

    m_MainQueue.Handle.submit2({ submitInfo }, sync.InFlightFence);
}

}
