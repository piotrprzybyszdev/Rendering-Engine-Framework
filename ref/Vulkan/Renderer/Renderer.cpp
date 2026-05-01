#include "Core/Core.h"

#include "Renderer.h"

namespace ref::vulkan
{

Renderer::Renderer(RendererSpec spec)
    : m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.MainQueue), m_ResourceAllocator(spec.ResourceAllocator),
      m_CurrentFrameGraph(spec.FrameGraph)
{
    vk::CommandPoolCreateInfo createInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_MainQueue.FamilyIndex
    );
    m_CommandPool = m_LogicalDevice.createCommandPool(createInfo);
    m_Fence = m_LogicalDevice.createFence(vk::FenceCreateInfo());

    vk::CommandBufferAllocateInfo allocateInfo(m_CommandPool, vk::CommandBufferLevel::ePrimary, 1);
    m_CommandBuffer = m_LogicalDevice.allocateCommandBuffers(allocateInfo)[0];

    m_StagingBuffer = m_ResourceAllocator->AddBufferResource("REF Staging Buffer");
    m_ResourceAllocator->AllocateResource(
        m_StagingBuffer,
        vk::BufferCreateInfo()
            .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
            .setSize(spec.StagingBufferSize),
        false
    );
}

Renderer::~Renderer()
{
    m_MainQueue.Handle.waitIdle();
}

void Renderer::UploadWithStaging(
    BufferResourceId buffer, std::span<const std::byte> data, vk::DeviceSize offset
)
{
    m_ResourceAllocator->UploadToBuffer(m_StagingBuffer, data.data(), data.size_bytes());

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_CommandBuffer.reset();
    m_CommandBuffer.begin(beginInfo);

    const BufferResource &srcBuffer = m_ResourceAllocator->GetBufferResource(m_StagingBuffer);
    const BufferResource &dstBuffer = m_ResourceAllocator->GetBufferResource(buffer);

    m_CommandBuffer.copyBuffer(
        srcBuffer.Handle, dstBuffer.Handle, vk::BufferCopy(0, offset, data.size_bytes())
    );

    m_CommandBuffer.end();

    vk::CommandBufferSubmitInfo cmdInfo(m_CommandBuffer);
    m_MainQueue.Handle.submit2(vk::SubmitInfo2().setCommandBufferInfos(cmdInfo), m_Fence);

    [[maybe_unused]] vk::Result result =
        m_LogicalDevice.waitForFences({ m_Fence }, vk::True, std::numeric_limits<uint64_t>::max());
    assert(result == vk::Result::eSuccess);
    m_LogicalDevice.resetFences({ m_Fence });
}

void Renderer::UploadWithStaging(
    ImageResourceId image, std::span<const std::byte> data, vk::ImageLayout layout,
    vk::ImageSubresourceLayers layers
)
{
    m_ResourceAllocator->UploadToBuffer(m_StagingBuffer, data.data(), data.size_bytes());

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_CommandBuffer.reset();
    m_CommandBuffer.begin(beginInfo);

    BufferResource srcBuffer = m_ResourceAllocator->GetBufferResource(m_StagingBuffer);
    ImageResource dstImage = m_ResourceAllocator->GetImageResource(image);

    vk::ImageMemoryBarrier2 srcImageBarrier(
        vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
    );
    vk::ImageSubresourceRange range(layers.aspectMask, layers.mipLevel, 1, layers.baseArrayLayer, layers.layerCount);
    srcImageBarrier.setSubresourceRange(range);
    srcImageBarrier.setImage(dstImage.Handle);

    m_CommandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(srcImageBarrier));

    vk::BufferImageCopy imageCopy(0, 0, 0, layers, vk::Offset3D(0, 0, 0), dstImage.Extent);
    m_CommandBuffer.copyBufferToImage(
        srcBuffer.Handle, dstImage.Handle, vk::ImageLayout::eTransferDstOptimal, imageCopy
    );

    vk::ImageMemoryBarrier2 dstImageBarrier(
        vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eTransferDstOptimal, layout
    );
    dstImageBarrier.setSubresourceRange(range);
    dstImageBarrier.setImage(dstImage.Handle);

    m_CommandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(dstImageBarrier));

    m_CommandBuffer.end();

    vk::CommandBufferSubmitInfo cmdInfo(m_CommandBuffer);
    m_MainQueue.Handle.submit2(vk::SubmitInfo2().setCommandBufferInfos(cmdInfo), m_Fence);

    [[maybe_unused]] vk::Result result =
        m_LogicalDevice.waitForFences({ m_Fence }, vk::True, std::numeric_limits<uint64_t>::max());
    assert(result == vk::Result::eSuccess);
    m_LogicalDevice.resetFences({ m_Fence });
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

void Renderer::OnUpdate([[maybe_unused]] float timeStep)
{
}

void Renderer::OnRender()
{
    FrameGraph::FrameInfo frameInfo = {
        .FrameInFlightIndex = m_Swapchain->GetCurrentFrameInFlightIndex(),
        .FrameInFlightCount = m_Swapchain->GetInFlightCount(),
        .Image = m_Swapchain->GetCurrentFrame().Image,
        .ImageView = m_Swapchain->GetCurrentFrame().ImageView,
        .Extent = m_Swapchain->GetExtent(),
    };

    const auto &resources = m_RenderingResources.at(m_Swapchain->GetCurrentFrameInFlightIndex());
    m_CurrentFrameGraph->OnRender(resources.CommandBuffer, frameInfo);

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
