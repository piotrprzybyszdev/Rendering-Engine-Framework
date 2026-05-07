#include "Core/Core.h"

#include "Vulkan/Application.h"

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

    Application::GetInstance()->SetDebugName(m_CommandPool, "REF Main Command Buffer Pool");
    Application::GetInstance()->SetDebugName(m_CommandBuffer, "REF Main Command Buffer");

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

    for (const auto &resources : m_RenderingResources)
        m_LogicalDevice.destroyCommandPool(resources.CommandPool);

    m_LogicalDevice.destroyFence(m_Fence);
    m_LogicalDevice.destroyCommandPool(m_CommandPool);
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

void Renderer::ClearImage(
    ImageResourceId image, vk::ImageLayout layout, vk::ClearColorValue value, vk::ImageSubresourceRange range
)
{
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_CommandBuffer.reset();
    m_CommandBuffer.begin(beginInfo);

    ImageResource dstImage = m_ResourceAllocator->GetImageResource(image);

    vk::ImageMemoryBarrier2 srcImageBarrier(
        vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eClear,
        vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
    );
    srcImageBarrier.setSubresourceRange(range);
    srcImageBarrier.setImage(dstImage.Handle);

    m_CommandBuffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(srcImageBarrier));

    m_CommandBuffer.clearColorImage(dstImage.Handle, vk::ImageLayout::eTransferDstOptimal, value, range);

    vk::ImageMemoryBarrier2 dstImageBarrier(
        vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
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

void Renderer::FillBuffer(BufferResourceId buffer, vk::DeviceSize offset, vk::DeviceSize size, uint32_t value)
{
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_CommandBuffer.reset();
    m_CommandBuffer.begin(beginInfo);

    BufferResource dstBuffer = m_ResourceAllocator->GetBufferResource(buffer);

    vk::BufferMemoryBarrier2 srcBufferBarrier(
        vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite
    );

    srcBufferBarrier.setOffset(offset);
    srcBufferBarrier.setSize(size);
    srcBufferBarrier.setBuffer(dstBuffer.Handle);

    m_CommandBuffer.pipelineBarrier2(vk::DependencyInfo().setBufferMemoryBarriers(srcBufferBarrier));

    m_CommandBuffer.fillBuffer(dstBuffer.Handle, offset, size, value);

    vk::BufferMemoryBarrier2 dstBufferBarrier(
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone
    );

    dstBufferBarrier.setOffset(offset);
    dstBufferBarrier.setSize(size);
    dstBufferBarrier.setBuffer(dstBuffer.Handle);

    m_CommandBuffer.pipelineBarrier2(vk::DependencyInfo().setBufferMemoryBarriers(dstBufferBarrier));

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

    while (m_Swapchain->GetInFlightCount() > m_RenderingResources.size())
    {
        const uint32_t i = static_cast<uint32_t>(m_RenderingResources.size());

        RenderingResources resources = {};

        vk::CommandPoolCreateInfo createInfo(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_MainQueue.FamilyIndex
        );
        resources.CommandPool = m_LogicalDevice.createCommandPool(createInfo);

        vk::CommandBufferAllocateInfo allocateInfo(
            resources.CommandPool, vk::CommandBufferLevel::ePrimary, 1
        );
        resources.CommandBuffer = m_LogicalDevice.allocateCommandBuffers(allocateInfo)[0];

        Application::GetInstance()->SetDebugName(
            resources.CommandPool, std::format("REF Frame Command Buffer Pool {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            resources.CommandBuffer, std::format("REF Frame Command Buffer {}", i).c_str()
        );

        m_RenderingResources.push_back(resources);
    }

    m_CurrentFrameGraph->UpdateImage(FrameGraph::g_SwapchainImageResourceName);
    m_CurrentFrameGraph->UpdateImageView(FrameGraph::g_SwapchainImageViewResourceName);
}

void Renderer::BeginFrame()
{
    FrameGraph::FrameInfo frameInfo = {
        .FrameInFlightIndex = m_Swapchain->GetCurrentFrameInFlightIndex(),
        .FrameInFlightCount = m_Swapchain->GetInFlightCount(),
        .Image = m_Swapchain->GetCurrentFrame().Image,
        .ImageView = m_Swapchain->GetCurrentFrame().ImageView,
        .Extent = m_Swapchain->GetExtent(),
    };
    m_CurrentFrameGraph->SetFrame(frameInfo);
}

void Renderer::EndFrame()
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
