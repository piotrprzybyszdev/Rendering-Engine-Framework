#pragma once

#include "vulkan/vulkan.hpp"

namespace ref::vulkan
{

struct SwapchainSpec
{
    vk::SurfaceKHR Surface;
    vk::ImageUsageFlags UsageFlags;
    vk::PresentModeKHR PresentMode;
    vk::SurfaceFormatKHR SurfaceFormat;
    vk::Extent2D Extent;
    uint32_t ImageCount;
    std::span<const uint32_t> PresentQueueFamilies;
};

class Swapchain
{
public:
    struct Frame
    {
        vk::Image Image;
        vk::ImageView ImageView;
    };

    struct SynchronizationObjects
    {
        vk::Semaphore ImageAcquiredSemaphore;
        vk::Semaphore RenderCompleteSemaphore;
        vk::Fence InFlightFence;
    };

public:
    Swapchain(vk::Device device, const SwapchainSpec &spec, const Swapchain *old = nullptr);

    bool AcquireImage(vk::Device device);
    bool Present(vk::Queue queue);

    vk::SurfaceKHR GetSurface() const;

    const Frame &GetCurrentFrame() const;
    const SynchronizationObjects &GetCurrentSynchronizationObjects();

private:
    const vk::SurfaceKHR m_Surface;
    const vk::PresentModeKHR m_PresentMode;
    const vk::SurfaceFormatKHR m_SurfaceFormat;
    const vk::Extent2D m_Extent;
    const uint32_t m_ImageCount = 2;
    const uint32_t m_InFlightCount = 1;

    vk::SwapchainKHR m_Handle;

    std::vector<Frame> m_Frames;
    std::vector<SynchronizationObjects> m_SynchronizationObjects;

    uint32_t m_CurrentFrameInFlightIndex = 0;
    uint32_t m_CurrentFrameIndex = 0;
};

}