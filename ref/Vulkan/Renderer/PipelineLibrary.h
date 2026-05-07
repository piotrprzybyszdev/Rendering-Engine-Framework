#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <span>
#include <utility>
#include <vector>

#include "Core/Core.h"

#include "DescriptorSet.h"
#include "ShaderLibrary.h"

namespace ref::vulkan
{

struct ComputePipelineInfo
{
    std::string Name;
    ShaderId ComputeShader;
};

struct GraphicsPipelineInfo
{
    std::string Name;
    ShaderId VertexShaderId;
    ShaderId GeometryShaderId;
    ShaderId FragmentShaderId;
};

using ComputePipelineId = IdType<size_t, ComputePipelineInfo>;
using GraphicsPipelineId = IdType<size_t, GraphicsPipelineInfo>;
using PipelineSpecialization = std::pair<uint32_t, std::array<std::byte, 4>>;

struct ComputePipelineInstanceInfo
{
    std::string Name;
    ComputePipelineId PipelineId;

    std::vector<PipelineSpecialization> PipelineSpecializations;
};

struct GraphicsPipelineInstanceInfo
{
    struct VertexInput
    {
        uint32_t Binding;
        uint32_t Offset;
    };

    std::string Name;
    GraphicsPipelineId PipelineId;

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

    std::vector<PipelineSpecialization> PipelineSpecializations;
};

template<typename I, typename T> void AddSpecializationConstant(I &info, uint32_t id, T value)
{
    static_assert(sizeof(T) == 4);

    PipelineSpecialization specialization;
    specialization.first = id;
    std::ranges::copy(std::as_bytes(std::span(&value, 1)), specialization.second.begin());
    info.PipelineSpecializations.push_back(specialization);
}

struct Pipeline
{
    std::string Name;
    std::vector<ShaderId> Shaders;
    std::filesystem::file_time_type UpdateTime;

    ref::vulkan::DescriptorSetBuilder DescriptorSetBuilder;
    ReflectionData Reflection;
    vk::PipelineLayout Layout;

    bool IsValid() const;
};

struct PipelineInstance
{
    std::filesystem::file_time_type UpdateTime = std::filesystem::file_time_type::min();
    vk::Pipeline Handle;

    bool IsValid() const;
};

using ComputePipelineInstanceId = IdType<size_t, ComputePipelineInstanceInfo>;
using GraphicsPipelineInstanceId = IdType<size_t, GraphicsPipelineInstanceInfo>;

class PipelineLibrary
{
public:
    PipelineLibrary(vk::Device logicalDevice, ShaderLibrary *shaderLibrary);
    ~PipelineLibrary();

    PipelineLibrary(const PipelineLibrary &) = delete;
    PipelineLibrary &operator=(const PipelineLibrary &) = delete;

    void SetPipelineCachePath(const std::filesystem::path &path);

    ComputePipelineId AddPipeline(ComputePipelineInfo pipelineInfo);
    GraphicsPipelineId AddPipeline(GraphicsPipelineInfo pipelineInfo);

    ComputePipelineInstanceId AddPipelineInstance(ComputePipelineInstanceInfo instanceInfo);
    GraphicsPipelineInstanceId AddPipelineInstance(GraphicsPipelineInstanceInfo instanceInfo);

    bool CompilePipelineInstance(ComputePipelineInstanceId id);
    bool CompilePipelineInstance(GraphicsPipelineInstanceId id);

    bool CompilePipelines();

    void WritePipelineCache();

    const Pipeline &GetPipeline(ComputePipelineInstanceId id) const;
    const Pipeline &GetPipeline(GraphicsPipelineInstanceId id) const;
    const PipelineInstance &GetPipelineInstance(ComputePipelineInstanceId id) const;
    const PipelineInstance &GetPipelineInstance(GraphicsPipelineInstanceId id) const;
    const ComputePipelineInstanceInfo &GetPipelineInstanceInfo(ComputePipelineInstanceId id) const;
    const GraphicsPipelineInstanceInfo &GetPipelineInstanceInfo(GraphicsPipelineInstanceId id) const;

private:
    vk::Device m_LogicalDevice;
    ShaderLibrary *m_ShaderLibrary;
    std::filesystem::path m_PipelineCachePath;
    vk::PipelineCache m_PipelineCache;

    std::vector<ComputePipelineInstanceInfo> m_ComputePipelineInstanceInfos;
    std::vector<GraphicsPipelineInstanceInfo> m_GraphicsPipelineInstanceInfos;

    std::vector<Pipeline> m_ComputePipelines;
    std::vector<Pipeline> m_GraphicsPipelines;

    std::vector<PipelineInstance> m_ComputePipelineInstances;
    std::vector<PipelineInstance> m_GraphicsPipelineInstances;

private:
    bool LoadPipeline(Pipeline &pipeline);

    std::filesystem::path GetCachePath() const;
};

}
