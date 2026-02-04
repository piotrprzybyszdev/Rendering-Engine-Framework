#include "Core/Core.h"

#include "Application.h"
#include "Swapchain.h"

namespace ref::vulkan
{

Swapchain::Swapchain(vk::Device device, const SwapchainSpec &spec, const Swapchain *old)
    : m_Surface(spec.Surface), m_PresentMode(spec.PresentMode), m_SurfaceFormat(spec.SurfaceFormat),
      m_Extent(spec.Extent), m_ImageCount(spec.ImageCount)
{
    logger::info("Current present mode: {}", vk::to_string(m_PresentMode));
    logger::info("Swapchain Image Count: {}", m_ImageCount);
    logger::info("Frame In Flight Count: {}", m_InFlightCount);
    logger::info("Swapchain resizing to: {}x{}", m_Extent.width, m_Extent.height);

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

    for (vk::Image image : device.getSwapchainImagesKHR(m_Handle))
    {
        vk::ImageViewCreateInfo imageViewCreateInfo(
            vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, m_SurfaceFormat.format,
            vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        auto imageView = device.createImageView(imageViewCreateInfo);
        m_Frames.emplace_back(image, imageView);
    }

    while (m_SynchronizationObjects.size() < m_Frames.size())
    {
        m_SynchronizationObjects.push_back(
            {
                device.createSemaphore(vk::SemaphoreCreateInfo()),
                device.createSemaphore(vk::SemaphoreCreateInfo()),
                device.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)),
            }
        );
    }

    device.destroySwapchainKHR(oldSwapchainHandle);

    m_CurrentFrameInFlightIndex = 0;
    m_CurrentFrameIndex = 0;

    for (size_t i = 0; i < m_Frames.size(); i++)
    {
        Application::GetInstance()->SetDebugName(
            m_Frames[i].Image, std::format("Swapchain Image {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            m_Frames[i].ImageView, std::format("Swapchain ImageView {}", i).c_str()
        );
    }

    for (size_t i = 0; i < m_SynchronizationObjects.size(); i++)
    {
        Application::GetInstance()->SetDebugName(
            m_SynchronizationObjects[i].ImageAcquiredSemaphore, std::format("Swapchain Image Acquired Semaphore {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            m_SynchronizationObjects[i].RenderCompleteSemaphore, std::format("Swapchain Render Complete Semaphore {}", i).c_str()
        );
        Application::GetInstance()->SetDebugName(
            m_SynchronizationObjects[i].InFlightFence, std::format("Swapchain In Flight Fence {}", i).c_str()
        );
    }
}

vk::SurfaceKHR Swapchain::GetSurface() const
{
    return m_Surface;
}

bool Swapchain::AcquireImage(vk::Device device)
{
    const SynchronizationObjects &sync = m_SynchronizationObjects[m_CurrentFrameInFlightIndex];

    {
        vk::Result result = device.waitForFences(
            { sync.InFlightFence }, vk::True, std::numeric_limits<uint64_t>::max()
        );

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

    return true;
}

bool Swapchain::Present(vk::Queue queue)
{
    const SynchronizationObjects &sync = m_SynchronizationObjects[m_CurrentFrameInFlightIndex];

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

    m_CurrentFrameInFlightIndex++;
    if (m_CurrentFrameInFlightIndex == m_InFlightCount)
        m_CurrentFrameInFlightIndex = 0;

    return true;
}

const Swapchain::Frame &Swapchain::GetCurrentFrame() const
{
    return m_Frames[m_CurrentFrameIndex];
}

const Swapchain::SynchronizationObjects &Swapchain::GetCurrentSynchronizationObjects()
{
    return m_SynchronizationObjects[m_CurrentFrameInFlightIndex];
}

}