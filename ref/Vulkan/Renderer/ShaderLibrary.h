#pragma once

#include <vulkan/vulkan.hpp>

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "Core/Core.h"

#include "DescriptorSet.h"

namespace ref::vulkan
{

struct ShaderInfo
{
    std::filesystem::path Path;
    std::string Entry;
    vk::ShaderStageFlagBits Stage;

    bool operator==(const ShaderInfo &other) const = default;
};

struct ReflectionData
{
    vk::PushConstantRange PushConstants;
    std::vector<uint32_t> SpecializationConstantIds;
    std::vector<vk::DescriptorSetLayoutBinding> SetLayoutBindings;

    struct Attribute
    {
        uint32_t Location;
        vk::Format Format;
    };

    std::vector<Attribute> InputAttributes;
    std::vector<Attribute> OutputAttributes;

    bool HasPushConstants() const;
    void Combine(const ReflectionData &other);
};

struct Shader
{
    std::vector<uint32_t> Code;
    ReflectionData Reflection;

    bool IsValid() const;
};

using ShaderId = IdType<size_t, Shader>;

struct ShaderCompilationFailure
{
    ShaderId Id;
    std::string Error;
};

class ShaderLibrary
{
public:
    ShaderLibrary(vk::Device logicalDevice, uint32_t apiVersion);

    ShaderLibrary(const ShaderLibrary &) = delete;
    ShaderLibrary &operator=(const ShaderLibrary &) = delete;

    ShaderId AddShader(ShaderInfo info);

    void LoadShader(ShaderId id);
    void LoadShaders();

    void WriteShaderCache();

    const ShaderInfo &GetShaderInfo(ShaderId id) const;
    const Shader &GetShader(ShaderId id) const;

    std::span<const ShaderCompilationFailure> GetCompilationFailedShaders() const;

private:
    vk::Device m_LogicalDevice;
    const uint32_t m_ApiVersion;

    std::vector<ShaderInfo> m_ShaderInfos;
    std::vector<Shader> m_Shaders;

    std::vector<ShaderCompilationFailure> m_CompilationFailures;

private:
    ReflectionData ReflectShader(std::span<const uint32_t> spirv, vk::ShaderStageFlagBits stage);

    void CompileShader(ShaderId id);
    void CompileShaders();

    void WriteShaderCache(ShaderId id);
    void ReadShaderCache(ShaderId id);

private:
    std::filesystem::path GetShaderCachePath(const std::filesystem::path &path);
};

using PipelineSpecialization = std::pair<uint32_t, uint32_t>;

struct ComputePipelineInfo
{
    std::string Name;

    ShaderId ComputeShader;

    // TODO: std::vector<PipelineSpecialization> PipelineSpecializations;
};

struct GraphicsPipelineInfo
{
    struct VertexInput
    {
        uint32_t Binding;
        uint32_t Offset;
    };

    std::string Name;

    ShaderId VertexShaderId;
    ShaderId FragmentShaderId;

    std::vector<vk::VertexInputBindingDescription> BindingDescriptions;
    std::vector<VertexInput> VertexInputs;
    std::vector<vk::Format> ColorAttachmentFormats;
    vk::Format DepthAttachmentFormat = vk::Format::eUndefined;
    vk::Format StencilAttachmentFormat = vk::Format::eUndefined;

    vk::PipelineInputAssemblyStateCreateInfo InputAssemblyState;
    vk::PipelineMultisampleStateCreateInfo MultisampleState;
    vk::PipelineRasterizationStateCreateInfo RasterizationState;
    vk::PipelineDepthStencilStateCreateInfo DepthStencilState;
    std::vector<vk::PipelineColorBlendAttachmentState> AttachmentBlendStates;

    // TODO: std::vector<PipelineSpecialization> PipelineSpecializations;
};

struct Pipeline
{
    ref::vulkan::DescriptorSetBuilder DescriptorSetBuilder;
    std::vector<vk::PushConstantRange> PushConstants;
    vk::PipelineLayout Layout;
    vk::Pipeline Handle;

    bool IsValid() const;
};

using ComputePipelineId = IdType<size_t, ComputePipelineInfo>;
using GraphicsPipelineId = IdType<size_t, GraphicsPipelineInfo>;

class PipelineLibrary
{
public:
    PipelineLibrary(vk::Device logicalDevice, ShaderLibrary *shaderLibrary);

    PipelineLibrary(const PipelineLibrary &) = delete;
    PipelineLibrary &operator=(const PipelineLibrary &) = delete;

    ComputePipelineId AddPipeline(ComputePipelineInfo pipelineInfo);
    GraphicsPipelineId AddPipeline(GraphicsPipelineInfo pipelineInfo);

    bool CompilePipeline(ComputePipelineId id);
    bool CompilePipeline(GraphicsPipelineId id);
    bool CompilePipelines();

    const Pipeline &GetPipeline(ComputePipelineId id) const;
    const Pipeline &GetPipeline(GraphicsPipelineId id) const;

private:
    vk::Device m_LogicalDevice;
    ShaderLibrary *m_ShaderLibrary;

    std::vector<std::pair<ComputePipelineInfo, size_t>> m_ComputePipelineInfos;
    std::vector<std::pair<GraphicsPipelineInfo, size_t>> m_GraphicsPipelineInfos;

    std::vector<Pipeline> m_Pipelines;
};

}
