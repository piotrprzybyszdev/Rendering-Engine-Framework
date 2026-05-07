#pragma once

#include <vulkan/vulkan.hpp>

#include <memory>
#include <span>
#include <vector>

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
    std::vector<uint32_t> PresentQueueFamilies;
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
    ~Swapchain();

    Swapchain(const Swapchain &) = delete;
    Swapchain &operator=(const Swapchain &) = delete;

    bool AcquireImage();
    bool Present(vk::Queue queue);

    vk::SurfaceKHR GetSurface() const;

    vk::Extent2D GetExtent() const;
    uint32_t GetInFlightCount() const;
    uint32_t GetCurrentFrameInFlightIndex() const;

    const Frame &GetCurrentFrame() const;
    const SynchronizationObjects &GetCurrentSynchronizationObjects() const;

private:
    const vk::Device m_LogicalDevice;
    const vk::SurfaceKHR m_Surface;
    const vk::PresentModeKHR m_PresentMode;
    const vk::SurfaceFormatKHR m_SurfaceFormat;
    const vk::Extent2D m_Extent;
    const uint32_t m_ImageCount = 2;
    const uint32_t m_InFlightCount = 1;

    vk::SwapchainKHR m_Handle;

    std::vector<Frame> m_Frames;
    std::vector<vk::Fence> m_InFlightFences;
    std::vector<vk::Semaphore> m_ImageAcquiredSemaphores;
    std::vector<vk::Semaphore> m_RenderCompleteSemaphores;

    SynchronizationObjects m_CurrentSynchronizationObjects;

    uint32_t m_CurrentFrameInFlightIndex = 0;
    uint32_t m_CurrentFrameIndex = 0;

private:
    void UpdateCurrentSyncronizationObjects();
};

class SwapchainBuilder
{
public:
    SwapchainBuilder(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);

    void RefreshSurfaceCabilities();

    std::span<const vk::PresentModeKHR> GetPresentModes() const;
    std::span<const vk::SurfaceFormatKHR> GetSurfaceFormats() const;
    uint32_t GetMinImageCount() const;
    uint32_t GetMaxImageCount() const;

    void SetPresentQueueFamilies(std::vector<uint32_t> familyIndices);
    void SetUsageFlags(vk::ImageUsageFlags usageFlags);
    void SetPresentMode(vk::PresentModeKHR presentMode);
    void SetSurfaceFormat(vk::SurfaceFormatKHR format);
    void SetExtent(vk::Extent2D extent);
    void SetImageCount(uint32_t count);

    Swapchain CreateSwapchain(vk::Device device, const Swapchain *old = nullptr);
    std::unique_ptr<Swapchain> CreateSwapchainUnique(vk::Device device, const Swapchain *old = nullptr);

private:
    vk::PhysicalDevice m_PhysicalDevice;
    vk::SurfaceKHR m_Surface;

    vk::SurfaceCapabilitiesKHR m_SurfaceCapabilities;
    std::vector<vk::SurfaceFormatKHR> m_SurfaceFormats;
    std::vector<vk::PresentModeKHR> m_PresentModes;

    SwapchainSpec m_Spec;
};

}
