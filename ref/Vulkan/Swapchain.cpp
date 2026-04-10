#include <ranges>

#include "Core/Core.h"

#include "Application.h"
#include "Swapchain.h"

namespace ref::vulkan
{

Swapchain::Swapchain(vk::Device device, const SwapchainSpec &spec, const Swapchain *old)
    : m_Surface(spec.Surface), m_PresentMode(spec.PresentMode), m_SurfaceFormat(spec.SurfaceFormat),
      m_Extent(spec.Extent), m_ImageCount(spec.ImageCount), m_InFlightCount(m_ImageCount - 1)
{
    logger::debug("Current present mode: {}", vk::to_string(m_PresentMode));
    logger::debug("Swapchain Image Count: {}", m_ImageCount);
    logger::debug("Frame In Flight Count: {}", m_InFlightCount);
    logger::debug("Swapchain resizing to: {}x{}", m_Extent.width, m_Extent.height);

    auto presentQueueFamilies = spec.PresentQueueFamilies;

    vk::SwapchainKHR oldSwapchainHandle = nullptr;
    if (old)
        oldSwapchainHandle = old->m_Handle;

    vk::SwapchainCreateInfoKHR createInfo(
        vk::SwapchainCreateFlagsKHR(), m_Surface, m_ImageCount, m_SurfaceFormat.format,
        m_SurfaceFormat.colorSpace, m_Extent, 1, spec.UsageFlags,
        presentQueueFamilies.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
        presentQueueFamilies, vk::SurfaceTransformFlagBitsKHR::eIdentity,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, m_PresentMode, vk::True, oldSwapchainHandle
    );

    m_Handle = device.createSwapchainKHR(createInfo);

    for (const Frame &frame : m_Frames)
        device.destroyImageView(frame.ImageView);
    m_Frames.clear();

    uint32_t i = 0;
    for (vk::Image image : device.getSwapchainImagesKHR(m_Handle))
    {
        vk::ImageViewCreateInfo imageViewCreateInfo(
            vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, m_SurfaceFormat.format,
            vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        auto imageView = device.createImageView(imageViewCreateInfo);
        m_Frames.emplace_back(image, imageView);

        m_RenderCompleteSemaphores.emplace_back(device.createSemaphore(vk::SemaphoreCreateInfo()));

        Application::GetInstance()->SetDebugName(
            m_Frames.back().Image, std::format("Swapchain Image {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            m_Frames.back().ImageView, std::format("Swapchain ImageView {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            m_RenderCompleteSemaphores.back(),
            std::format("Swapchain Render Complete Semaphore {}", i).c_str()
        );

        i++;
    }

    i = 0;
    while (m_InFlightFences.size() < m_Frames.size())
    {
        m_ImageAcquiredSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));
        m_InFlightFences.push_back(
            device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled))
        );

        Application::GetInstance()->SetDebugName(
            m_ImageAcquiredSemaphores.back(), std::format("Swapchain Image Acquired Semaphore {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            m_InFlightFences.back(), std::format("Swapchain In Flight Fence {}", i).c_str()
        );

        i++;
    }

    device.destroySwapchainKHR(oldSwapchainHandle);

    UpdateCurrentSyncronizationObjects();
}

vk::SurfaceKHR Swapchain::GetSurface() const
{
    return m_Surface;
}

vk::Extent2D Swapchain::GetExtent() const
{
    return m_Extent;
}

uint32_t Swapchain::GetInFlightCount() const
{
    return m_InFlightCount;
}

uint32_t Swapchain::GetCurrentFrameInFlightIndex() const
{
    return m_CurrentFrameInFlightIndex;
}

bool Swapchain::AcquireImage(vk::Device device)
{
    const SynchronizationObjects &sync = GetCurrentSynchronizationObjects();

    {
        vk::Result result =
            device.waitForFences({ sync.InFlightFence }, vk::True, std::numeric_limits<uint64_t>::max());

        assert(result == vk::Result::eSuccess);
        if (result != vk::Result::eSuccess)
            logger::warn("Swapchain wait fence: {}", vk::to_string(result));
    }

    try
    {
        vk::ResultValue result = device.acquireNextImageKHR(
            m_Handle, std::numeric_limits<uint64_t>::max(), sync.ImageAcquiredSemaphore, nullptr
        );

        m_CurrentFrameIndex = result.value;

        if (result.result == vk::Result::eSuboptimalKHR)
            logger::warn("Swapchain Acquire: {}", vk::to_string(result.result));
        else
            assert(result.result == vk::Result::eSuccess);
    }
    catch (const vk::OutOfDateKHRError &error)
    {
        logger::warn(error.what());
        return false;
    }

    device.resetFences({ sync.InFlightFence });
    UpdateCurrentSyncronizationObjects();

    return true;
}

bool Swapchain::Present(vk::Queue queue)
{
    const SynchronizationObjects &sync = GetCurrentSynchronizationObjects();

    vk::PresentInfoKHR presentInfo({ sync.RenderCompleteSemaphore }, { m_Handle }, { m_CurrentFrameIndex });
    try
    {
        vk::Result res = queue.presentKHR(presentInfo);
        if (res == vk::Result::eSuboptimalKHR)
        {
            logger::warn("Swapchain Present: {}", vk::to_string(res));
            return false;
        }
        assert(res == vk::Result::eSuccess);
    }
    catch (const vk::OutOfDateKHRError &error)
    {
        logger::warn(error.what());
        return false;
    }

    m_CurrentFrameIndex++;
    if (m_CurrentFrameIndex == m_ImageCount)
        m_CurrentFrameIndex = 0;

    m_CurrentFrameInFlightIndex++;
    if (m_CurrentFrameInFlightIndex == m_InFlightCount)
        m_CurrentFrameInFlightIndex = 0;
    UpdateCurrentSyncronizationObjects();

    return true;
}

const Swapchain::Frame &Swapchain::GetCurrentFrame() const
{
    return m_Frames[m_CurrentFrameIndex];
}

const Swapchain::SynchronizationObjects &Swapchain::GetCurrentSynchronizationObjects() const
{
    return m_CurrentSynchronizationObjects;
}

void Swapchain::UpdateCurrentSyncronizationObjects()
{
    m_CurrentSynchronizationObjects = {
        .ImageAcquiredSemaphore = m_ImageAcquiredSemaphores[m_CurrentFrameInFlightIndex],
        .RenderCompleteSemaphore = m_RenderCompleteSemaphores[m_CurrentFrameIndex],
        .InFlightFence = m_InFlightFences[m_CurrentFrameInFlightIndex],
    };
}

SwapchainBuilder::SwapchainBuilder(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
    : m_PhysicalDevice(physicalDevice), m_Surface(surface)
{
    RefreshSurfaceCabilities();
}

void SwapchainBuilder::RefreshSurfaceCabilities()
{
    m_SurfaceCapabilities = m_PhysicalDevice.getSurfaceCapabilitiesKHR(m_Surface);
    logger::trace("Supported usage flags: {}", vk::to_string(m_SurfaceCapabilities.supportedUsageFlags));
    logger::trace("Supported transforms: {}", vk::to_string(m_SurfaceCapabilities.supportedTransforms));
    logger::trace(
        "Supported composite alpha: {}", vk::to_string(m_SurfaceCapabilities.supportedCompositeAlpha)
    );

    m_SurfaceFormats = m_PhysicalDevice.getSurfaceFormatsKHR(m_Surface);
    for (vk::SurfaceFormatKHR format : m_SurfaceFormats)
        logger::trace(
            "Supported format: {} ({})", vk::to_string(format.format), vk::to_string(format.colorSpace)
        );

    m_PresentModes = m_PhysicalDevice.getSurfacePresentModesKHR(m_Surface);

    for (vk::PresentModeKHR mode : m_PresentModes)
        logger::trace("Supported present mode: {}", vk::to_string(mode));

    logger::trace(
        "Surface allowed image count: {} - {}", m_SurfaceCapabilities.minImageCount,
        m_SurfaceCapabilities.maxImageCount
    );

    std::array<std::pair<std::string_view, vk::Extent2D>, 3> extents = { {
        { "min", m_SurfaceCapabilities.minImageExtent },
        { "max", m_SurfaceCapabilities.maxImageExtent },
        { "current", m_SurfaceCapabilities.currentExtent },
    } };

    for (auto &[name, extent] : extents)
        logger::trace("Surface {} extent: {}x{}", name, extent.width, extent.height);

    m_Spec.Surface = m_Surface;
}

std::span<const vk::PresentModeKHR> SwapchainBuilder::GetPresentModes() const
{
    return m_PresentModes;
}

std::span<const vk::SurfaceFormatKHR> SwapchainBuilder::GetSurfaceFormats() const
{
    return m_SurfaceFormats;
}

uint32_t SwapchainBuilder::GetMinImageCount() const
{
    return m_SurfaceCapabilities.minImageCount;
}

uint32_t SwapchainBuilder::GetMaxImageCount() const
{
    return m_SurfaceCapabilities.maxImageCount == 0 ? std::numeric_limits<uint32_t>::max() : m_SurfaceCapabilities.maxImageCount;
}

void SwapchainBuilder::SetPresentQueueFamilies(std::vector<uint32_t> familyIndices)
{
    m_Spec.PresentQueueFamilies = familyIndices;
}

void SwapchainBuilder::SetUsageFlags(vk::ImageUsageFlags usageFlags)
{
    assert((m_SurfaceCapabilities.supportedUsageFlags & usageFlags) == usageFlags);
    m_Spec.UsageFlags = usageFlags;
}

void SwapchainBuilder::SetPresentMode(vk::PresentModeKHR presentMode)
{
    assert(std::ranges::contains(m_PresentModes, presentMode));
    m_Spec.PresentMode = presentMode;
}

void SwapchainBuilder::SetSurfaceFormat(vk::SurfaceFormatKHR format)
{
    assert(std::ranges::contains(m_SurfaceFormats, format));
    m_Spec.SurfaceFormat = format;
}

void SwapchainBuilder::SetExtent(vk::Extent2D extent)
{
    assert(
        m_SurfaceCapabilities.maxImageExtent.width >= extent.width &&
        extent.width >= m_SurfaceCapabilities.minImageExtent.width
    );
    assert(
        m_SurfaceCapabilities.minImageExtent.height >= extent.height &&
        extent.height >= m_SurfaceCapabilities.maxImageExtent.height
    );
    m_Spec.Extent = extent;
}

void SwapchainBuilder::SetImageCount(uint32_t count)
{
    assert(count >= m_SurfaceCapabilities.minImageCount);
    assert(m_SurfaceCapabilities.maxImageCount == 0 || count <= m_SurfaceCapabilities.maxImageCount);
    m_Spec.ImageCount = count;
}

Swapchain SwapchainBuilder::CreateSwapchain(vk::Device device, const Swapchain *old)
{
    return Swapchain(device, m_Spec, old);
}

std::unique_ptr<Swapchain> SwapchainBuilder::CreateSwapchainUnique(vk::Device device, const Swapchain *old)
{
    return std::make_unique<Swapchain>(device, m_Spec, old);
}

}
