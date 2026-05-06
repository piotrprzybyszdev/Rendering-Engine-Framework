#include <ranges>

#include "Core/Core.h"

#include "Vulkan/Application.h"

#include "FrameGraph.h"

namespace ref::vulkan
{

FrameGraph::FrameGraph(
    PipelineLibrary *pipelineLibrary, ResourceAllocator *resourceAllocator, FrameGraphSpec spec
)
    : m_PipelineLibrary(pipelineLibrary), m_ResourceAllocator(resourceAllocator),
      m_BufferResources(std::move(spec.Buffers)), m_ImageResources(std::move(spec.Images)),
      m_PassExecutions(std::move(spec.PassExecutions)), m_ImageViewResources(std::move(spec.ImageViews)),
      m_BottomOfPipeBufferNames(std::move(spec.BottomOfPipeBufferNames)),
      m_BottomOfPipeBufferBarriers(std::move(spec.BottomOfPipeBufferBarriers)),
      m_BottomOfPipeImageNames(std::move(spec.BottomOfPipeImageNames)),
      m_BottomOfPipeImageBarriers(std::move(spec.BottomOfPipeImageBarriers))
{
    for (auto &[name, buffer] : m_BufferResources)
    {
        if (buffer.IsBuffered)
            continue;

        buffer.Resources.push_back(AddAndCreateBuffer(name, buffer));
    }

    for (auto &[name, image] : m_ImageResources)
    {
        if (image.IsBuffered)
            continue;

        image.Resources.push_back(AddAndCreateImage(name, image));
    }

    for (auto& [name, imageView] : m_ImageViewResources)
    {
        const auto &image = m_ImageResources.at(imageView.Image);
        if (image.IsBuffered)
            continue;

        imageView.Resources.push_back(AddAndCreateImageView(name, imageView, image.Resources.back()));
    }

    for (auto &[name, pass] : spec.ClearPasses)
        m_ClearPasses[name] = ClearPass(std::move(pass.Spec));

    for (auto &[name, pass] : spec.BlitPasses)
        m_BlitPasses[name] = BlitPass(std::move(pass.Spec));

    for (auto &[name, pass] : spec.ComputePasses)
        m_ComputePasses[name] = ComputePass(std::move(pass.Spec), nullptr);

    auto setupGraphicsPass = []<typename P>(const auto &info) {
        P pass(
            std::move(info.Spec), std::move(info.ColorAttachments), std::move(info.DepthAttachment),
            std::move(info.StencilAttachment)
        );

        const auto &spec = pass.Spec;
        using S = std::remove_cvref_t<decltype(spec)>;

        if constexpr (HasVertexBuffers<S>)
        {
            for (const auto &buffer : spec.VertexBuffers.VertexBuffers)
                pass.VertexBufferOffsets.push_back(buffer.Offset);
            pass.VertexBuffers.resize(pass.VertexBufferOffsets.size());
        }

        return pass;
    };

    for (auto &[name, info] : spec.GraphicsPasses)
        m_GraphicsPasses[name] = setupGraphicsPass.template operator()<GraphicsPass>(info);

    for (auto &[name, info] : spec.IndexedGraphicsPasses)
        m_IndexedGraphicsPasses[name] = setupGraphicsPass.template operator()<IndexedGraphicsPass>(info);

    for (auto &[name, info] : spec.IndexedIndirectGraphicsPasses)
        m_IndexedIndirectGraphicsPasses[name] =
            setupGraphicsPass.template operator()<IndexedIndirectGraphicsPass>(info);

    for (auto &[name, info] : spec.CustomGraphicsPasses)
        m_CustomGraphicsPasses[name] = setupGraphicsPass.template operator()<CustomGraphicsPass>(info);
}

std::span<const BufferResourceId> FrameGraph::GetBuffer(const std::string &name)
{
    return m_BufferResources.at(name).Resources;
}

std::span<const ImageResourceId> FrameGraph::GetImage(const std::string &name)
{
    return m_ImageResources.at(name).Resources;
}

std::span<const ImageViewResourceId> FrameGraph::GetImageView(const std::string &name)
{
    return m_ImageViewResources.at(name).Resources;
}

Buffer &FrameGraph::ModifyBuffer(const std::string &name)
{
    return m_BufferResources.at(name);
}

Image &FrameGraph::ModifyImage(const std::string &name)
{
    return m_ImageResources.at(name);
}

ImageView& FrameGraph::ModifyImageView(const std::string& name)
{
    return m_ImageViewResources.at(name);
}

void FrameGraph::UpdateBuffer(const std::string &name)
{
    auto &buffer = m_BufferResources.at(name);
    for (auto id : buffer.Resources)
        m_ResourceAllocator->AllocateResource(id, buffer.Info, buffer.IsDevice);
    m_BuffersToUpdate.push_back(name);
}

void FrameGraph::UpdateImage(const std::string &name)
{
    if (name != g_SwapchainImageResourceName)
    {
        auto &image = m_ImageResources.at(name);
        for (auto id : image.Resources)
            m_ResourceAllocator->AllocateResource(id, image.Info, image.IsDevice);
    }
    m_ImagesToUpdate.push_back(name);
}

void FrameGraph::UpdateImageView(const std::string &name)
{
    if (name != g_SwapchainImageViewResourceName)
    {
        auto &view = m_ImageViewResources.at(name);
        const auto &image = m_ImageResources.at(view.Image);
        for (auto [viewId, imageId] : std::views::zip(view.Resources, image.Resources))
        {
            vk::Image handle = m_ResourceAllocator->GetImageResource(imageId).Handle;
            view.ViewInfo.setImage(handle);
            m_ResourceAllocator->AllocateResource(viewId, view.ViewInfo);
        }
    }
    m_ImageViewsToUpdate.push_back(name);
}

BufferResourceId FrameGraph::GetCurrentBuffer(const std::string &name)
{
    return GetBufferResourceId(name, m_FrameInfo.FrameInFlightIndex);
}

ImageResourceId FrameGraph::GetCurrentImage(const std::string &name)
{
    return GetImageResourceId(name, m_FrameInfo.FrameInFlightIndex);
}

ImageViewResourceId FrameGraph::GetCurrentImageView(const std::string &name)
{
    return GetImageViewResourceId(name, m_FrameInfo.FrameInFlightIndex);
}

ClearPassDynamicConfig FrameGraph::GetClearPassDynamicConfig(const std::string &name)
{
    return ClearPassDynamicConfig(m_ClearPasses.at(name).Spec);
}

BlitPassDynamicConfig FrameGraph::GetBlitPassDynamicConfig(const std::string &name)
{
    return BlitPassDynamicConfig(m_BlitPasses.at(name).Spec);
}

ComputePassDynamicConfig FrameGraph::GetComputePassDynamicConfig(const std::string &name)
{
    return ComputePassDynamicConfig(m_ComputePasses.at(name).Spec);
}

GraphicsPassDynamicConfig FrameGraph::GetGraphicsPassDynamicConfig(const std::string &name)
{
    return GraphicsPassDynamicConfig(m_GraphicsPasses.at(name).Spec);
}

IndexedGraphicsPassDynamicConfig FrameGraph::GetIndexedGraphicsPassDynamicConfig(const std::string &name)
{
    return IndexedGraphicsPassDynamicConfig(m_IndexedGraphicsPasses.at(name).Spec);
}

IndexedIndirectGraphicsPassDynamicConfig FrameGraph::GetIndexedIndirectGraphicsPassDynamicConfig(
    const std::string &name
)
{
    return IndexedIndirectGraphicsPassDynamicConfig(m_IndexedIndirectGraphicsPasses.at(name).Spec);
}

CustomGraphicsPassDynamicConfig FrameGraph::GetCustomGraphicsPassDynamicConfig(const std::string &name)
{
    return CustomGraphicsPassDynamicConfig(m_CustomGraphicsPasses.at(name).Spec);
}

void FrameGraph::SetFrame(const FrameInfo &frameInfo)
{
    m_FrameInfo = frameInfo;

    bool isFrameCountChanged = false;

    while (m_RenderingResourcesCount < m_FrameInfo.FrameInFlightCount)
    {
        for (auto &[name, buffer] : m_BufferResources)
        {
            if (!buffer.IsBuffered)
                continue;

            buffer.Resources.push_back(AddAndCreateBuffer(
                std::format("{} (frame in flight {})", name, m_RenderingResourcesCount), buffer
            ));
        }

        for (auto &[name, image] : m_ImageResources)
        {
            if (!image.IsBuffered)
                continue;

            image.Resources.push_back(AddAndCreateImage(
                std::format("{} (frame in flight {})", name, m_RenderingResourcesCount), image
            ));
        }

        for (auto& [name, imageView] : m_ImageViewResources)
        {
            const auto &image = m_ImageResources.at(imageView.Image);
            if (!image.IsBuffered)
                continue;

            imageView.Resources.push_back(AddAndCreateImageView(
                std::format("{} (frame in flight {})", name, m_RenderingResourcesCount), imageView,
                image.Resources.back()
            ));
        }

        m_RenderingResourcesCount++;
        isFrameCountChanged = true;
    }

    if (isFrameCountChanged == false && m_BuffersToUpdate.empty() && m_ImagesToUpdate.empty() &&
        m_ImageViewsToUpdate.empty())
        return;

    auto updateDescriptors = [](auto &pass, FrameGraph *frameGraph, const PassExecution &execution,
                                bool isFrameCountChanged) {
        if constexpr (HasBindings<std::remove_cvref_t<decltype(pass.Spec)>>)
        {
            frameGraph->UpdateDescriptors(pass, execution, isFrameCountChanged);
        }
    };

    for (const auto &execution : m_PassExecutions)
        Dispatch(execution, updateDescriptors, this, execution, isFrameCountChanged);

    m_BuffersToUpdate.clear();
    m_ImagesToUpdate.clear();
    m_ImageViewsToUpdate.clear();
}

template<typename P>
void FrameGraph::UpdateDescriptors(P &pass, const PassExecution &execution, bool isFrameCountChanged)
{
    if (isFrameCountChanged)
    {
        pass.Set = m_PipelineLibrary->GetPipeline(pass.Spec.Pipeline)
                       .DescriptorSetBuilder.CreateSetUnique(m_RenderingResourcesCount);
        logger::trace(
            "Recreating descriptor sets for pass {} (new set count: {})", execution.Name,
            m_RenderingResourcesCount
        );
    }

    for (const auto &binding : pass.Spec.BufferBindings)
    {
        if (isFrameCountChanged == false &&
            std::ranges::contains(m_BuffersToUpdate, binding.BufferResource) == false)
            continue;

        const uint64_t end = binding.Size == vk::WholeSize ? vk::WholeSize : binding.Offset + binding.Size;
        for (uint32_t frameInFlight = 0; frameInFlight < m_RenderingResourcesCount; frameInFlight++)
        {
            auto buffer = GetBufferResource(binding.BufferResource, frameInFlight);
            assert(end == vk::WholeSize || buffer.second >= end);
            pass.Set->UpdateBuffer(
                binding.Binding, frameInFlight, buffer.first, binding.Size, binding.Offset
            );
        }

        logger::trace(
            "Pass `{}` binds buffer resource `{}` (range: [{}, {}]) at set 0 and binding {}", execution.Name,
            binding.BufferResource, binding.Offset, end, binding.Binding
        );
    }

    for (const auto &binding : pass.Spec.ImageBindings)
    {
        if (isFrameCountChanged == false &&
            std::ranges::contains(m_ImageViewsToUpdate, binding.ImageViewResource) == false)
            continue;

        vk::ImageLayout layout =
            binding.Write ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
        for (uint32_t frameInFlight = 0; frameInFlight < m_RenderingResourcesCount; frameInFlight++)
        {
            vk::ImageView view = GetImageViewHandle(binding.ImageViewResource, frameInFlight);
            pass.Set->UpdateImage(binding.Binding, frameInFlight, view, binding.Sampler, layout);
        }

        logger::trace(
            "Pass `{}` binds image view resource `{}` at set 0 and binding {}", execution.Name,
            binding.ImageViewResource, binding.Binding
        );
    }
}

template<typename P> void FrameGraph::SetupPass(P &pass, PassExecution &execution)
{
    const auto &spec = pass.Spec;
    using S = std::remove_cvref_t<decltype(spec)>;

    for (auto &&[resource, barrier] :
         std::views::zip(execution.BufferBarriers.Resources, execution.BufferBarriers.Barriers))
        std::tie(barrier.buffer, barrier.size) = GetBufferResource(resource);

    for (auto &&[resource, barrier] :
         std::views::zip(execution.ImageBarriers.Resources, execution.ImageBarriers.Barriers))
        barrier.image = GetImageHandle(resource);

    auto setupAttachment = [this](const auto &attachment, auto &info) {
        info.imageView = this->GetImageViewHandle(attachment.ImageViewResource);
        if (attachment.ResolveImageViewResource.has_value())
            info.resolveImageView = this->GetImageViewHandle(attachment.ResolveImageViewResource.value());
    };

    if constexpr (HasColorAttachments<S>)
    {
        for (auto &&[attachment, info] : std::views::zip(spec.ColorAttachments, pass.ColorAttachments))
            setupAttachment(attachment, info);
    }

    if constexpr (HasDepthAttachment<S>)
    {
        if (pass.Spec.DepthAttachment.has_value())
            setupAttachment(pass.Spec.DepthAttachment.value(), pass.DepthAttachment.value());
    }

    if constexpr (HasStencilAttachment<S>)
    {
        if (pass.Spec.StencilAttachment.has_value())
            setupAttachment(pass.Spec.StencilAttachment.value(), pass.StencilAttachment.value());
    }

    if constexpr (HasBindings<S>)
    {
        pass.Set->FlushUpdate(m_FrameInfo.FrameInFlightIndex);
    }

    if constexpr (HasVertexBuffers<S>)
    {
        for (auto &&[buffer, bufferSpec] :
             std::views::zip(pass.VertexBuffers, pass.Spec.VertexBuffers.VertexBuffers))
            buffer = GetBufferResource(bufferSpec.BufferResource).first;
    }
}

template<typename P> void FrameGraph::ExecutePass(const P &pass, vk::CommandBuffer commandBuffer)
{
    const auto &spec = pass.Spec;
    using S = std::remove_cvref_t<decltype(spec)>;

    if constexpr (HasBindings<S>)
    {
        const auto &pipeline = m_PipelineLibrary->GetPipeline(spec.Pipeline);
        const auto &pipelineInstance = m_PipelineLibrary->GetPipelineInstance(spec.Pipeline);
        commandBuffer.bindPipeline(P::BindPoint, pipelineInstance.Handle);
        commandBuffer.bindDescriptorSets(
            P::BindPoint, pipeline.Layout, 0, pass.Set->GetSet(m_FrameInfo.FrameInFlightIndex), {}
        );
    }

    if constexpr (HasScissors<S>)
    {
        commandBuffer.setScissor(0, spec.Scissors);
    }

    if constexpr (HasViewports<S>)
    {
        commandBuffer.setViewport(0, spec.Viewports);
    }

    if constexpr (HasVertexBuffers<S>)
    {
        if (!pass.VertexBuffers.empty())
            commandBuffer.bindVertexBuffers(
                spec.VertexBuffers.FirstBinding, pass.VertexBuffers, pass.VertexBufferOffsets
            );
    }

    if constexpr (HasIndexBuffer<S>)
    {
        vk::Buffer indexBuffer = GetBufferResource(spec.IndexBuffer.BufferResource).first;
        commandBuffer.bindIndexBuffer(indexBuffer, spec.IndexBuffer.Offset, spec.IndexBuffer.IndexType);
    }

    if constexpr (HasColorAttachments<S> || HasDepthAttachment<S> || HasStencilAttachment<S>)
    {
        std::span<const vk::RenderingAttachmentInfo> colorAttachments;
        const vk::RenderingAttachmentInfo *depthAttachment = nullptr;
        const vk::RenderingAttachmentInfo *stencilAttachment = nullptr;

        if constexpr (HasColorAttachments<S>)
        {
            colorAttachments = pass.ColorAttachments;
        }

        if constexpr (HasDepthAttachment<S>)
        {
            if (pass.DepthAttachment.has_value())
                depthAttachment = &pass.DepthAttachment.value();
        }

        if constexpr (HasStencilAttachment<S>)
        {
            if (pass.StencilAttachment.has_value())
                stencilAttachment = &pass.StencilAttachment.value();
        }

        commandBuffer.beginRendering(
            vk::RenderingInfo(
                vk::RenderingFlags(), pass.Spec.RenderArea, 1, 0, pass.ColorAttachments, depthAttachment,
                stencilAttachment
            )
        );
    }

    Execute(commandBuffer, pass);

    if constexpr (HasColorAttachments<S> || HasDepthAttachment<S> || HasStencilAttachment<S>)
    {
        commandBuffer.endRendering();
    }
}

void FrameGraph::OnRender(vk::CommandBuffer commandBuffer)
{
    [[maybe_unused]] auto getColor = [](PassType type) {
        auto fromHex = [](uint32_t hex) -> std::array<float, 4> {
            float r = ((hex >> 16) & 0xff) / 255.0f;
            float g = ((hex >> 8) & 0xff) / 255.0f;
            float b = (hex & 0xff) / 255.0f;
            return { r, g, b, 1.0f };
        };

        static std::array<std::array<float, 4>, 5> colors = {
            fromHex(0x50514F), fromHex(0xF25F5C), fromHex(0xFFE066), fromHex(0x247BA0), fromHex(0x70C1B3),
        };

        switch (type)
        {
        case PassType::Clear:
            return colors[0];
        case PassType::Blit:
            return colors[3];
        case PassType::Compute:
            return colors[2];
        case PassType::Graphics:
        case PassType::IndexedGraphics:
        case PassType::IndexedIndirectGraphics:
        case PassType::CustomGraphics:
            return colors[4];
        default:
            throw std::runtime_error("Unsupported PassType");
        }
    };

    commandBuffer.reset();
    commandBuffer.begin(vk::CommandBufferBeginInfo());

    for (auto &execution : m_PassExecutions)
    {
        auto setupPass = [](auto &pass, FrameGraph *frameGraph, PassExecution &execution) {
            frameGraph->SetupPass(pass, execution);
        };

        Dispatch(execution, setupPass, this, execution);
    }

    for (const auto &execution : m_PassExecutions)
    {
#if defined(REF_CONFIG_DEBUG) || defined(REF_CONFIG_TRACE) || defined(REF_CONFIG_PROFILE)

        commandBuffer.beginDebugUtilsLabelEXT(
            vk::DebugUtilsLabelEXT(execution.Name.c_str(), getColor(execution.Type)),
            *Application::GetInstance()->GetApplicationStateSpec().DispatchLoader
        );
#endif

        vk::DependencyInfo dependency;
        dependency.setBufferMemoryBarriers(execution.BufferBarriers.Barriers);
        dependency.setImageMemoryBarriers(execution.ImageBarriers.Barriers);
        commandBuffer.pipelineBarrier2(dependency);

        auto executePass = [](auto &pass, FrameGraph *frameGraph, vk::CommandBuffer commandBuffer) {
            frameGraph->ExecutePass(pass, commandBuffer);
        };

        Dispatch(execution, executePass, this, commandBuffer);
#if defined(REF_CONFIG_DEBUG) || defined(REF_CONFIG_TRACE) || defined(REF_CONFIG_PROFILE)
        commandBuffer.endDebugUtilsLabelEXT(
            *Application::GetInstance()->GetApplicationStateSpec().DispatchLoader
        );
#endif
    }

    for (auto &&[name, barrier] : std::views::zip(m_BottomOfPipeBufferNames, m_BottomOfPipeBufferBarriers))
        barrier.setBuffer(GetBufferResource(name).first);
    for (auto &&[name, barrier] : std::views::zip(m_BottomOfPipeImageNames, m_BottomOfPipeImageBarriers))
        barrier.setImage(GetImageHandle(name));

    vk::DependencyInfo dependency;
    dependency.setBufferMemoryBarriers(m_BottomOfPipeBufferBarriers);
    dependency.setImageMemoryBarriers(m_BottomOfPipeImageBarriers);
    commandBuffer.pipelineBarrier2(dependency);

    commandBuffer.end();
}

BufferResourceId FrameGraph::AddAndCreateBuffer(const std::string &name, const Buffer &buffer)
{
    const auto id = m_ResourceAllocator->AddBufferResource(name);
    m_ResourceAllocator->AllocateResource(id, buffer.Info, buffer.IsDevice);
    return id;
}

ImageResourceId FrameGraph::AddAndCreateImage(const std::string &name, Image &image)
{
    const auto id = m_ResourceAllocator->AddImageResource(name);
    m_ResourceAllocator->AllocateResource(id, image.Info, image.IsDevice);
    return id;
}

ImageViewResourceId FrameGraph::AddAndCreateImageView(
    const std::string &name, ImageView &view, ImageResourceId imageId
)
{
    const auto viewId = m_ResourceAllocator->AddImageViewResource(name);
    const auto &image = m_ImageResources.at(view.Image);
    assert(std::ranges::contains(image.Resources, imageId) == true);

    vk::Image handle = m_ResourceAllocator->GetImageResource(imageId).Handle;
    view.ViewInfo.setImage(handle);
    m_ResourceAllocator->AllocateResource(viewId, view.ViewInfo);

    return viewId;
}

std::pair<vk::Buffer, vk::DeviceSize> FrameGraph::GetBufferResource(const std::string &name)
{
    return GetBufferResource(name, m_FrameInfo.FrameInFlightIndex);
}

std::pair<vk::Buffer, vk::DeviceSize> FrameGraph::GetBufferResource(
    const std::string &name, uint32_t frameInFlight
)
{
    const auto bufferId = GetBufferResourceId(name, frameInFlight);
    const auto &buffer = m_ResourceAllocator->GetBufferResource(bufferId);
    return std::make_pair(buffer.Handle, buffer.Size);
}

vk::Image FrameGraph::GetImageHandle(const std::string &name)
{
    return GetImageHandle(name, m_FrameInfo.FrameInFlightIndex);
}

vk::Image FrameGraph::GetImageHandle(const std::string &name, uint32_t frameInFlight)
{
    if (name == g_SwapchainImageResourceName)
        return m_FrameInfo.Image;
    const auto imageId = GetImageResourceId(name, frameInFlight);
    return m_ResourceAllocator->GetImageResource(imageId).Handle;
}

vk::ImageView FrameGraph::GetImageViewHandle(const std::string &name)
{
    return GetImageViewHandle(name, m_FrameInfo.FrameInFlightIndex);
}

vk::ImageView FrameGraph::GetImageViewHandle(const std::string &name, uint32_t frameInFlight)
{
    if (name == g_SwapchainImageViewResourceName)
        return m_FrameInfo.ImageView;
    const auto imageViewId = GetImageViewResourceId(name, frameInFlight);
    return m_ResourceAllocator->GetImageViewResource(imageViewId).Handle;
}

template<typename P, typename D>
void FrameGraph::SetPushConstants(vk::CommandBuffer commandBuffer, const P &pass, const D &draw)
{
    const auto &spec = pass.Spec;
    const auto &pipeline = m_PipelineLibrary->GetPipeline(spec.Pipeline);

    if (!draw.PushConstantData.empty())
        for (const auto pc : pipeline.Reflection.PushConstantRanges)
            commandBuffer.pushConstants(
                pipeline.Layout, pc.stageFlags, pc.offset,
                static_cast<uint32_t>(draw.PushConstantData.size_bytes() - pc.offset),
                draw.PushConstantData.data() + pc.offset
            );
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const ClearPass &pass)
{
    vk::Image handle = GetImageHandle(pass.Spec.ImageResource);
    commandBuffer.clearColorImage(
        handle, vk::ImageLayout::eTransferDstOptimal, pass.Spec.ClearValue, pass.Spec.Ranges
    );
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const BlitPass &pass)
{
    vk::Image srcHandle = GetImageHandle(pass.Spec.SrcImageResource);
    vk::Image dstHandle = GetImageHandle(pass.Spec.DstImageResource);
    vk::BlitImageInfo2 blit(
        srcHandle, vk::ImageLayout::eTransferSrcOptimal, dstHandle, vk::ImageLayout::eTransferDstOptimal,
        pass.Spec.Regions, pass.Spec.Filter
    );
    commandBuffer.blitImage2(blit);
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const ComputePass &pass)
{
    for (const auto &dispatch : pass.Spec.Dispatches)
    {
        SetPushConstants(commandBuffer, pass, dispatch);
        commandBuffer.dispatch(
            dispatch.Command.GroupCountX, dispatch.Command.GroupCountY, dispatch.Command.GroupCountZ
        );
    }
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const GraphicsPass &pass)
{
    for (const auto &draw : pass.Spec.Draws)
    {
        SetPushConstants(commandBuffer, pass, draw);
        commandBuffer.draw(
            draw.Command.VertexCount, draw.Command.InstanceCount, draw.Command.FirstVertex,
            draw.Command.FirstInstance
        );
    }
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const IndexedGraphicsPass &pass)
{
    for (const auto &draw : pass.Spec.Draws)
    {
        SetPushConstants(commandBuffer, pass, draw);
        commandBuffer.drawIndexed(
            draw.Command.IndexCount, draw.Command.InstanceCount, draw.Command.FirstIndex,
            draw.Command.VertexOffset, draw.Command.FirstInstance
        );
    }
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const IndexedIndirectGraphicsPass &pass)
{
    for (const auto &draw : pass.Spec.Draws)
    {
        SetPushConstants(commandBuffer, pass, draw);
        commandBuffer.drawIndexedIndirect(
            GetBufferResource(pass.Spec.IndirectBufferResource).first, draw.Command.Offset,
            draw.Command.DrawCount, draw.Command.Stride
        );
    }
}

void FrameGraph::Execute(vk::CommandBuffer commandBuffer, const CustomGraphicsPass &pass)
{
    pass.Spec.OnRender(commandBuffer);
}

BufferResourceId FrameGraph::GetBufferResourceId(const std::string &name, uint32_t frameInFlight)
{
    const auto &buffer = m_BufferResources.at(name);
    if (buffer.IsBuffered)
        return buffer.Resources[frameInFlight];
    assert(buffer.Resources.size() == 1);
    return buffer.Resources.front();
}

ImageResourceId FrameGraph::GetImageResourceId(const std::string &name, uint32_t frameInFlight)
{
    const auto &image = m_ImageResources.at(name);
    if (image.IsBuffered)
        return image.Resources[frameInFlight];
    assert(image.Resources.size() == 1);
    return image.Resources.front();
}

ImageViewResourceId FrameGraph::GetImageViewResourceId(const std::string &name, uint32_t frameInFlight)
{
    const auto &imageView = m_ImageViewResources.at(name);
    const auto &image = m_ImageResources.at(imageView.Image);
    assert(imageView.Resources.size() == image.Resources.size());
    if (image.IsBuffered)
        return imageView.Resources[frameInFlight];
    assert(imageView.Resources.size() == 1);
    return imageView.Resources.front();
}

template<typename F, typename... Args>
void FrameGraph::Dispatch(const PassExecution &execution, F &&func, Args &&...args)
{
    switch (execution.Type)
    {
    case PassType::Clear:
        func(m_ClearPasses.at(execution.Name), args...);
        break;
    case PassType::Blit:
        func(m_BlitPasses.at(execution.Name), args...);
        break;
    case PassType::Compute:
        func(m_ComputePasses.at(execution.Name), args...);
        break;
    case PassType::Graphics:
        func(m_GraphicsPasses.at(execution.Name), args...);
        break;
    case PassType::IndexedGraphics:
        func(m_IndexedGraphicsPasses.at(execution.Name), args...);
        break;
    case PassType::IndexedIndirectGraphics:
        func(m_IndexedIndirectGraphicsPasses.at(execution.Name), args...);
        break;
    case PassType::CustomGraphics:
        func(m_CustomGraphicsPasses.at(execution.Name), args...);
        break;
    }
}

}
