#pragma once

#include <vulkan/vulkan.hpp>

#include <map>
#include <span>
#include <vector>

#include "RenderPass.h"
#include "ResourceManager.h"
#include "PipelineLibrary.h"

namespace ref::vulkan
{

struct Buffer
{
    vk::BufferCreateInfo Info;
    bool IsDevice;
    bool IsBuffered;
    bool IsPersistent;
    std::vector<BufferResourceId> Resources;

    BufferResourceId GetResourceId(uint32_t frameInFlight);
};

struct Image
{
    vk::ImageCreateInfo Info;
    vk::ImageViewCreateInfo ViewInfo;
    bool IsDevice;
    bool IsBuffered;
    bool IsPersistent;
    std::vector<std::pair<ImageResourceId, ImageViewResourceId>> Resources;

    std::pair<ImageResourceId, ImageViewResourceId> GetResourceId(uint32_t frameInFlight);
};

struct ClearPass
{
    ClearPassSpec Spec;
};

struct BlitPass
{
    BlitPassSpec Spec;
};

struct ComputePass
{
    static inline constexpr vk::PipelineBindPoint BindPoint = vk::PipelineBindPoint::eCompute;

    ComputePassSpec Spec;

    std::unique_ptr<DescriptorSet> Set;
};

struct GraphicsPass
{
    static inline constexpr vk::PipelineBindPoint BindPoint = vk::PipelineBindPoint::eGraphics;

    GraphicsPassSpec Spec;

    std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
    std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
    std::optional<vk::RenderingAttachmentInfo> StencilAttachment;

    std::unique_ptr<DescriptorSet> Set;

    std::vector<vk::Buffer> VertexBuffers;
    std::vector<vk::DeviceSize> VertexBufferOffsets;
};

struct IndexedGraphicsPass
{
    static inline constexpr vk::PipelineBindPoint BindPoint = vk::PipelineBindPoint::eGraphics;

    IndexedGraphicsPassSpec Spec;

    std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
    std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
    std::optional<vk::RenderingAttachmentInfo> StencilAttachment;

    std::unique_ptr<DescriptorSet> Set;

    std::vector<vk::Buffer> VertexBuffers;
    std::vector<vk::DeviceSize> VertexBufferOffsets;
};

struct IndexedIndirectGraphicsPass
{
    static inline constexpr vk::PipelineBindPoint BindPoint = vk::PipelineBindPoint::eGraphics;

    IndexedIndirectGraphicsPassSpec Spec;

    std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
    std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
    std::optional<vk::RenderingAttachmentInfo> StencilAttachment;

    std::unique_ptr<DescriptorSet> Set;

    std::vector<vk::Buffer> VertexBuffers;
    std::vector<vk::DeviceSize> VertexBufferOffsets;
};

struct CustomGraphicsPass
{
    CustomGraphicsPassSpec Spec;

    std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
    std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
    std::optional<vk::RenderingAttachmentInfo> StencilAttachment;
};

enum class PassType
{
    Clear,
    Blit,
    Compute,
    Graphics,
    IndexedGraphics,
    IndexedIndirectGraphics,
    CustomGraphics,
};

struct PassExecution
{
    PassType Type;
    std::string Name;

    struct PassBufferBarriers
    {
        std::vector<std::string> Resources;
        std::vector<vk::BufferMemoryBarrier2> Barriers;
    } BufferBarriers;

    struct PassImageBarriers
    {
        std::vector<std::string> Resources;
        std::vector<vk::ImageMemoryBarrier2> Barriers;
    } ImageBarriers;
};

struct FrameGraphSpec
{
    struct ClearPassInfo
    {
        ClearPassSpec Spec;
    };

    struct BlitPassInfo
    {
        BlitPassSpec Spec;
    };

    struct ComputePassInfo
    {
        ComputePassSpec Spec;
    };

    struct GraphicsPassInfo
    {
        GraphicsPassSpec Spec;
        std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
        std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
        std::optional<vk::RenderingAttachmentInfo> StencilAttachment;
    };

    struct IndexedGraphicsPassInfo
    {
        IndexedGraphicsPassSpec Spec;
        std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
        std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
        std::optional<vk::RenderingAttachmentInfo> StencilAttachment;
    };

    struct IndexedIndirectGraphicsPassInfo
    {
        IndexedIndirectGraphicsPassSpec Spec;
        std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
        std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
        std::optional<vk::RenderingAttachmentInfo> StencilAttachment;
    };

    struct CustomGraphicsPassInfo
    {
        CustomGraphicsPassSpec Spec;
        std::vector<vk::RenderingAttachmentInfo> ColorAttachments;
        std::optional<vk::RenderingAttachmentInfo> DepthAttachment;
        std::optional<vk::RenderingAttachmentInfo> StencilAttachment;
    };

    std::map<std::string, Buffer> Buffers;
    std::map<std::string, Image> Images;

    std::map<std::string, ClearPassInfo> ClearPasses;
    std::map<std::string, BlitPassInfo> BlitPasses;
    std::map<std::string, ComputePassInfo> ComputePasses;
    std::map<std::string, GraphicsPassInfo> GraphicsPasses;
    std::map<std::string, IndexedGraphicsPassInfo> IndexedGraphicsPasses;
    std::map<std::string, IndexedIndirectGraphicsPassInfo> IndexedIndirectGraphicsPasses;
    std::map<std::string, CustomGraphicsPassInfo> CustomGraphicsPasses;

    std::vector<PassExecution> PassExecutions;
    std::vector<vk::ImageMemoryBarrier2> PresentImageBarriers;
};

class FrameGraph
{
public:
    struct FrameInfo
    {
        uint32_t FrameInFlightIndex;
        uint32_t FrameInFlightCount;
        vk::Image Image;
        vk::ImageView ImageView;
        vk::Extent2D Extent;
    };

public:
    static inline const std::string g_SwapchainImageResourceName = "REF Swapchain Image Resource";

public:
    FrameGraph(PipelineLibrary *pipelineLibrary, ResourceAllocator *resourceAllocator, FrameGraphSpec spec);

    FrameGraph(const FrameGraph &) = delete;
    FrameGraph &operator=(const FrameGraph &) = delete;

    std::span<const BufferResourceId> GetBuffer(const std::string &name);
    std::span<const std::pair<ImageResourceId, ImageViewResourceId>> GetImage(const std::string &name);

    Buffer &ModifyBuffer(const std::string &name);
    Image &ModifyImage(const std::string &name);

    void UpdateBuffer(const std::string &name);
    void UpdateImage(const std::string &name);

    BufferResourceId GetCurrentBuffer(const std::string &name);
    ImageResourceId GetCurrentImage(const std::string &name);
    ImageViewResourceId GetCurrentImageView(const std::string &name);

    ClearPassDynamicConfig GetClearPassDynamicConfig(const std::string &name);
    BlitPassDynamicConfig GetBlitPassDynamicConfig(const std::string &name);
    ComputePassDynamicConfig GetComputePassDynamicConfig(const std::string &name);
    GraphicsPassDynamicConfig GetGraphicsPassDynamicConfig(const std::string &name);
    IndexedGraphicsPassDynamicConfig GetIndexedGraphicsPassDynamicConfig(const std::string &name);
    IndexedIndirectGraphicsPassDynamicConfig GetIndexedIndirectGraphicsPassDynamicConfig(
        const std::string &name
    );
    CustomGraphicsPassDynamicConfig GetCustomGraphicsPassDynamicConfig(const std::string &name);

    void SetFrame(const FrameInfo &frameInfo);
    void OnRender(vk::CommandBuffer commandBuffer);

private:
    PipelineLibrary *m_PipelineLibrary;
    ResourceAllocator *m_ResourceAllocator;

    uint32_t m_RenderingResourcesCount = 0;
    std::map<std::string, Buffer> m_BufferResources;
    std::map<std::string, Image> m_ImageResources;

    std::map<std::string, ClearPass> m_ClearPasses;
    std::map<std::string, BlitPass> m_BlitPasses;
    std::map<std::string, ComputePass> m_ComputePasses;
    std::map<std::string, GraphicsPass> m_GraphicsPasses;
    std::map<std::string, IndexedGraphicsPass> m_IndexedGraphicsPasses;
    std::map<std::string, IndexedIndirectGraphicsPass> m_IndexedIndirectGraphicsPasses;
    std::map<std::string, CustomGraphicsPass> m_CustomGraphicsPasses;

    std::vector<PassExecution> m_PassExecutions;
    std::vector<vk::ImageMemoryBarrier2> m_PresentImageBarriers;

    FrameInfo m_FrameInfo;

    std::vector<std::string> m_BuffersToUpdate;
    std::vector<std::string> m_ImagesToUpdate;

private:
    BufferResourceId AddAndCreateBuffer(const std::string &name, const Buffer &buffer);
    std::pair<ImageResourceId, ImageViewResourceId> AddAndCreateImage(const std::string &name, Image &image);

    std::pair<vk::Buffer, vk::DeviceSize> GetBufferResource(const std::string &name);
    std::pair<vk::Buffer, vk::DeviceSize> GetBufferResource(const std::string &name, uint32_t frameInFlight);
    vk::Image GetImageHandle(const std::string &name);
    vk::Image GetImageHandle(const std::string &name, uint32_t frameInFlight);
    vk::ImageView GetImageViewHandle(const std::string &name);
    vk::ImageView GetImageViewHandle(const std::string &name, uint32_t frameInFlight);

    template<typename P, typename D>
    void SetPushConstants(vk::CommandBuffer commandBuffer, const P &pass, const D &draw);

    void Execute(vk::CommandBuffer commandBuffer, const ClearPass &pass);
    void Execute(vk::CommandBuffer commandBuffer, const BlitPass &pass);
    void Execute(vk::CommandBuffer commandBuffer, const ComputePass &pass);
    void Execute(vk::CommandBuffer commandBuffer, const GraphicsPass &pass);
    void Execute(vk::CommandBuffer commandBuffer, const IndexedGraphicsPass &pass);
    void Execute(vk::CommandBuffer commandBuffer, const IndexedIndirectGraphicsPass &pass);
    void Execute(vk::CommandBuffer commandBuffer, const CustomGraphicsPass &pass);

    template<typename P>
    void UpdateDescriptors(P &pass, const PassExecution &execution, bool isFrameCountChanged);

    template<typename P> void SetupPass(P &pass, PassExecution &execution);
    template<typename P> void ExecutePass(const P &pass, vk::CommandBuffer commandBuffer);

    template<typename F, typename... Args>
    void Dispatch(const PassExecution &execution, F &&func, Args &&...args);
};

}
