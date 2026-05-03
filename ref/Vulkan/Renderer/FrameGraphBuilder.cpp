#include <ranges>

#include "Core/Core.h"

#include "FrameGraphBuilder.h"

namespace ref::vulkan
{

void FrameGraphBuilder::AddHostBuffer(
    std::string name, vk::BufferCreateInfo info, ResourceType type, bool buffered
)
{
    if (m_Spec.Buffers.contains(name))
        logger::warn("Buffer `{}` already exists and will be overridden", name);

    m_Spec.Buffers[name] = Buffer(info, false, buffered, type, {});
}

void FrameGraphBuilder::AddDeviceBuffer(
    std::string name, vk::BufferCreateInfo info, ResourceType type, bool buffered
)
{
    if (m_Spec.Buffers.contains(name))
        logger::warn("Buffer `{}` already exists and will be overridden", name);

    m_Spec.Buffers[name] = Buffer(info, true, buffered, type, {});
}

void FrameGraphBuilder::AddHostImage(
    std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, ResourceType type,
    bool buffered
)
{
    if (m_Spec.Images.contains(name))
        logger::warn("Image `{}` already exists and will be overridden", name);

    m_Spec.Images[name] = Image(info, viewInfo, false, buffered, type, {});
}

void FrameGraphBuilder::AddDeviceImage(
    std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, ResourceType type,
    bool buffered
)
{
    if (m_Spec.Images.contains(name))
        logger::warn("Image `{}` already exists and will be overridden", name);

    m_Spec.Images[name] = Image(info, viewInfo, true, buffered, type, {});
}

namespace
{

vk::ImageViewType ToImageViewType(vk::ImageType type)
{
    switch (type)
    {
    case vk::ImageType::e1D:
        return vk::ImageViewType::e1D;
    case vk::ImageType::e2D:
        return vk::ImageViewType::e2D;
    case vk::ImageType::e3D:
        return vk::ImageViewType::e3D;
    default:
        throw std::runtime_error("Unsupported image type");
    }
}

vk::ImageViewCreateInfo ToImageViewCreateInfo(const vk::ImageCreateInfo &info)
{
    auto toAspectFlags = [](vk::Format format) -> vk::ImageAspectFlags {
        switch (format)
        {
        case vk::Format::eD16Unorm:
        case vk::Format::eD32Sfloat:
        case vk::Format::eX8D24UnormPack32:
            return vk::ImageAspectFlagBits::eDepth;
        case vk::Format::eS8Uint:
            return vk::ImageAspectFlagBits::eStencil;
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:
            return vk::ImageAspectFlagBits::eColor;
        }
    };

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.setFormat(info.format);
    viewInfo.setViewType(ToImageViewType(info.imageType));
    viewInfo.setSubresourceRange(
        vk::ImageSubresourceRange(toAspectFlags(info.format), 0, info.arrayLayers, 0, info.mipLevels)
    );
    return viewInfo;
}

}

void FrameGraphBuilder::AddHostImage(
    std::string name, vk::ImageCreateInfo info, ResourceType type, bool buffered
)
{
    if (m_Spec.Images.contains(name))
        logger::warn("Image `{}` already exists and will be overridden", name);

    m_Spec.Images[name] = Image(info, ToImageViewCreateInfo(info), false, buffered, type, {});
}

void FrameGraphBuilder::AddDeviceImage(
    std::string name, vk::ImageCreateInfo info, ResourceType type, bool buffered
)
{
    if (m_Spec.Images.contains(name))
        logger::warn("Image `{}` already exists and will be overridden", name);

    m_Spec.Images[name] = Image(info, ToImageViewCreateInfo(info), true, buffered, type, {});
}

void WarnImageBindings(std::span<const ImageBinding> bindings)
{
    for (const auto &binding : bindings)
        if (binding.ImageResource == FrameGraph::g_SwapchainImageResourceName)
            logger::warn(R"(Binding the Swapchain Image Resource to a pass requires calling every frame:
`FrameGraph::UpdateImage(FrameGraph::g_SwapchainImageResourceName)`.
Consider creating a storage image and blitting the result onto the swapchain image.)");
}

template<typename T> void AssertPass(const std::map<std::string, T> &passes, const std::string &name)
{
    if (passes.contains(name))
        throw std::runtime_error(std::format("Pass `{}` already exists", name));
}

void FrameGraphBuilder::AddClearPass(std::string name, ClearPassSpec spec)
{
    AssertPass(m_Spec.ClearPasses, name);
    m_Spec.ClearPasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::Clear, name);
}

void FrameGraphBuilder::AddBlitPass(std::string name, BlitPassSpec spec)
{
    AssertPass(m_Spec.BlitPasses, name);
    m_Spec.BlitPasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::Blit, name);
}

void FrameGraphBuilder::AddComputePass(std::string name, ComputePassSpec spec)
{
    AssertPass(m_Spec.ComputePasses, name);
    WarnImageBindings(spec.ImageBindings);
    m_Spec.ComputePasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::Compute, name);
}

void FrameGraphBuilder::AddGraphicsPass(std::string name, GraphicsPassSpec spec)
{
    AssertPass(m_Spec.GraphicsPasses, name);
    WarnImageBindings(spec.ImageBindings);
    m_Spec.GraphicsPasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::Graphics, name);
}

void FrameGraphBuilder::AddIndexedGraphicsPass(std::string name, IndexedGraphicsPassSpec spec)
{
    AssertPass(m_Spec.IndexedGraphicsPasses, name);
    WarnImageBindings(spec.ImageBindings);
    m_Spec.IndexedGraphicsPasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::IndexedGraphics, name);
}

void FrameGraphBuilder::AddIndexedIndirectGraphicsPass(std::string name, IndexedIndirectGraphicsPassSpec spec)
{
    AssertPass(m_Spec.IndexedIndirectGraphicsPasses, name);
    WarnImageBindings(spec.ImageBindings);
    m_Spec.IndexedIndirectGraphicsPasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::IndexedIndirectGraphics, name);
}

void FrameGraphBuilder::AddCustomGraphicsPass(std::string name, CustomGraphicsPassSpec spec)
{
    AssertPass(m_Spec.CustomGraphicsPasses, name);
    m_Spec.CustomGraphicsPasses[name] = { std::move(spec) };
    m_Spec.PassExecutions.emplace_back(PassType::CustomGraphics, name);
}

namespace
{

std::string ToString(const vk::BufferMemoryBarrier2 &barrier)
{
    const uint64_t end = barrier.size == vk::WholeSize ? vk::WholeSize : barrier.offset + barrier.size;
    return std::format(
        R"(
    BufferBarrier
        Stage:       {} -> {}
        Access:      {} -> {}
        Subresource: (range: [{}, {}])
)",
        vk::to_string(barrier.srcStageMask), vk::to_string(barrier.dstStageMask),
        vk::to_string(barrier.srcAccessMask), vk::to_string(barrier.dstAccessMask), barrier.offset, end
    );
}

std::string ToString(const vk::ImageMemoryBarrier2 &barrier)
{
    const uint32_t layerEnd =
        barrier.subresourceRange.layerCount == vk::RemainingArrayLayers
            ? vk::RemainingArrayLayers
            : barrier.subresourceRange.baseArrayLayer + barrier.subresourceRange.layerCount;
    const uint32_t mipEnd = barrier.subresourceRange.levelCount == vk::RemainingMipLevels
                                ? vk::RemainingMipLevels
                                : barrier.subresourceRange.baseMipLevel + barrier.subresourceRange.levelCount;
    return std::format(
        R"(
    ImageBarrier
        Stage:       {} -> {}
        Access:      {} -> {}
        Layout:      {} -> {}
        Subresource: ({}, layers: [{}, {}], mips: [{}, {}])
)",
        vk::to_string(barrier.srcStageMask), vk::to_string(barrier.dstStageMask),
        vk::to_string(barrier.srcAccessMask), vk::to_string(barrier.dstAccessMask),
        vk::to_string(barrier.oldLayout), vk::to_string(barrier.newLayout),
        vk::to_string(barrier.subresourceRange.aspectMask), barrier.subresourceRange.baseArrayLayer, layerEnd,
        barrier.subresourceRange.baseMipLevel, mipEnd
    );
}

}

struct FrameGraphAccesses
{
    struct BufferAccess
    {
        vk::PipelineStageFlags2 Stage = vk::PipelineStageFlagBits2::eTopOfPipe;
        vk::AccessFlags2 Access = vk::AccessFlagBits2::eNone;
        vk::DeviceSize Offset = 0;
        vk::DeviceSize Size = vk::WholeSize;
    };

    struct ImageAccess
    {
        vk::PipelineStageFlags2 Stage = vk::PipelineStageFlagBits2::eTopOfPipe;
        vk::AccessFlags2 Access = vk::AccessFlagBits2::eNone;
        vk::ImageLayout Layout = vk::ImageLayout::eUndefined;
        vk::ImageSubresourceRange Range =
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    };

    const std::map<std::string, Buffer> &Buffers;
    const std::map<std::string, Image> &Images;

    std::map<std::string, BufferAccess> BufferResources;
    std::map<std::string, std::vector<ImageAccess>> ImageResources;

    std::map<std::string, vk::ImageLayout> InitialImageLayouts;

    vk::ImageSubresourceRange GetRange(const std::string &image)
    {
        if (image == FrameGraph::g_SwapchainImageResourceName)
            return vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        return Images.at(image).ViewInfo.subresourceRange;
    };

    FrameGraphAccesses(
        const std::map<std::string, Buffer> &buffers, const std::map<std::string, Image> &images
    )
        : Buffers(buffers), Images(images)
    {
        ImageResources[FrameGraph::g_SwapchainImageResourceName].push_back(FrameGraphAccesses::ImageAccess());

        for (const auto &image : Images | std::views::keys)
        {
            ImageAccess access;
            access.Range = GetRange(image);
            ImageResources[image].push_back(access);
        }
    }

    // TODO: batch barriers if both are reads
    void AddBufferBarrier(const std::string &name, BufferAccess dstAccess, PassExecution &execution)
    {
        if (Buffers.at(name).Type == ResourceType::Persistent)
            return;

        BufferAccess srcAccess = BufferResources[name];

        vk::BufferMemoryBarrier2 barrier(
            srcAccess.Stage, srcAccess.Access, dstAccess.Stage, dstAccess.Access, vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored, nullptr, srcAccess.Offset, srcAccess.Size
        );

        BufferResources[name] = dstAccess;

        execution.BufferBarriers.Resources.push_back(name);
        execution.BufferBarriers.Barriers.push_back(barrier);
        logger::trace("Inserting barrier for Buffer `{}`: {}", name, ToString(barrier));
    }

    // TODO: batch barriers if both are reads
    void AddImageBarrier(const std::string &name, ImageAccess dstAccess, PassExecution &execution)
    {
        if (name != FrameGraph::g_SwapchainImageResourceName)
        {
            if (Images.at(name).Type == ResourceType::Persistent)
                return;

            if (Images.at(name).Type == ResourceType::Temporal)
            {
                // for the first barrier assume that the image is already in the correct layout
                InitialImageLayouts.emplace(name, dstAccess.Layout);
                auto &accesses = ImageResources.at(name);
                assert(accesses.size() == 1);
                accesses.front().Layout = dstAccess.Layout;
            }
        }

        auto getMaxLayer = [](const vk::ImageSubresourceRange &range) {
            return range.baseArrayLayer + range.layerCount - 1;
        };
        auto getMaxMip = [](const vk::ImageSubresourceRange &range) {
            return range.baseMipLevel + range.levelCount - 1;
        };

        std::vector<ImageAccess> accesses;
        for (ImageAccess &srcAccess : ImageResources.at(name))
        {
            vk::ImageSubresourceRange intersection;
            {
                const auto &r1 = srcAccess.Range, &r2 = dstAccess.Range;
                uint32_t layer = std::max(r1.baseArrayLayer, r2.baseArrayLayer);
                uint32_t mip = std::max(r1.baseMipLevel, r2.baseMipLevel);
                uint32_t layerCount = std::min(getMaxLayer(r1), getMaxLayer(r2)) - layer + 1;
                uint32_t mipCount = std::min(getMaxMip(r1), getMaxMip(r2)) - mip + 1;
                intersection = vk::ImageSubresourceRange(r2.aspectMask, mip, mipCount, layer, layerCount);
            }

            if (intersection.levelCount == 0 || intersection.layerCount == 0)
            {
                accesses.push_back(srcAccess);
                continue;
            }

            vk::ImageMemoryBarrier2 barrier(
                srcAccess.Stage, srcAccess.Access, dstAccess.Stage, dstAccess.Access, srcAccess.Layout,
                dstAccess.Layout, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, nullptr, intersection
            );
            execution.ImageBarriers.Resources.push_back(name);
            execution.ImageBarriers.Barriers.push_back(barrier);
            logger::trace("Inserting barrier for Image `{}`: {}", name, ToString(barrier));

            accesses.emplace_back(dstAccess.Stage, dstAccess.Access, dstAccess.Layout, intersection);

            {
                vk::ImageSubresourceRange range = srcAccess.Range;
                range.aspectMask &= ~dstAccess.Range.aspectMask;
                if (range.aspectMask != vk::ImageAspectFlags())
                    accesses.emplace_back(srcAccess.Stage, srcAccess.Access, srcAccess.Layout, range);
            }

            auto dstMask = dstAccess.Range.aspectMask;

            {
                const auto &range = srcAccess.Range;
                uint32_t layer = range.baseArrayLayer;
                uint32_t layerCount = intersection.baseArrayLayer - layer;
                if (layerCount > 0)
                    accesses.emplace_back(
                        dstAccess.Stage, dstAccess.Access, dstAccess.Layout,
                        vk::ImageSubresourceRange(
                            dstMask, range.baseMipLevel, range.levelCount, layer, layerCount
                        )
                    );
            }

            {
                const auto &range = srcAccess.Range;
                uint32_t layer = getMaxLayer(intersection) + 1;
                uint32_t layerCount = getMaxLayer(range) - layer + 1;
                if (layerCount > 0)
                    accesses.emplace_back(
                        dstAccess.Stage, dstAccess.Access, dstAccess.Layout,
                        vk::ImageSubresourceRange(
                            dstMask, range.baseMipLevel, range.levelCount, layer, layerCount
                        )
                    );
            }

            {
                const auto &range = srcAccess.Range;
                uint32_t mip = range.baseMipLevel;
                uint32_t mipCount = intersection.baseMipLevel - mip;
                if (mipCount > 0)
                    accesses.emplace_back(
                        dstAccess.Stage, dstAccess.Access, dstAccess.Layout,
                        vk::ImageSubresourceRange(
                            dstMask, mip, mipCount, range.baseArrayLayer, range.layerCount
                        )
                    );
            }

            {
                const auto &range = srcAccess.Range;
                uint32_t mip = getMaxMip(intersection) + 1;
                uint32_t mipCount = getMaxMip(range) - mip + 1;
                if (mipCount > 0)
                    accesses.emplace_back(
                        dstAccess.Stage, dstAccess.Access, dstAccess.Layout,
                        vk::ImageSubresourceRange(
                            dstMask, mip, mipCount, range.baseArrayLayer, range.layerCount
                        )
                    );
            }
        }

        ImageResources[name] = accesses;
    }

    vk::AccessFlags2 GetAccessFlags(const auto &binding)
    {
        vk::AccessFlags2 access;
        if (binding.Read)
            access |= vk::AccessFlagBits2::eShaderRead;
        if (binding.Write)
            access |= vk::AccessFlagBits2::eShaderWrite;
        return access;
    }

    void SynchronizePass(auto &pass, auto &execution, PipelineLibrary *pipelineLibrary)
    {
        const auto &spec = pass.Spec;
        using S = std::remove_cvref_t<decltype(spec)>;

        if constexpr (HasImageResource<S> && HasRanges<S>)
        {
            for (const auto &range : spec.Ranges)
                AddImageBarrier(
                    spec.ImageResource,
                    ImageAccess(
                        vk::PipelineStageFlagBits2::eClear, vk::AccessFlagBits2::eTransferWrite,
                        vk::ImageLayout::eTransferDstOptimal, range
                    ),
                    execution
                );
        }

        if constexpr (HasRegions<S>)
        {
            auto toRange = [](const vk::ImageSubresourceLayers &layer) {
                return vk::ImageSubresourceRange(
                    layer.aspectMask, layer.mipLevel, 1, layer.baseArrayLayer, layer.layerCount
                );
            };

            for (const auto &region : spec.Regions)
            {
                if constexpr (HasSrcImageResource<S>)
                {
                    AddImageBarrier(
                        spec.SrcImageResource,
                        ImageAccess(
                            vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead,
                            vk::ImageLayout::eTransferSrcOptimal, toRange(region.srcSubresource)
                        ),
                        execution
                    );
                }

                if constexpr (HasDstImageResource<S>)
                {
                    AddImageBarrier(
                        spec.DstImageResource,
                        ImageAccess(
                            vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite,
                            vk::ImageLayout::eTransferDstOptimal, toRange(region.dstSubresource)
                        ),
                        execution
                    );
                }
            }
        }

        if constexpr (HasBindings<S>)
        {
            const auto &pipeline = pipelineLibrary->GetPipeline(spec.Pipeline);

            auto toPipelineStage = [](vk::ShaderStageFlags shaders) {
                vk::PipelineStageFlags2 stages;
                if (shaders & vk::ShaderStageFlagBits::eCompute)
                    stages |= vk::PipelineStageFlagBits2::eComputeShader;
                if (shaders & vk::ShaderStageFlagBits::eFragment)
                    stages |= vk::PipelineStageFlagBits2::eFragmentShader;
                if (shaders & vk::ShaderStageFlagBits::eVertex)
                    stages |= vk::PipelineStageFlagBits2::eVertexShader;
                if (shaders & vk::ShaderStageFlagBits::eGeometry)
                    stages |= vk::PipelineStageFlagBits2::eGeometryShader;
                assert(
                    (shaders & ~(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment |
                                 vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry)) ==
                    vk::ShaderStageFlags()
                );
                return stages;
            };

            for (const auto &binding : spec.BufferBindings)
            {
                const auto &bindingInfo = pipeline.DescriptorSetBuilder.GetBinding(binding.Binding);
                AddBufferBarrier(
                    binding.BufferResource,
                    BufferAccess(
                        toPipelineStage(bindingInfo.stageFlags), GetAccessFlags(binding), binding.Offset,
                        binding.Size
                    ),
                    execution
                );
            }

            for (const auto &binding : spec.ImageBindings)
            {
                vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal;
                if (binding.Write)
                    layout = vk::ImageLayout::eGeneral;

                const auto &bindingInfo = pipeline.DescriptorSetBuilder.GetBinding(binding.Binding);
                AddImageBarrier(
                    binding.ImageResource,
                    ImageAccess(
                        toPipelineStage(bindingInfo.stageFlags), GetAccessFlags(binding), layout,
                        GetRange(binding.ImageResource)
                    ),
                    execution
                );
            }
        }

        if constexpr (HasColorAttachments<S>)
        {
            for (const auto &attachment : spec.ColorAttachments)
            {
                const auto layout = vk::ImageLayout::eColorAttachmentOptimal;

                vk::AccessFlags2 access;
                if (attachment.LoadOp == vk::AttachmentLoadOp::eLoad)
                    access |= vk::AccessFlagBits2::eColorAttachmentRead;
                if (attachment.LoadOp == vk::AttachmentLoadOp::eClear ||
                    attachment.LoadOp == vk::AttachmentLoadOp::eDontCare)
                    access |= vk::AccessFlagBits2::eColorAttachmentWrite;
                if (attachment.StoreOp == vk::AttachmentStoreOp::eStore ||
                    attachment.StoreOp == vk::AttachmentStoreOp::eDontCare)
                    access |= vk::AccessFlagBits2::eColorAttachmentWrite;
                AddImageBarrier(
                    attachment.ImageResource,
                    ImageAccess(
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput, access, layout,
                        GetRange(attachment.ImageResource)
                    ),
                    execution
                );

                if (attachment.ResolveImageResource.has_value())
                {
                    AddImageBarrier(
                        attachment.ResolveImageResource.value(),
                        ImageAccess(
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput, access, layout,
                            GetRange(attachment.ResolveImageResource.value())
                        ),
                        execution
                    );
                }

                vk::RenderingAttachmentInfo rendering(nullptr, layout);
                rendering.setLoadOp(attachment.LoadOp);
                rendering.setStoreOp(attachment.StoreOp);
                rendering.setClearValue(attachment.ClearValue);
                rendering.setImageLayout(layout);
                rendering.setResolveImageLayout(layout);
                rendering.setResolveMode(attachment.ResolveMode);
                pass.ColorAttachments.push_back(rendering);

                logger::trace(
                    "Attaching image resource `{}` as color attachment to pass `{}` with (load op: {}, store "
                    "op: {})",
                    execution.Name, attachment.ImageResource, vk::to_string(attachment.LoadOp),
                    vk::to_string(attachment.StoreOp)
                );
            }
        }

        if constexpr (HasDepthAttachment<S>)
        {
            if (spec.DepthAttachment.has_value())
            {
                const auto &attachment = spec.DepthAttachment.value();

                const auto layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

                vk::AccessFlags2 access;
                if (attachment.LoadOp == vk::AttachmentLoadOp::eLoad)
                    access |= vk::AccessFlagBits2::eDepthStencilAttachmentRead;
                if (attachment.LoadOp == vk::AttachmentLoadOp::eClear ||
                    attachment.LoadOp == vk::AttachmentLoadOp::eDontCare)
                    access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
                if (attachment.StoreOp == vk::AttachmentStoreOp::eStore ||
                    attachment.StoreOp == vk::AttachmentStoreOp::eDontCare)
                    access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
                AddImageBarrier(
                    attachment.ImageResource,
                    ImageAccess(
                        vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                            vk::PipelineStageFlagBits2::eLateFragmentTests,
                        access, layout, GetRange(attachment.ImageResource)
                    ),
                    execution
                );
                if (attachment.ResolveImageResource.has_value())
                {
                    AddImageBarrier(
                        attachment.ResolveImageResource.value(),
                        ImageAccess(
                            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests,
                            access, layout, GetRange(attachment.ResolveImageResource.value())
                        ),
                        execution
                    );
                }

                vk::RenderingAttachmentInfo rendering(nullptr, layout);
                rendering.setLoadOp(attachment.LoadOp);
                rendering.setStoreOp(attachment.StoreOp);
                rendering.setClearValue(attachment.ClearValue);
                rendering.setImageLayout(layout);
                rendering.setResolveImageLayout(layout);
                rendering.setResolveMode(attachment.ResolveMode);
                pass.DepthAttachment = rendering;

                logger::trace(
                    "Attaching image resource `{}` as depth attachment to pass `{}` with (load op: {}, store "
                    "op: {})",
                    execution.Name, attachment.ImageResource, vk::to_string(attachment.LoadOp),
                    vk::to_string(attachment.StoreOp)
                );
            }
        }

        if constexpr (HasStencilAttachment<S>)
        {
            if (spec.StencilAttachment.has_value())
            {
                const auto &attachment = spec.StencilAttachment.value();

                const auto layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

                vk::AccessFlags2 access;
                if (attachment.LoadOp == vk::AttachmentLoadOp::eLoad)
                    access |= vk::AccessFlagBits2::eDepthStencilAttachmentRead;
                if (attachment.LoadOp == vk::AttachmentLoadOp::eClear ||
                    attachment.LoadOp == vk::AttachmentLoadOp::eDontCare)
                    access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
                if (attachment.StoreOp == vk::AttachmentStoreOp::eStore ||
                    attachment.StoreOp == vk::AttachmentStoreOp::eDontCare)
                    access |= vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
                AddImageBarrier(
                    attachment.ImageResource,
                    ImageAccess(
                        vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                            vk::PipelineStageFlagBits2::eLateFragmentTests,
                        access, layout, GetRange(attachment.ImageResource)
                    ),
                    execution
                );
                if (attachment.ResolveImageResource.has_value())
                {
                    AddImageBarrier(
                        attachment.ResolveImageResource.value(),
                        ImageAccess(
                            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests,
                            access, layout, GetRange(attachment.ResolveImageResource.value())
                        ),
                        execution
                    );
                }

                vk::RenderingAttachmentInfo rendering(nullptr, layout);
                rendering.setLoadOp(attachment.LoadOp);
                rendering.setStoreOp(attachment.StoreOp);
                rendering.setClearValue(attachment.ClearValue);
                rendering.setImageLayout(layout);
                rendering.setResolveImageLayout(layout);
                rendering.setResolveMode(attachment.ResolveMode);
                pass.StencilAttachment = rendering;

                logger::trace(
                    "Attaching image resource `{}` as stencil attachment to pass `{}` with (load op: {}, "
                    "store "
                    "op: {})",
                    execution.Name, attachment.ImageResource, vk::to_string(attachment.LoadOp),
                    vk::to_string(attachment.StoreOp)
                );
            }
        }

        if constexpr (HasVertexBuffers<S>)
        {
            for (const auto &buffer : spec.VertexBuffers.VertexBuffers)
                AddBufferBarrier(
                    buffer.BufferResource,
                    BufferAccess(
                        vk::PipelineStageFlagBits2::eVertexAttributeInput,
                        vk::AccessFlagBits2::eVertexAttributeRead, buffer.Offset
                    ),
                    execution
                );
        }

        if constexpr (HasIndexBuffer<S>)
        {
            AddBufferBarrier(
                spec.IndexBuffer.BufferResource,
                BufferAccess(
                    vk::PipelineStageFlagBits2::eIndexInput, vk::AccessFlagBits2::eIndexRead,
                    spec.IndexBuffer.Offset
                ),
                execution
            );
        }

        if constexpr (HasIndirectBuffer<S>)
        {
            AddBufferBarrier(
                spec.IndirectBufferResource,
                BufferAccess(
                    vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead
                ),
                execution
            );
        }
    }
};

template<std::ranges::input_range R>
void AssertBuffer(R buffers, const std::string &name, const std::string &passName)
{
    if (std::ranges::contains(buffers, name))
        return;

    throw std::runtime_error(
        std::format("Buffer `{}` is required by pass `{}` but was not declared", name, passName)
    );
}

template<std::ranges::input_range R>
void AssertImage(R images, const std::string &name, const std::string &passName)
{
    if (name == FrameGraph::g_SwapchainImageResourceName)
        return;

    if (std::ranges::contains(images, name))
        return;

    throw std::runtime_error(
        std::format("Image `{}` is required by pass `{}` but was not declared", name, passName)
    );
}

template<std::ranges::input_range R1, std::ranges::input_range R2, typename P>
void ValidatePass(
    const P &pass, R1 buffers, R2 images, const std::string &passName, PipelineLibrary *pipelineLibrary
)
{
    const auto &spec = pass.Spec;
    using S = std::remove_cvref_t<decltype(spec)>;

    if constexpr (HasPipeline<S>)
    {
        const auto &pipeline = pipelineLibrary->GetPipelineInstance(spec.Pipeline);
        const auto &info = pipelineLibrary->GetPipelineInstanceInfo(spec.Pipeline);
        if (!pipeline.IsValid())
            throw std::runtime_error(
                std::format(
                    "Pipeline instance `{}` used in pass `{}` was is not compiled", info.Name, passName
                )
            );
    }

    if constexpr (HasImageResource<S>)
    {
        AssertImage(images, spec.ImageResource, passName);
    }

    if constexpr (HasSrcImageResource<S>)
    {
        AssertImage(images, spec.SrcImageResource, passName);
    }

    if constexpr (HasDstImageResource<S>)
    {
        AssertImage(images, spec.DstImageResource, passName);
    }

    if constexpr (HasBindings<S>)
    {
        const auto &pipeline = pipelineLibrary->GetPipeline(spec.Pipeline);
        for (const auto &binding : spec.BufferBindings)
        {
            if (!pipeline.DescriptorSetBuilder.HasBinding(binding.Binding))
                throw std::runtime_error(
                    std::format(
                        "Buffer `{}` is bound to pass `{}` at binding {} but pipeline `{}` used by the pass "
                        "does not contain the binding",
                        binding.BufferResource, passName, binding.Binding, pipeline.Name
                    )
                );

            AssertBuffer(buffers, binding.BufferResource, passName);
        }

        for (const auto &binding : spec.ImageBindings)
        {
            if (!pipeline.DescriptorSetBuilder.HasBinding(binding.Binding))
                throw std::runtime_error(
                    std::format(
                        "Image `{}` is bound to pass `{}` at binding {} but pipeline `{}` used by the pass "
                        "does not contain the binding",
                        binding.ImageResource, passName, binding.Binding, pipeline.Name
                    )
                );

            AssertImage(images, binding.ImageResource, passName);
        }

        for (int32_t binding = 0; binding <= pipeline.DescriptorSetBuilder.GetMaxBinding(); binding++)
        {
            if (!pipeline.DescriptorSetBuilder.HasBinding(binding))
                continue;

            auto proj = [](const auto &bnd) { return bnd.Binding; };
            if (!std::ranges::contains(spec.BufferBindings, binding, proj) &&
                !std::ranges::contains(spec.ImageBindings, binding, proj))
                throw std::runtime_error(
                    std::format(
                        "Pipeline `{}` expects a binding at {} but no resource was bound", pipeline.Name,
                        binding
                    )
                );
        }
    }

    if constexpr (HasColorAttachments<S>)
    {
        for (const auto &attachment : spec.ColorAttachments)
            AssertImage(images, attachment.ImageResource, passName);
    }

    if constexpr (HasDepthAttachment<S>)
    {
        if (spec.DepthAttachment.has_value())
        {
            const auto &attachment = spec.DepthAttachment.value();
            AssertImage(images, attachment.ImageResource, passName);
        }
    }

    if constexpr (HasStencilAttachment<S>)
    {
        if (spec.StencilAttachment.has_value())
        {
            const auto &attachment = spec.StencilAttachment.value();
            AssertImage(images, attachment.ImageResource, passName);
        }
    }

    if constexpr (HasVertexBuffers<S>)
    {
        for (const auto &buffer : spec.VertexBuffers.VertexBuffers)
            AssertBuffer(buffers, buffer.BufferResource, passName);
    }

    if constexpr (HasIndexBuffer<S>)
    {
        AssertBuffer(buffers, spec.IndexBuffer.BufferResource, passName);
    }

    if constexpr (HasIndirectBuffer<S>)
    {
        AssertBuffer(buffers, spec.IndirectBufferResource, passName);
    }
}

std::unique_ptr<FrameGraph> FrameGraphBuilder::CreateUnique(
    PipelineLibrary *pipelineLibrary, ResourceAllocator *resourceAllocator
)
{
    FrameGraphAccesses frameGraphAccess(m_Spec.Buffers, m_Spec.Images);

    for (const auto &execution : m_Spec.PassExecutions)
    {
        auto validatePass = [](auto &pass, auto buffers, auto images, const std::string &passName,
                               PipelineLibrary *pipelineLibrary) {
            ValidatePass(pass, buffers, images, passName, pipelineLibrary);
        };

        Dispatch(
            execution, validatePass, m_Spec.Buffers | std::views::keys, m_Spec.Images | std::views::keys,
            execution.Name, pipelineLibrary
        );
    }

    for (auto &execution : m_Spec.PassExecutions)
    {
        auto synchronizePass = [](auto &pass, FrameGraphAccesses *accesses, auto &execution,
                                  PipelineLibrary *pipelineLibrary) {
            accesses->SynchronizePass(pass, execution, pipelineLibrary);
        };

        Dispatch(execution, synchronizePass, &frameGraphAccess, execution, pipelineLibrary);
    }

    for (const auto &[name, srcAccess] : frameGraphAccess.BufferResources)
    {
        if (m_Spec.Buffers.at(name).Type != ResourceType::Temporal)
            continue;

        vk::BufferMemoryBarrier2 barrier(
            srcAccess.Stage, srcAccess.Access, vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::AccessFlagBits2::eNone, vk::QueueFamilyIgnored,
            vk::QueueFamilyIgnored, nullptr, srcAccess.Offset, srcAccess.Size
        );

        m_Spec.BottomOfPipeBufferNames.push_back(name);
        m_Spec.BottomOfPipeBufferBarriers.push_back(barrier);
        logger::trace("Inserting Temporal Barrier: {}", ToString(barrier));
    }

    for (const auto &[name, accesses] : frameGraphAccess.ImageResources)
    {
        if (name == FrameGraph::g_SwapchainImageResourceName ||
            m_Spec.Images.at(name).Type != ResourceType::Temporal)
            continue;

        for (const auto& srcAccess : accesses)
        {
            vk::ImageLayout dstLayout = frameGraphAccess.InitialImageLayouts.at(name);

            vk::ImageMemoryBarrier2 barrier(
                srcAccess.Stage, srcAccess.Access, vk::PipelineStageFlagBits2::eBottomOfPipe,
                vk::AccessFlagBits2::eNone, srcAccess.Layout, dstLayout,
                vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, nullptr, srcAccess.Range
            );

            m_Spec.BottomOfPipeImageNames.push_back(name);
            m_Spec.BottomOfPipeImageBarriers.push_back(barrier);
            logger::trace("Inserting Temporal Barrier: {}", ToString(barrier));
        }
    }

    for (const auto &srcAccess : frameGraphAccess.ImageResources.at(FrameGraph::g_SwapchainImageResourceName))
    {
        vk::ImageMemoryBarrier2 barrier(
            srcAccess.Stage, srcAccess.Access, vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::AccessFlagBits2::eNone, srcAccess.Layout, vk::ImageLayout::ePresentSrcKHR,
            vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, nullptr, srcAccess.Range
        );

        m_Spec.BottomOfPipeImageNames.push_back(FrameGraph::g_SwapchainImageResourceName);
        m_Spec.BottomOfPipeImageBarriers.push_back(barrier);
        logger::trace("Inserting Present Barrier: {}", ToString(barrier));
    }

    return std::make_unique<FrameGraph>(pipelineLibrary, resourceAllocator, std::move(m_Spec));
}

template<typename F, typename... Args>
void FrameGraphBuilder::Dispatch(const PassExecution &execution, F &&func, Args &&...args)
{
    switch (execution.Type)
    {
    case PassType::Clear:
        func(m_Spec.ClearPasses.at(execution.Name), args...);
        break;
    case PassType::Blit:
        func(m_Spec.BlitPasses.at(execution.Name), args...);
        break;
    case PassType::Compute:
        func(m_Spec.ComputePasses.at(execution.Name), args...);
        break;
    case PassType::Graphics:
        func(m_Spec.GraphicsPasses.at(execution.Name), args...);
        break;
    case PassType::IndexedGraphics:
        func(m_Spec.IndexedGraphicsPasses.at(execution.Name), args...);
        break;
    case PassType::IndexedIndirectGraphics:
        func(m_Spec.IndexedIndirectGraphicsPasses.at(execution.Name), args...);
        break;
    case PassType::CustomGraphics:
        func(m_Spec.CustomGraphicsPasses.at(execution.Name), args...);
        break;
    }
}

}
