#pragma once

#include <vulkan/vulkan.hpp>

#include <vector>

#include "Vulkan/Swapchain.h"

#include "FrameGraph.h"
#include "ResourceManager.h"

namespace ref::vulkan
{

struct Queue
{
    uint32_t FamilyIndex = vk::QueueFamilyIgnored;
    vk::Queue Handle;
};

struct RendererSpec
{
    vk::Device LogicalDevice;
    Queue MainQueue;
    ref::vulkan::FrameGraph *FrameGraph;
    ref::vulkan::ResourceAllocator *ResourceAllocator;
    vk::DeviceSize StagingBufferSize = 1024 * 1024;
};

class Renderer
{
public:
    Renderer(RendererSpec spec);
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void UploadWithStaging(
        BufferResourceId buffer, std::span<const std::byte> data, vk::DeviceSize offset = 0
    );
    void UploadWithStaging(
        ImageResourceId image, std::span<const std::byte> data, vk::ImageLayout layout,
        vk::ImageSubresourceLayers layers
    );

    void ClearImage(
        ImageResourceId image, vk::ImageLayout layout, vk::ClearColorValue value,
        vk::ImageSubresourceRange range
    );
    void FillBuffer(BufferResourceId buffer, vk::DeviceSize offset, vk::DeviceSize size, uint32_t value);

    void OnResize(const Swapchain *swapchain);

    void BeginFrame();
    void EndFrame();

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;
    ResourceAllocator *m_ResourceAllocator;

    const Swapchain *m_Swapchain = nullptr;

    FrameGraph *m_CurrentFrameGraph;

    struct RenderingResources
    {
        vk::CommandPool CommandPool;
        vk::CommandBuffer CommandBuffer;
    };

    std::vector<RenderingResources> m_RenderingResources;
    
    vk::CommandPool m_CommandPool;
    vk::CommandBuffer m_CommandBuffer;
    vk::Fence m_Fence;

    BufferResourceId m_StagingBuffer;
};

}
