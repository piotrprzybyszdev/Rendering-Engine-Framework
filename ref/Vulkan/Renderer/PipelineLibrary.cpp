#include <array>

#include "Core/Core.h"

#include "Vulkan/Application.h"

#include "PipelineLibrary.h"

namespace ref::vulkan
{

PipelineLibrary::PipelineLibrary(vk::Device logicalDevice, ShaderLibrary *shaderLibrary)
    : m_LogicalDevice(logicalDevice), m_ShaderLibrary(shaderLibrary)
{
}

ComputePipelineId PipelineLibrary::AddPipeline(ComputePipelineInfo pipelineInfo)
{
    m_ComputePipelines.push_back(Pipeline(std::move(pipelineInfo.Name), { pipelineInfo.ComputeShader }));
    return ComputePipelineId(m_ComputePipelines.size() - 1);
}

GraphicsPipelineId PipelineLibrary::AddPipeline(GraphicsPipelineInfo pipelineInfo)
{
    m_GraphicsPipelines.push_back(Pipeline(
        std::move(pipelineInfo.Name),
        { pipelineInfo.VertexShaderId, pipelineInfo.GeometryShaderId, pipelineInfo.FragmentShaderId }
    ));
    return GraphicsPipelineId(m_GraphicsPipelines.size() - 1);
}

ComputePipelineInstanceId PipelineLibrary::AddPipelineInstance(ComputePipelineInstanceInfo instanceInfo)
{
    m_ComputePipelineInstanceInfos.push_back(std::move(instanceInfo));
    m_ComputePipelineInstances.emplace_back(std::filesystem::file_time_type::min());
    return ComputePipelineInstanceId(m_ComputePipelineInstanceInfos.size() - 1);
}

GraphicsPipelineInstanceId PipelineLibrary::AddPipelineInstance(GraphicsPipelineInstanceInfo instanceInfo)
{
    m_GraphicsPipelineInstanceInfos.push_back(std::move(instanceInfo));
    m_GraphicsPipelineInstances.emplace_back(std::filesystem::file_time_type::min());
    return GraphicsPipelineInstanceId(m_GraphicsPipelineInstanceInfos.size() - 1);
}

bool PipelineLibrary::CompilePipelines()
{
    bool success = true;
    for (size_t id = 0; id < m_ComputePipelineInstances.size(); id++)
        success &= CompilePipelineInstance(ComputePipelineInstanceId(id));
    for (size_t id = 0; id < m_GraphicsPipelineInstances.size(); id++)
        success &= CompilePipelineInstance(GraphicsPipelineInstanceId(id));
    return success;
}

bool PipelineLibrary::LoadPipeline(Pipeline& pipeline)
{
    logger::debug("Loading pipeline `{}`", pipeline.Name);
    pipeline.UpdateTime = std::filesystem::file_time_type::min();
    pipeline.Reflection = ReflectionData();

    ShaderId previousShaderId;
    for (ShaderId shaderId : pipeline.Shaders)
    {
        if (!shaderId.IsValid())
            continue;

        m_ShaderLibrary->LoadShader(shaderId);
        
        const ShaderInfo &shaderInfo = m_ShaderLibrary->GetShaderInfo(shaderId);
        const Shader &shader = m_ShaderLibrary->GetShader(shaderId);
        
        if (!shader.IsValid())
        {
            pipeline = Pipeline(std::move(pipeline.Name), std::move(pipeline.Shaders));
            logger::error(
                "Cannot load pipeline `{}` because shader {} failed to compile", pipeline.Name,
                shaderInfo.Path.string()
            );
            return false;
        }

        pipeline.UpdateTime = std::max(pipeline.UpdateTime, shader.UpdateTime);
        pipeline.Reflection.Combine(shader.Reflection);

        if (previousShaderId.IsValid())
        {
            const ShaderInfo &previousShaderInfo = m_ShaderLibrary->GetShaderInfo(previousShaderId);
            const Shader &previousShader = m_ShaderLibrary->GetShader(previousShaderId);

            for (const auto &outputAttribute : previousShader.Reflection.OutputAttributes)
                for (const auto &inputAttribute : shader.Reflection.InputAttributes)
                    if (outputAttribute.Location == inputAttribute.Location &&
                        outputAttribute.Format != inputAttribute.Format)
                        logger::warn(
                            "In pipeline `{}` attribute at location {} in {} shader has format {} but in {} "
                            "shader has format {}",
                            pipeline.Name, outputAttribute.Location,
                            vk::to_string(previousShaderInfo.Stage), vk::to_string(outputAttribute.Format),
                            vk::to_string(shaderInfo.Stage), vk::to_string(inputAttribute.Format)
                        );
        }

        previousShaderId = shaderId;
    }

    if (pipeline.IsValid() && pipeline.UpdateTime <= pipeline.UpdateTime)
    {
        logger::debug("Pipeline `{}` is up to date", pipeline.Name);
        return true;
    }

    // TODO: ArraySize Hint
    pipeline.DescriptorSetBuilder = DescriptorSetBuilder(m_LogicalDevice);
    for (const auto &binding : pipeline.Reflection.SetLayoutBindings)
        pipeline.DescriptorSetBuilder.SetDescriptor(binding);

    auto descLayout = pipeline.DescriptorSetBuilder.CreateLayout();

    vk::PipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setSetLayouts(descLayout);
    layoutCreateInfo.setPushConstantRanges(pipeline.Reflection.PushConstantRanges);

    pipeline.Layout = m_LogicalDevice.createPipelineLayout(layoutCreateInfo);
    Application::GetInstance()->SetDebugName(pipeline.Layout, pipeline.Name.c_str());

    return true;
}

const Pipeline &PipelineLibrary::GetPipeline(ComputePipelineInstanceId id) const
{
    return m_ComputePipelines[m_ComputePipelineInstanceInfos[id].PipelineId];
}

const Pipeline &PipelineLibrary::GetPipeline(GraphicsPipelineInstanceId id) const
{
    return m_GraphicsPipelines[m_GraphicsPipelineInstanceInfos[id].PipelineId];
}

const PipelineInstance &PipelineLibrary::GetPipelineInstance(ComputePipelineInstanceId id) const
{
    return m_ComputePipelineInstances[id];
}

const PipelineInstance &PipelineLibrary::GetPipelineInstance(GraphicsPipelineInstanceId id) const
{
    return m_GraphicsPipelineInstances[id];
}

const ComputePipelineInstanceInfo &PipelineLibrary::GetPipelineInstanceInfo(
    ComputePipelineInstanceId id
) const
{
    return m_ComputePipelineInstanceInfos[id];
}

const GraphicsPipelineInstanceInfo &PipelineLibrary::GetPipelineInstanceInfo(
    GraphicsPipelineInstanceId id
) const
{
    return m_GraphicsPipelineInstanceInfos[id];
}

static std::pair<vk::SpecializationInfo, std::vector<vk::SpecializationMapEntry>> GetSpecializations(
    const Pipeline &pipeline, const auto &pipelineInstanceInfo
)
{
    std::vector<vk::SpecializationMapEntry> specializations;
    for (const auto &[specId, value] : pipelineInstanceInfo.PipelineSpecializations)
    {
        if (!std::ranges::contains(pipeline.Reflection.SpecializationConstantIds, specId))
        {
            logger::warn(
                "Compute pipeline `{}` does not contain a specialization constant with id {} but one was "
                "provided for pipeline instance `{}`",
                pipeline.Name, specId, pipelineInstanceInfo.Name
            );
        }

        const uint32_t offset = static_cast<uint32_t>(
            reinterpret_cast<uintptr_t>(value.data()) -
            reinterpret_cast<uintptr_t>(pipelineInstanceInfo.PipelineSpecializations.data())
        );
        specializations.emplace_back(specId, offset, std::span(value).size_bytes());
    }

    vk::SpecializationInfo specializationInfo;
    specializationInfo.setPData(pipelineInstanceInfo.PipelineSpecializations.data());
    specializationInfo.setDataSize(std::span(pipelineInstanceInfo.PipelineSpecializations).size_bytes());
    specializationInfo.setMapEntries(specializations);

    return { specializationInfo, std::move(specializations) };
}

bool PipelineLibrary::CompilePipelineInstance(ComputePipelineInstanceId id)
{
    auto &instance = m_ComputePipelineInstances[id];
    const auto &pipelineInstanceInfo = m_ComputePipelineInstanceInfos[id];
    const auto &pipeline = m_ComputePipelines[pipelineInstanceInfo.PipelineId];

    if (!LoadPipeline(m_ComputePipelines[pipelineInstanceInfo.PipelineId]))
    {
        logger::error(
            "Cannot load compute pipeline instance `{}` because compute pipeline `{}` failed to load",
            pipelineInstanceInfo.Name, pipelineInstanceInfo.Name
        );
        return false;
    }

    if (pipeline.UpdateTime <= instance.UpdateTime)
    {
        logger::debug("Compute pipeline instance `{}` is already up to date", pipelineInstanceInfo.Name);
        return true;
    }

    assert(pipeline.Shaders.size() == 1);
    ShaderId shaderId = pipeline.Shaders.front();
    const ShaderInfo &shaderInfo = m_ShaderLibrary->GetShaderInfo(shaderId);
    const Shader &shader = m_ShaderLibrary->GetShader(shaderId);
    assert(shaderInfo.Stage == vk::ShaderStageFlagBits::eCompute);

    assert(shader.IsValid());
    assert(pipeline.IsValid());

    auto [specializationInfo, specializations] = GetSpecializations(pipeline, pipelineInstanceInfo);
    vk::PipelineShaderStageCreateInfo stageInfo(
        vk::PipelineShaderStageCreateFlags(), shaderInfo.Stage, nullptr, shaderInfo.Entry.c_str(),
        &specializationInfo
    );
    vk::ShaderModuleCreateInfo moduleInfo(vk::ShaderModuleCreateFlags(), shader.Code);
    auto cs = m_LogicalDevice.createShaderModuleUnique(moduleInfo);
    stageInfo.setModule(cs.get());

    vk::ComputePipelineCreateInfo computePipelineInfo;
    computePipelineInfo.setStage(stageInfo);
    computePipelineInfo.setLayout(pipeline.Layout);

    auto result = m_LogicalDevice.createComputePipeline(nullptr, computePipelineInfo);
    if (!result.has_value())
    {
        logger::error("Compute pipeline instance `{}` compilation failed", pipelineInstanceInfo.Name);
        instance = PipelineInstance();
        return false;
    }

    Application::GetInstance()->SetDebugName(result.value, pipelineInstanceInfo.Name.c_str());

    logger::info("Successfully compiled compute pipeline `{}`", pipelineInstanceInfo.Name);

    instance = {
        .UpdateTime = pipeline.UpdateTime,
        .Handle = result.value,
    };

    return true;
}

bool PipelineLibrary::CompilePipelineInstance(GraphicsPipelineInstanceId id)
{
    auto &instance = m_GraphicsPipelineInstances[id];
    const auto &pipelineInstanceInfo = m_GraphicsPipelineInstanceInfos[id];
    const auto &pipeline = m_GraphicsPipelines[pipelineInstanceInfo.PipelineId];

    if (!LoadPipeline(m_GraphicsPipelines[pipelineInstanceInfo.PipelineId]))
    {
        logger::error(
            "Cannot load graphics pipeline instance `{}` because graphics pipeline `{}` failed to load",
            pipelineInstanceInfo.Name, pipeline.Name
        );
        return false;
    }

    if (pipeline.UpdateTime <= instance.UpdateTime)
    {
        logger::debug("Graphics pipeline instance `{}` is already up to date", pipelineInstanceInfo.Name);
        return true;
    }

    auto [specializationInfo, specializations] = GetSpecializations(pipeline, pipelineInstanceInfo);

    ShaderId vertexShaderId, fragmentShaderId;
    std::vector<vk::UniqueShaderModule> shaderModules;
    std::vector<vk::PipelineShaderStageCreateInfo> stageInfos;

    for (ShaderId shaderId : pipeline.Shaders)
    {
        if (!shaderId.IsValid())
            continue;

        const ShaderInfo &info = m_ShaderLibrary->GetShaderInfo(shaderId);
        const Shader &shader = m_ShaderLibrary->GetShader(shaderId);

        assert(shader.IsValid());

        if (info.Stage == vk::ShaderStageFlagBits::eVertex)
            vertexShaderId = shaderId;
        if (info.Stage == vk::ShaderStageFlagBits::eFragment)
            fragmentShaderId = shaderId;

        shaderModules.push_back(m_LogicalDevice.createShaderModuleUnique(
            vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shader.Code)
        ));
        stageInfos.emplace_back(
            vk::PipelineShaderStageCreateFlags(), info.Stage, shaderModules.back().get(), info.Entry.c_str(),
            &specializationInfo
        );
    }

    assert(vertexShaderId.IsValid() && fragmentShaderId.IsValid());
    {
        const size_t required = m_ShaderLibrary->GetShader(vertexShaderId).Reflection.InputAttributes.size();
        const size_t provided = pipelineInstanceInfo.VertexInputs.size();
        if (required > provided)
            throw std::runtime_error(
                std::format(
                    "Vertex shader requires {} inputs but pipeline instnace {} provided only {}", required,
                    pipelineInstanceInfo.Name, provided
                )
            );

        if (required < provided)
            logger::warn(
                "Pipeline instance {} provided {} inputs but vertex shader requires only {}",
                pipelineInstanceInfo.Name, required, provided
            );
    }

    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    for (const auto &attribute : m_ShaderLibrary->GetShader(vertexShaderId).Reflection.InputAttributes)
    {
        const auto &input = pipelineInstanceInfo.VertexInputs[attribute.Location];
        attributeDescriptions.emplace_back(attribute.Location, input.Binding, attribute.Format, input.Offset);
    }

    vk::PipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.setVertexAttributeDescriptions(attributeDescriptions);
    vertexInputState.setVertexBindingDescriptions(pipelineInstanceInfo.BindingDescriptions);

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setScissorCount(1);
    viewportState.setViewportCount(1);

    for ([[maybe_unused]] const auto &attribute :
         m_ShaderLibrary->GetShader(fragmentShaderId).Reflection.OutputAttributes)
        assert(attribute.Location < pipelineInstanceInfo.AttachmentBlendStates.size());

    vk::PipelineColorBlendStateCreateInfo blendState;
    blendState.setAttachments(pipelineInstanceInfo.AttachmentBlendStates);

    vk::PipelineRenderingCreateInfo renderingInfo(
        0, pipelineInstanceInfo.ColorAttachmentFormats, pipelineInstanceInfo.DepthAttachmentFormat,
        pipelineInstanceInfo.StencilAttachmentFormat
    );

    vk::PipelineDynamicStateCreateInfo dynamicState;
    std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport,
                                                      vk::DynamicState::eScissor };
    dynamicState.setDynamicStates(dynamicStates);

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo;
    graphicsPipelineInfo.setPVertexInputState(&vertexInputState);
    graphicsPipelineInfo.setPInputAssemblyState(&pipelineInstanceInfo.InputAssemblyState);
    graphicsPipelineInfo.setPMultisampleState(&pipelineInstanceInfo.MultisampleState);
    graphicsPipelineInfo.setPRasterizationState(&pipelineInstanceInfo.RasterizationState);
    graphicsPipelineInfo.setPDepthStencilState(&pipelineInstanceInfo.DepthStencilState);
    graphicsPipelineInfo.setPColorBlendState(&blendState);
    graphicsPipelineInfo.setPViewportState(&viewportState);
    graphicsPipelineInfo.setPDynamicState(&dynamicState);
    graphicsPipelineInfo.setStages(stageInfos);
    graphicsPipelineInfo.setLayout(pipeline.Layout);
    graphicsPipelineInfo.setPNext(renderingInfo);

    auto result = m_LogicalDevice.createGraphicsPipeline(nullptr, graphicsPipelineInfo);
    if (!result.has_value())
    {
        logger::error("Graphics pipeline instance `{}` compilation failed", pipelineInstanceInfo.Name);
        instance = PipelineInstance();
        return false;
    }

    Application::GetInstance()->SetDebugName(result.value, pipelineInstanceInfo.Name.c_str());

    logger::info("Successfully compiled graphics pipeline instnace `{}`", pipelineInstanceInfo.Name);

    instance = {
        .UpdateTime = pipeline.UpdateTime,
        .Handle = result.value,
    };

    return true;
}

bool Pipeline::IsValid() const
{
    return Layout != nullptr;
}

bool PipelineInstance::IsValid() const
{
    return Handle != nullptr;
}

}