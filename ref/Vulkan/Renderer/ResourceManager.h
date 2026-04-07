#pragma once

#include <vulkan/vulkan.hpp>

#include "Core/Core.h"

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace ref::vulkan
{

struct ResourceManagerSpec
{
    uint32_t ApiVersion;
    vk::Instance Instance;
    vk::PhysicalDevice PhysicalDevice;
    vk::Device LogicalDevice;
};

struct BufferResource
{
    std::string Name;
    vk::Buffer Handle;
    vk::DeviceSize Size;
    bool IsDevice;
};
using BufferResourceId = IdType<size_t, BufferResource>;

struct ImageResource
{
    std::string Name;
    vk::Image Handle;
    vk::Extent3D Extent;
    bool IsDevice;
};
using ImageResourceId = IdType<size_t, ImageResource>;

struct ImageViewResource
{
    std::string Name;
    vk::ImageView Handle;
};
using ImageViewResourceId = IdType<size_t, ImageViewResource>;

class ResourceAllocator
{
public:
    ResourceAllocator(ResourceManagerSpec spec);
    ~ResourceAllocator();

    ResourceAllocator(const ResourceAllocator &) = delete;
    ResourceAllocator &operator=(const ResourceAllocator &) = delete;

    [[nodiscard]] BufferResourceId AddBufferResource(const std::string &name);
    [[nodiscard]] ImageResourceId AddImageResource(const std::string &name);
    [[nodiscard]] ImageViewResourceId AddImageViewResource(const std::string &name);

    void AllocateResource(BufferResourceId id, vk::BufferCreateInfo info, bool isDevice);
    void AllocateResource(ImageResourceId id, vk::ImageCreateInfo info, bool isDevice);
    void AllocateResource(ImageViewResourceId id, vk::ImageViewCreateInfo info);

    const BufferResource &GetBufferResource(BufferResourceId id) const;
    const ImageResource &GetImageResource(ImageResourceId id) const;
    const ImageViewResource &GetImageViewResource(ImageViewResourceId id) const;

    void UploadToBuffer(
        BufferResourceId id, const void *data, vk::DeviceSize size, vk::DeviceSize offset = 0
    ) const;

private:
    VmaAllocator m_Allocator;
    vk::Device m_LogicalDevice;

    std::vector<BufferResource> m_BufferResources;
    std::vector<ImageResource> m_ImageResources;
    std::vector<ImageViewResource> m_ImageViewResources;

    std::vector<VmaAllocation> m_BufferAllocations;
    std::vector<VmaAllocation> m_ImageAllocations;
};

}