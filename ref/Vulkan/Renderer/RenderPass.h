#pragma once

#include <vulkan/vulkan.hpp>

#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

#include "ShaderLibrary.h"

namespace ref::vulkan
{

struct BufferBinding
{
    std::string BufferResource;
    uint32_t Binding;
    bool Read, Write;
    vk::DeviceSize Offset = 0;
    vk::DeviceSize Size = vk::WholeSize;
};

struct ImageBinding
{
    std::string ImageResource;
    uint32_t Binding;
    vk::Sampler Sampler;
    bool Read, Write;
};

struct ClearPassSpec
{
    std::string ImageResource;
    std::vector<vk::ImageSubresourceRange> Ranges;
    vk::ClearColorValue ClearValue;
};

struct BlitPassSpec
{
    std::string SrcImageResource;
    std::string DstImageResource;
    std::vector<vk::ImageBlit2> Regions;
    vk::Filter Filter = vk::Filter::eNearest;
};

struct ComputePassSpec
{
    ComputePipelineId Pipeline;
    std::span<const std::byte> PushConstantData;
    std::vector<BufferBinding> BufferBindings;
    std::vector<ImageBinding> ImageBindings;

    struct CommandSpec
    {
        uint32_t GroupCountX;
        uint32_t GroupCountY;
        uint32_t GroupCountZ;
    } Command;
};

struct PassAttachmentSpec
{
    std::string ImageResource;
    vk::AttachmentLoadOp LoadOp = vk::AttachmentLoadOp::eLoad;
    vk::AttachmentStoreOp StoreOp = vk::AttachmentStoreOp::eStore;
    vk::ClearValue ClearValue;

    std::optional<std::string> ResolveImageResource;
    vk::ResolveModeFlagBits ResolveMode = vk::ResolveModeFlagBits::eNone;
};

struct VertexBufferSpec
{
    std::string BufferResource;
    vk::DeviceSize Offset = 0;
};

struct VertexBuffersSpec
{
    std::vector<VertexBufferSpec> VertexBuffers;
    uint32_t FirstBinding = 0;
};

struct IndexBufferSpec
{
    std::string BufferResource;
    vk::DeviceSize Offset;
    vk::IndexType IndexType;
};

struct GraphicsPassSpec
{
    GraphicsPipelineId Pipeline;
    std::span<const std::byte> PushConstantData;
    std::vector<BufferBinding> BufferBindings;
    std::vector<ImageBinding> ImageBindings;

    VertexBuffersSpec VertexBuffers;

    std::vector<PassAttachmentSpec> ColorAttachments;
    std::optional<PassAttachmentSpec> DepthAttachment;
    std::optional<PassAttachmentSpec> StencilAttachment;
    vk::Rect2D RenderArea;

    std::vector<vk::Viewport> Viewports;
    std::vector<vk::Rect2D> Scissors;

    struct CommandSpec
    {
        uint32_t VertexCount;
        uint32_t InstanceCount;
        uint32_t FirstVertex;
        uint32_t FirstInstance;
    } Command;
};

struct IndexedGraphicsPassSpec
{
    GraphicsPipelineId Pipeline;
    std::span<const std::byte> PushConstantData;
    std::vector<BufferBinding> BufferBindings;
    std::vector<ImageBinding> ImageBindings;

    VertexBuffersSpec VertexBuffers;
    IndexBufferSpec IndexBuffer;

    std::vector<PassAttachmentSpec> ColorAttachments;
    std::optional<PassAttachmentSpec> DepthAttachment;
    std::optional<PassAttachmentSpec> StencilAttachment;
    vk::Rect2D RenderArea;

    std::vector<vk::Viewport> Viewports;
    std::vector<vk::Rect2D> Scissors;

    struct CommandSpec
    {
        uint32_t IndexCount;
        uint32_t InstanceCount;
        uint32_t FirstIndex;
        uint32_t VertexOffset;
        uint32_t FirstInstance;
    } Command;
};

struct IndexedIndirectGraphicsPassSpec
{
    GraphicsPipelineId Pipeline;
    std::span<const std::byte> PushConstantData;
    std::vector<BufferBinding> BufferBindings;
    std::vector<ImageBinding> ImageBindings;

    VertexBuffersSpec VertexBuffers;
    IndexBufferSpec IndexBuffer;

    std::vector<PassAttachmentSpec> ColorAttachments;
    std::optional<PassAttachmentSpec> DepthAttachment;
    std::optional<PassAttachmentSpec> StencilAttachment;
    vk::Rect2D RenderArea;

    std::string IndirectBufferResource;

    std::vector<vk::Viewport> Viewports;
    std::vector<vk::Rect2D> Scissors;

    struct CommandSpec
    {
        uint32_t Offset;
        uint32_t DrawCount;
        uint32_t Stride;
    } Command;
};

struct CustomGraphicsPassSpec
{
    using OnRenderCallback = std::function<void(vk::CommandBuffer)>;

    OnRenderCallback OnRender;

    std::vector<PassAttachmentSpec> ColorAttachments;
    std::optional<PassAttachmentSpec> DepthAttachment;
    std::optional<PassAttachmentSpec> StencilAttachment;

    vk::Rect2D RenderArea;
};

template<typename S>
concept HasClearValue = requires(S s) {
    { s.ClearValue } -> std::same_as<vk::ClearColorValue &>;
};

template<typename S>
concept HasFilter = requires(S s) {
    { s.Filter } -> std::same_as<vk::Filter &>;
};

template<typename R, typename T>
concept ElementRange = std::ranges::range<R> && std::same_as<std::ranges::range_value_t<R>, T>;

template<typename S>
concept HasRegions = requires(S s) {
    { s.Regions } -> ElementRange<vk::ImageBlit2>;
};

template<typename S>
concept HasRenderArea = requires(S s) {
    { s.RenderArea } -> std::same_as<vk::Rect2D &>;
};

template<typename S>
concept HasColorAttachments = requires(S s) {
    { s.ColorAttachments } -> ElementRange<PassAttachmentSpec>;
};

template<typename S>
concept HasDepthAttachment = requires(S s) {
    { s.DepthAttachment } -> std::same_as<std::optional<PassAttachmentSpec> &>;
};

template<typename S>
concept HasStencilAttachment = requires(S s) {
    { s.StencilAttachment } -> std::same_as<std::optional<PassAttachmentSpec> &>;
};

template<typename S>
concept HasDispatchCommand = requires(S s) {
    { s.Command } -> std::same_as<ComputePassSpec::CommandSpec &>;
};

template<typename S>
concept HasViewports = requires(S s) {
    { s.Viewports } -> std::same_as<std::vector<vk::Viewport> &>;
};

template<typename S>
concept HasScissors = requires(S s) {
    { s.Scissors } -> std::same_as<std::vector<vk::Rect2D> &>;
};

template<typename S>
concept HasDrawCommand = requires(S s) {
    { s.Command } -> std::same_as<GraphicsPassSpec::CommandSpec &>;
};

template<typename S>
concept HasIndexedIndirectDrawCommand = requires(S s) {
    { s.Command } -> std::same_as<IndexedIndirectGraphicsPassSpec::CommandSpec &>;
};

template<typename S>
concept HasImageResource = requires(S s) {
    { s.ImageResource } -> std::same_as<std::string &>;
};

template<typename S>
concept HasSrcImageResource = requires(S s) {
    { s.SrcImageResource } -> std::same_as<std::string &>;
};

template<typename S>
concept HasDstImageResource = requires(S s) {
    { s.DstImageResource } -> std::same_as<std::string &>;
};

template<typename S>
concept HasRanges = requires(S s) {
    { s.Ranges } -> ElementRange<vk::ImageSubresourceRange>;
};

template<typename S>
concept HasBindings = requires(S s) {
    { s.BufferBindings } -> ElementRange<BufferBinding>;
    { s.ImageBindings } -> ElementRange<ImageBinding>;
};

template<typename S>
concept HasVertexBuffers = requires(S s) {
    { s.VertexBuffers } -> std::same_as<VertexBuffersSpec &>;
};

template<typename S>
concept HasIndexBuffer = requires(S s) {
    { s.IndexBuffer } -> std::same_as<IndexBufferSpec &>;
};

template<typename S>
concept HasIndirectBuffer = requires(S s) {
    { s.IndirectBufferResource } -> std::same_as<std::string &>;
};

template<typename S> class DynamicConfig
{
public:
    DynamicConfig(S &spec) : m_Spec(spec)
    {
    }

    vk::Filter &GetFilter() requires HasFilter<S>
    {
        return m_Spec.Filter;
    }

    auto GetSrcOffsets() requires HasRegions<S>
    {
        return std::views::transform(
            m_Spec.Regions,
            [](auto &region) -> vk::ArrayWrapper1D<vk::Offset3D, 2> & { return region.srcOffsets; }
        );
    }

    auto GetDstOffsets() requires HasRegions<S>
    {
        return std::views::transform(
            m_Spec.Regions,
            [](auto &region) -> vk::ArrayWrapper1D<vk::Offset3D, 2> & { return region.dstOffsets; }
        );
    }

    vk::ClearColorValue &GetClearValue() requires HasClearValue<S>
    {
        return m_Spec.ClearValue;
    }

    vk::Rect2D &GetRenderArea() requires HasRenderArea<S>
    {
        return m_Spec.RenderArea;
    }

    auto GetColorAttachmentClearValues() requires HasColorAttachments<S>
    {
        return std::views::transform(m_Spec.ColorAttachments, [](auto &attachment) -> vk::ClearValue & {
            return attachment.ClearValue;
        });
    }

    vk::ClearDepthStencilValue &GetDepthAttachmentClearValue() requires HasDepthAttachment<S>
    {
        return m_Spec.DepthAttachment.ClearValue;
    }

    ComputePassSpec::CommandSpec &GetDispatchCommand() requires HasDispatchCommand<S>
    {
        return m_Spec.Command;
    }

    std::vector<vk::Viewport> &GetViewports() requires HasViewports<S>
    {
        return m_Spec.Viewports;
    }

    std::vector<vk::Rect2D> &GetScissors() requires HasScissors<S>
    {
        return m_Spec.Scissors;
    }

    GraphicsPassSpec::CommandSpec &GetDrawCommand() requires HasDrawCommand<S>
    {
        return m_Spec.Command;
    }

    IndexedIndirectGraphicsPassSpec::CommandSpec &GetIndexedIndirectDrawCommand() requires HasIndexedIndirectDrawCommand<S>
    {
        return m_Spec.Command;
    }

private:
    S &m_Spec;
};

using ClearPassDynamicConfig = DynamicConfig<ClearPassSpec>;
using BlitPassDynamicConfig = DynamicConfig<BlitPassSpec>;
using ComputePassDynamicConfig = DynamicConfig<ComputePassSpec>;
using GraphicsPassDynamicConfig = DynamicConfig<GraphicsPassSpec>;
using IndexedGraphicsPassDynamicConfig = DynamicConfig<IndexedGraphicsPassSpec>;
using IndexedIndirectGraphicsPassDynamicConfig = DynamicConfig<IndexedIndirectGraphicsPassSpec>;
using CustomGraphicsPassDynamicConfig = DynamicConfig<CustomGraphicsPassSpec>;

}
