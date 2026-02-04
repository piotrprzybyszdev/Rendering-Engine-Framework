#include "Core/Core.h"

#include "Application.h"

namespace ref::vulkan
{

Application *Application::GetInstance()
{
    return s_Instance;
}

Application::Application(ApplicationSpec &&spec)
    : m_Window(std::move(spec.ApplicationWindow)), m_Surface(spec.Surface), m_DebugMessenger(spec.DebugMessenger),
      m_DispatchLoader(spec.DispatchLoader), m_PhysicalDevice(spec.PhysicalDevice),
      m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.Queues.at(MainQueueName))
{
    for (const auto &[name, queue] : spec.Queues)
        SetDebugName(queue.Handle, name);

    if (s_Instance != nullptr)
        throw std::runtime_error("Only one instance of the application must exist");

    s_Instance = this;
}

Application::~Application()
{
    s_Instance = nullptr;
}

void Application::Run()
{
    vk::Extent2D extent;
    {
        auto [width, height] = m_Window->GetSize();
        extent = vk::Extent2D(width, height);
    }

    std::array<uint32_t, 1> presentQueueFamilies = { m_MainQueue.FamilyIndex };
    SwapchainSpec swapchainSpec = {
        .Surface = m_Surface,
        .UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        .PresentMode = vk::PresentModeKHR::eFifo,
        .SurfaceFormat = vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear),
        .Extent = extent,
        .ImageCount = 2,
        .PresentQueueFamilies = presentQueueFamilies,
    };

    m_Swapchain = std::make_unique<Swapchain>(m_LogicalDevice, swapchainSpec);

    vk::CommandPoolCreateInfo createInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_MainQueue.FamilyIndex
    );
    auto pool = m_LogicalDevice.createCommandPool(createInfo);

    vk::CommandBufferAllocateInfo allocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1);
    auto buffer = m_LogicalDevice.allocateCommandBuffers(allocateInfo)[0];

    while (!m_Window->ShouldClose())
    {
        Window::PollEvents();

        auto [width, height] = m_Window->GetSize();
        if (extent.width != width || extent.height != height)
        {
            extent = vk::Extent2D(width, height);
            swapchainSpec.Extent = extent;
            m_Swapchain = std::make_unique<Swapchain>(m_LogicalDevice, swapchainSpec, m_Swapchain.get());
            continue;
        }

        m_Swapchain->AcquireImage(m_LogicalDevice);

        buffer.reset();
        buffer.begin(vk::CommandBufferBeginInfo());

        const auto image = m_Swapchain->GetCurrentFrame().Image;

        const vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        {
            vk::ImageMemoryBarrier2 barrier(
                vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
                vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored, image, range
            );

            vk::DependencyInfo dependency;
            dependency.setImageMemoryBarriers(barrier);

            buffer.pipelineBarrier2(dependency);
        }

        buffer.clearColorImage(
            image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(0.0f, 0.0f, 1.0f, 0.0f),
            { range }
        );

        {
            vk::ImageMemoryBarrier2 barrier(
                vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone,
                vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, vk::QueueFamilyIgnored,
                vk::QueueFamilyIgnored, image, range
            );

            vk::DependencyInfo dependency;
            dependency.setImageMemoryBarriers(barrier);

            buffer.pipelineBarrier2(dependency);
        }

        buffer.end();

        const auto &sync = m_Swapchain->GetCurrentSynchronizationObjects();

        vk::CommandBufferSubmitInfo cmdInfo(buffer);
        vk::SemaphoreSubmitInfo waitInfo(
            sync.ImageAcquiredSemaphore, 0, vk::PipelineStageFlagBits2::eAllCommands
        );
        vk::SemaphoreSubmitInfo signalInfo(
            sync.RenderCompleteSemaphore, 0, vk::PipelineStageFlagBits2::eAllCommands
        );
        vk::SubmitInfo2 submitInfo(vk::SubmitFlags(), waitInfo, cmdInfo, signalInfo);

        m_MainQueue.Handle.submit2({ submitInfo }, sync.InFlightFence);
        m_Swapchain->Present(m_MainQueue.Handle);
        m_MainQueue.Handle.waitIdle();
    }
}

}