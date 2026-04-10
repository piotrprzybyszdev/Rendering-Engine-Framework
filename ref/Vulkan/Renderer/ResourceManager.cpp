#include <vk_mem_alloc.h>

#include <ranges>

#include "Core/Core.h"

#include "Vulkan/Application.h"

#include "ResourceManager.h"

namespace ref::vulkan
{

ResourceAllocator::ResourceAllocator(ResourceManagerSpec spec) : m_LogicalDevice(spec.LogicalDevice)
{
    VmaAllocatorCreateInfo allocatorInfo = {
        // TODO (make sure it's enabled): .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = spec.PhysicalDevice,
        .device = spec.LogicalDevice,
        .instance = spec.Instance,
        .vulkanApiVersion = spec.ApiVersion,
    };

    VkResult result = vmaCreateAllocator(&allocatorInfo, &m_Allocator);
    if (result != VkResult::VK_SUCCESS)
        throw std::runtime_error("Failed to initialize resource manager");
}

ResourceAllocator::~ResourceAllocator()
{
    for (const auto &[buffer, allocation] : std::views::zip(m_BufferResources, m_BufferAllocations))
        vmaDestroyBuffer(m_Allocator, buffer.Handle, allocation);

    for (const auto &[image, allocation] : std::views::zip(m_ImageResources, m_ImageAllocations))
        vmaDestroyImage(m_Allocator, image.Handle, allocation);

    for (const auto &view : m_ImageViewResources)
        m_LogicalDevice.destroyImageView(view.Handle);

    vmaDestroyAllocator(m_Allocator);
}

BufferResourceId ResourceAllocator::AddBufferResource(const std::string &name)
{
    m_BufferResources.emplace_back(name);
    m_BufferAllocations.emplace_back();
    return BufferResourceId(m_BufferResources.size() - 1);
}

ImageResourceId ResourceAllocator::AddImageResource(const std::string &name)
{
    m_ImageResources.emplace_back(name);
    m_ImageAllocations.emplace_back();
    return ImageResourceId(m_ImageResources.size() - 1);
}

ImageViewResourceId ResourceAllocator::AddImageViewResource(const std::string &name)
{
    m_ImageViewResources.emplace_back(name);
    return ImageViewResourceId(m_ImageViewResources.size() - 1);
}

void ResourceAllocator::AllocateResource(BufferResourceId id, vk::BufferCreateInfo info, bool isDevice)
{
    assert(info.size > 0);

    auto &buffer = m_BufferResources[id];
    auto &allocation = m_BufferAllocations[id];
    vmaDestroyBuffer(m_Allocator, buffer.Handle, allocation);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = isDevice ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE : VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    if (!isDevice)
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkBuffer handle = nullptr;
    [[maybe_unused]] VkResult result =
        vmaCreateBuffer(m_Allocator, info, &allocInfo, &handle, &allocation, nullptr);

    // TODO: Make sure the prefer worked
    assert(result == VkResult::VK_SUCCESS);
    buffer.Handle = handle;
    buffer.Size = info.size;
    buffer.IsDevice = isDevice;

    Application::GetInstance()->SetDebugName(buffer.Handle, buffer.Name.c_str());
    logger::trace("Allocating buffer resource `{}`", buffer.Name);
}

void ResourceAllocator::AllocateResource(ImageResourceId id, vk::ImageCreateInfo info, bool isDevice)
{
    assert(info.extent.width > 0 && info.extent.height > 0 && info.extent.depth > 0);

    auto &image = m_ImageResources[id];
    auto &allocation = m_ImageAllocations[id];
    vmaDestroyImage(m_Allocator, image.Handle, allocation);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = isDevice ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE : VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

    VkImage handle = nullptr;
    [[maybe_unused]] VkResult result =
        vmaCreateImage(m_Allocator, info, &allocInfo, &handle, &allocation, nullptr);

    // TODO: Make sure the prefer worked
    assert(result == VkResult::VK_SUCCESS);
    image.Handle = handle;
    image.Extent = info.extent;
    image.IsDevice = isDevice;

    Application::GetInstance()->SetDebugName(image.Handle, image.Name.c_str());
    logger::trace("Allocating image resource `{}`", image.Name);
}

void ResourceAllocator::AllocateResource(ImageViewResourceId id, vk::ImageViewCreateInfo info)
{
    auto &view = m_ImageViewResources[id];
    m_LogicalDevice.destroyImageView(view.Handle);

    vk::ImageView handle = m_LogicalDevice.createImageView(info);

    view.Handle = handle;

    Application::GetInstance()->SetDebugName(view.Handle, view.Name.c_str());
    logger::trace("Allocating image view resource `{}`", view.Name);
}

const BufferResource &ResourceAllocator::GetBufferResource(BufferResourceId id) const
{
    return m_BufferResources[id];
}

const ImageResource &ResourceAllocator::GetImageResource(ImageResourceId id) const
{
    return m_ImageResources[id];
}

const ImageViewResource &ResourceAllocator::GetImageViewResource(ImageViewResourceId id) const
{
    return m_ImageViewResources[id];
}

void ResourceAllocator::UploadToBuffer(
    BufferResourceId id, const void *data, vk::DeviceSize size, vk::DeviceSize offset
) const
{
    [[maybe_unused]] const auto &buffer = m_BufferResources[id];
    const auto &allocation = m_BufferAllocations[id];

    assert(buffer.IsDevice == false);
    assert(buffer.Size >= size);
    assert(data != nullptr);

    [[maybe_unused]] VkResult result = vmaCopyMemoryToAllocation(m_Allocator, data, allocation, offset, size);
    assert(result == VkResult::VK_SUCCESS);
}

}