#pragma once

#include <vulkan/vulkan.hpp>

#include <vector>

#include "Vulkan/Swapchain.h"

#include "FrameGraph.h"

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
};

class Renderer
{
public:
    Renderer(RendererSpec spec);

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    void OnResize(const Swapchain *swapchain);

    void OnUpdate(float timeStep);
    void OnRender();

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;

    const Swapchain *m_Swapchain = nullptr;

    FrameGraph *m_CurrentFrameGraph;

    struct RenderingResources
    {
        vk::CommandPool CommandPool;
        vk::CommandBuffer CommandBuffer;
    };

    std::vector<RenderingResources> m_RenderingResources;
};

}
