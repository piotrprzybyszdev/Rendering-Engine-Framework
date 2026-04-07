#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>

#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <ranges>
#include <span>

#include "Core/Core.h"

#include "Vulkan/Application.h"

#include "ShaderLibrary.h"

namespace ref::vulkan
{

namespace
{

shaderc_shader_kind ToShaderKind(vk::ShaderStageFlagBits stage)
{
    switch (stage)
    {
    case vk::ShaderStageFlagBits::eVertex:
        return shaderc_shader_kind::shaderc_vertex_shader;
    case vk::ShaderStageFlagBits::eFragment:
        return shaderc_shader_kind::shaderc_fragment_shader;
    case vk::ShaderStageFlagBits::eCompute:
        return shaderc_shader_kind::shaderc_compute_shader;
    default:
        throw std::runtime_error("Unsupported shader stage");
    }
}

vk::Format ToFormat(uint32_t components, spirv_cross::SPIRType::BaseType type)
{
    using Type = spirv_cross::SPIRType::BaseType;

    if (type == Type::Float && components == 1)
        return vk::Format::eR32Sfloat;
    if (type == Type::Float && components == 2)
        return vk::Format::eR32G32Sfloat;
    if (type == Type::Float && components == 3)
        return vk::Format::eR32G32B32Sfloat;
    if (type == Type::Float && components == 4)
        return vk::Format::eR32G32B32A32Sfloat;

    throw std::runtime_error("Unsupported base type or component count");
}

}

ShaderLibrary::ShaderLibrary(vk::Device logicalDevice, uint32_t apiVersion)
    : m_LogicalDevice(logicalDevice), m_ApiVersion(apiVersion)
{
}

ShaderId ShaderLibrary::AddShader(ShaderInfo info)
{
    auto it = std::ranges::find_if(m_ShaderInfos, [&info](const auto &other) { return info == other; });

    if (it != m_ShaderInfos.end())
        return std::distance(m_ShaderInfos.begin(), it);

    m_ShaderInfos.push_back(std::move(info));
    m_Shaders.push_back(Shader());
    return ShaderId(m_ShaderInfos.size() - 1);
}

void ShaderLibrary::LoadShader(ShaderId id)
{
    const ShaderInfo &info = m_ShaderInfos[id];
    logger::debug("Loading Shader {}", info.Path.string());

    const std::filesystem::path cache = GetShaderCachePath(info.Path);
    if (std::filesystem::exists(cache) &&
        std::filesystem::last_write_time(info.Path) < std::filesystem::last_write_time(cache))
        ReadShaderCache(id);
    else
        CompileShader(id);
}

void ShaderLibrary::LoadShaders()
{
    for (size_t id = 0; id < m_ShaderInfos.size(); id++)
        LoadShader(ShaderId(id));
}

void ShaderLibrary::WriteShaderCache()
{
    for (size_t id = 0; id < m_ShaderInfos.size(); id++)
    {
        ShaderInfo &info = m_ShaderInfos[id];
        std::filesystem::path cache = GetShaderCachePath(info.Path);
        if (m_Shaders[id].IsValid() &&
            (!std::filesystem::exists(cache) ||
             std::filesystem::last_write_time(info.Path) > std::filesystem::last_write_time(cache)))
            WriteShaderCache(ShaderId(id));
    }
}

void ShaderLibrary::CompileShader(ShaderId id)
{
    std::erase_if(m_CompilationFailures, [&id](auto &failure) { return failure.Id == id; });

    const auto &shaderInfo = m_ShaderInfos[id];

    std::vector<char> data;
    {
        std::ifstream file(shaderInfo.Path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error(
                std::format("Shader file {} cannot be opened", shaderInfo.Path.string())
            );

        size_t size = file.tellg();
        data.resize(size);
        file.seekg(0);
        file.read(data.data(), size);
        file.close();
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, m_ApiVersion);

    const std::string path = shaderInfo.Path.string();
    auto result = compiler.CompileGlslToSpv(
        data.data(), data.size(), ToShaderKind(shaderInfo.Stage), path.c_str(), shaderInfo.Entry.c_str(),
        options
    );

    if (result.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success)
    {
        m_Shaders[id] = Shader();
        m_CompilationFailures.push_back(ShaderCompilationFailure(id, result.GetErrorMessage()));
        logger::error("Shader `{}` failed compilation", shaderInfo.Path.string());
        logger::error("{}", result.GetErrorMessage());
        return;
    }

    const auto spv = std::span(result);

    Shader shader;
    shader.Reflection = ReflectShader(spv, shaderInfo.Stage);
    shader.Code.assign(spv.begin(), spv.end());

    logger::info("Successfully compiled shader `{}`", shaderInfo.Path.string());
    m_Shaders[id] = std::move(shader);
}

void ShaderLibrary::CompileShaders()
{
    for (size_t id = 0; id < m_ShaderInfos.size(); id++)
        CompileShader(ShaderId(id));
}

const ShaderInfo &ShaderLibrary::GetShaderInfo(ShaderId id) const
{
    return m_ShaderInfos[id];
}

const Shader &ShaderLibrary::GetShader(ShaderId id) const
{
    return m_Shaders[id];
}

std::span<const ShaderCompilationFailure> ShaderLibrary::GetCompilationFailedShaders() const
{
    return m_CompilationFailures;
}

ReflectionData ShaderLibrary::ReflectShader(std::span<const uint32_t> spirv, vk::ShaderStageFlagBits stage)
{
    ReflectionData reflection;

    spirv_cross::Compiler compiler(spirv.data(), spirv.size());

    auto reflectBindings =
        [&reflection, &compiler,
         &stage](std::span<const spirv_cross::Resource> resources, vk::DescriptorType descriptorType) {
            for (const spirv_cross::Resource &resource : resources)
            {
                const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

                // TODO: Support multiple sets
                if (set != 0)
                {
                    logger::warn(
                        "Only one descriptor set is supported, ignoring binding {}, set {}", binding, set
                    );
                    continue;
                }

                reflection.SetLayoutBindings.emplace_back(binding, descriptorType, 1, stage);
            }
        };

    auto reflectAttributes = [&compiler](std::span<const spirv_cross::Resource> resources) {
        std::vector<ReflectionData::Attribute> attributes;

        for (const spirv_cross::Resource &resource : resources)
        {
            const uint32_t location = compiler.get_decoration(resource.id, spv::DecorationLocation);
            const auto &type = compiler.get_type(resource.type_id);
            const vk::Format format = ToFormat(type.vecsize, type.basetype);

            attributes.emplace_back(location, format);
        }

        return attributes;
    };

    const auto resources = compiler.get_shader_resources();

    reflectBindings(resources.sampled_images, vk::DescriptorType::eCombinedImageSampler);
    reflectBindings(resources.storage_buffers, vk::DescriptorType::eStorageBuffer);
    reflectBindings(resources.storage_images, vk::DescriptorType::eStorageImage);
    reflectBindings(resources.uniform_buffers, vk::DescriptorType::eUniformBuffer);

    reflection.InputAttributes = reflectAttributes(resources.stage_inputs);
    reflection.OutputAttributes = reflectAttributes(resources.stage_outputs);

    if (!resources.push_constant_buffers.empty())
    {
        assert(resources.push_constant_buffers.size() == 1);
        auto &pushConstants = resources.push_constant_buffers.front();

        const auto ranges = compiler.get_active_buffer_ranges(pushConstants.id);

        uint32_t offset = 0;
        uint32_t size = 0;
        for (auto &range : ranges)
        {
            if (range.offset <= offset)
            {
                size += offset - static_cast<uint32_t>(range.offset);
                offset = static_cast<uint32_t>(range.offset);
            }

            size = std::max(size, static_cast<uint32_t>(range.offset + range.range) - offset);
        }

        reflection.PushConstants = { stage, offset, size };
    }

    auto specializationConstants = compiler.get_specialization_constants();
    for (const auto &specialization : specializationConstants)
        reflection.SpecializationConstantIds.push_back(specialization.constant_id);

    return reflection;
}

struct ShaderCacheHeader
{
    uint32_t Magic;
    uint32_t Version;
    struct ReflectionInfo
    {
        uint32_t SpecilizationConstantCount;
        uint32_t BindingCount;
        uint32_t InputAttributeCount;
        uint32_t OutputAttributeCount;
    } Reflection;
    struct CodeInfo
    {
        uint32_t Offset;
        uint32_t Size;
    } Code;
};

namespace
{

template<typename T> T Align(T value, T alignment)
{
    assert(std::has_single_bit(alignment));
    return (value + alignment - 1) & ~(alignment - 1);
}

template<typename I, typename T> void AddOffset(T &position, T alignment)
{
    position = Align(position + sizeof(I), alignment);
}

template<typename I, typename T> void AddOffset(T &position, T alignment, std::span<const I> items)
{
    position = Align(position + items.size_bytes(), alignment);
}

static constexpr uint64_t g_Alignment = 16;
static constexpr std::array<char, g_Alignment> g_Padding = {};

template<typename I, typename T, typename C>
void Serialize(std::basic_ostream<C> &stream, T alignment, const I &item)
{
    stream.write(reinterpret_cast<const C *>(&item), sizeof(I));
    uint64_t position = stream.tellp();
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.write(g_Padding.data(), paddingSize);
}

template<typename I, typename T, typename C>
void Serialize(std::basic_ostream<C> &stream, T alignment, std::span<const I> items)
{
    stream.write(reinterpret_cast<const C *>(items.data()), items.size_bytes());
    uint64_t position = stream.tellp();
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.write(g_Padding.data(), paddingSize);
}

template<typename I, typename T, typename C>
void Deserialize(std::basic_istream<C> &stream, T alignment, I &item)
{
    [[maybe_unused]] uint64_t before = stream.tellg();
    stream.read(reinterpret_cast<C *>(&item), sizeof(I));
    uint64_t position = stream.tellg();
    assert(position - before == sizeof(I));
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.ignore(paddingSize);
    [[maybe_unused]] uint64_t after = stream.tellg();
    assert(after - position == paddingSize);
}

template<typename I, typename T, typename C>
void Deserialize(std::basic_istream<C> &stream, T alignment, size_t count, std::vector<I> &items)
{
    items.resize(count);
    [[maybe_unused]] uint64_t before = stream.tellg();
    stream.read(reinterpret_cast<C *>(items.data()), count * sizeof(I));
    uint64_t position = stream.tellg();
    assert(position - before == count * sizeof(I));
    uint64_t paddingSize = Align(position, alignment) - position;
    stream.ignore(paddingSize);
    [[maybe_unused]] uint64_t after = stream.tellg();
    assert(after - position == paddingSize);
}

}

constexpr uint32_t g_Magic = 0x72736368;
constexpr uint32_t g_Version = vk::makeApiVersion(0u, 0u, 1u, 0u);

void ShaderLibrary::WriteShaderCache(ShaderId id)
{
    const Shader &shader = m_Shaders[id];
    const ShaderInfo &shaderInfo = m_ShaderInfos[id];
    assert(shader.IsValid());

    const std::filesystem::path path = GetShaderCachePath(shaderInfo.Path);

    uint64_t offset = 0;
    AddOffset<ShaderCacheHeader>(offset, g_Alignment);
    AddOffset<vk::PushConstantRange>(offset, g_Alignment);
    AddOffset(offset, g_Alignment, std::span(shader.Reflection.SpecializationConstantIds));
    AddOffset(offset, g_Alignment, std::span(shader.Reflection.SetLayoutBindings));
    AddOffset(offset, g_Alignment, std::span(shader.Reflection.InputAttributes));
    AddOffset(offset, g_Alignment, std::span(shader.Reflection.OutputAttributes));

    ShaderCacheHeader header = {
        .Magic = g_Magic,
        .Version = g_Version,
        .Reflection = {
            .SpecilizationConstantCount = static_cast<uint32_t>(shader.Reflection.SpecializationConstantIds.size()),
            .BindingCount = static_cast<uint32_t>(shader.Reflection.SetLayoutBindings.size()),
            .InputAttributeCount = static_cast<uint32_t>(shader.Reflection.InputAttributes.size()),
            .OutputAttributeCount = static_cast<uint32_t>(shader.Reflection.OutputAttributes.size()),
        },
        .Code = {
            .Offset = static_cast<uint32_t>(offset),
            .Size = static_cast<uint32_t>(shader.Code.size()),
        },
    };

    std::ofstream file(path, std::ios::binary | std::ios::out);
    Serialize(file, g_Alignment, header);
    Serialize(file, g_Alignment, shader.Reflection.PushConstants);
    assert(file.is_open());
    Serialize(file, g_Alignment, std::span(shader.Reflection.SpecializationConstantIds));
    Serialize(file, g_Alignment, std::span(shader.Reflection.SetLayoutBindings));
    Serialize(file, g_Alignment, std::span(shader.Reflection.InputAttributes));
    Serialize(file, g_Alignment, std::span(shader.Reflection.OutputAttributes));
    assert(file.tellp() == header.Code.Offset);
    Serialize(file, g_Alignment, std::span(shader.Code));

    logger::info("Successfully written cache for shader `{}`", shaderInfo.Path.string());
}

void ShaderLibrary::ReadShaderCache(ShaderId id)
{
    std::erase_if(m_CompilationFailures, [&id](auto &failure) { return failure.Id == id; });

    const ShaderInfo &shaderInfo = m_ShaderInfos[id];
    const std::filesystem::path path = GetShaderCachePath(shaderInfo.Path);
    assert(std::filesystem::last_write_time(path) > std::filesystem::last_write_time(shaderInfo.Path));

    std::ifstream file(path, std::ios::binary | std::ios::in);
    assert(file.is_open());

    ShaderCacheHeader header = {};
    Deserialize(file, g_Alignment, header);
    assert(header.Magic == g_Magic);
    assert(header.Version == g_Version);

    Shader shader;
    auto &reflection = shader.Reflection;
    Deserialize(file, g_Alignment, reflection.PushConstants);
    Deserialize(
        file, g_Alignment, header.Reflection.SpecilizationConstantCount, reflection.SpecializationConstantIds
    );
    Deserialize(file, g_Alignment, header.Reflection.BindingCount, reflection.SetLayoutBindings);
    Deserialize(file, g_Alignment, header.Reflection.InputAttributeCount, reflection.InputAttributes);
    Deserialize(file, g_Alignment, header.Reflection.OutputAttributeCount, reflection.OutputAttributes);
    assert(header.Code.Offset == file.tellg());
    Deserialize(file, g_Alignment, header.Code.Size, shader.Code);

    file.ignore();
    assert(file.eof());

    logger::info("Successfully read cache for shader `{}`", shaderInfo.Path.string());
    m_Shaders[id] = std::move(shader);
}

std::filesystem::path ShaderLibrary::GetShaderCachePath(const std::filesystem::path &path)
{
    std::filesystem::path cache = path;
    cache.concat(".rsc");
    return cache;
}

PipelineLibrary::PipelineLibrary(vk::Device logicalDevice, ShaderLibrary *shaderLibrary)
    : m_LogicalDevice(logicalDevice), m_ShaderLibrary(shaderLibrary)
{
}

ComputePipelineId PipelineLibrary::AddPipeline(ComputePipelineInfo pipelineInfo)
{
    m_Pipelines.emplace_back();
    m_ComputePipelineInfos.emplace_back(std::move(pipelineInfo), m_Pipelines.size() - 1);
    return m_ComputePipelineInfos.size() - 1;
}

GraphicsPipelineId PipelineLibrary::AddPipeline(GraphicsPipelineInfo pipelineInfo)
{
    m_Pipelines.emplace_back();
    m_GraphicsPipelineInfos.emplace_back(std::move(pipelineInfo), m_Pipelines.size() - 1);
    return m_GraphicsPipelineInfos.size() - 1;
}

bool PipelineLibrary::CompilePipelines()
{
    bool result = true;
    for (size_t i = 0; i < m_ComputePipelineInfos.size(); i++)
        result &= CompilePipeline(ComputePipelineId(i));
    for (size_t i = 0; i < m_GraphicsPipelineInfos.size(); i++)
        result &= CompilePipeline(GraphicsPipelineId(i));
    return result;
}

const Pipeline &PipelineLibrary::GetPipeline(ComputePipelineId id) const
{
    return m_Pipelines[m_ComputePipelineInfos[id].second];
}

const Pipeline &PipelineLibrary::GetPipeline(GraphicsPipelineId id) const
{
    return m_Pipelines[m_GraphicsPipelineInfos[id].second];
}

bool PipelineLibrary::CompilePipeline(ComputePipelineId id)
{
    const auto &[pipelineInfo, pipelineIndex] = m_ComputePipelineInfos[id];
    const auto &info = m_ShaderLibrary->GetShaderInfo(pipelineInfo.ComputeShader);
    const auto &shader = m_ShaderLibrary->GetShader(pipelineInfo.ComputeShader);

    assert(info.Stage == vk::ShaderStageFlagBits::eCompute);

    if (!shader.IsValid())
    {
        logger::error(
            "Cannot compile compute pipeline `{}` because the shader failed compilation", pipelineInfo.Name
        );
        m_Pipelines[pipelineIndex] = Pipeline();
        return false;
    }

    std::vector<vk::PushConstantRange> pushConstants = { shader.Reflection.PushConstants };

    // TODO: ArraySize Hint
    DescriptorSetBuilder descriptorSetBuilder(m_LogicalDevice);
    for (const auto &binding : shader.Reflection.SetLayoutBindings)
        descriptorSetBuilder.SetDescriptor(binding);

    auto descLayout = descriptorSetBuilder.CreateLayout();

    vk::PipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setSetLayouts(descLayout);
    if (shader.Reflection.HasPushConstants())
        layoutCreateInfo.setPushConstantRanges(shader.Reflection.PushConstants);

    auto layout = m_LogicalDevice.createPipelineLayout(layoutCreateInfo);

    vk::PipelineShaderStageCreateInfo stageInfo(
        vk::PipelineShaderStageCreateFlags(), info.Stage, nullptr, info.Entry.c_str()
    );
    vk::ShaderModuleCreateInfo moduleInfo(vk::ShaderModuleCreateFlags(), shader.Code);
    auto cs = m_LogicalDevice.createShaderModuleUnique(moduleInfo);
    stageInfo.setModule(cs.get());

    vk::ComputePipelineCreateInfo computePipelineInfo;
    computePipelineInfo.setStage(stageInfo);
    computePipelineInfo.setLayout(layout);

    auto result = m_LogicalDevice.createComputePipeline(nullptr, computePipelineInfo);
    if (!result.has_value())
    {
        logger::error("Compute pipeline `{}` compilation failed", pipelineInfo.Name);
        m_Pipelines[pipelineIndex] = Pipeline();
        return false;
    }

    Application::GetInstance()->SetDebugName(layout, pipelineInfo.Name.c_str());
    Application::GetInstance()->SetDebugName(result.value, pipelineInfo.Name.c_str());

    logger::info("Successfully compiled compute pipeline `{}`", pipelineInfo.Name);
    m_Pipelines[pipelineIndex] =
        Pipeline(std::move(descriptorSetBuilder), std::move(pushConstants), layout, result.value);
    return true;
}

bool PipelineLibrary::CompilePipeline(GraphicsPipelineId id)
{
    const auto &[pipelineInfo, pipelineIndex] = m_GraphicsPipelineInfos[id];
    const auto &vertexInfo = m_ShaderLibrary->GetShaderInfo(pipelineInfo.VertexShaderId);
    const auto &fragmentInfo = m_ShaderLibrary->GetShaderInfo(pipelineInfo.FragmentShaderId);
    const auto &vertexShader = m_ShaderLibrary->GetShader(pipelineInfo.VertexShaderId);
    const auto &fragmentShader = m_ShaderLibrary->GetShader(pipelineInfo.FragmentShaderId);

    assert(vertexInfo.Stage == vk::ShaderStageFlagBits::eVertex);
    assert(fragmentInfo.Stage == vk::ShaderStageFlagBits::eFragment);

    if (!vertexShader.IsValid() || !fragmentShader.IsValid())
    {
        logger::error(
            "Cannot compile graphics pipeline `{}` because the shaders failed compilation", pipelineInfo.Name
        );
        m_Pipelines[pipelineIndex] = Pipeline();
        return false;
    }

    for (const auto &outputAttribute : vertexShader.Reflection.OutputAttributes)
        for (const auto &inputAttribute : fragmentShader.Reflection.InputAttributes)
            if (outputAttribute.Location == inputAttribute.Location &&
                outputAttribute.Format != inputAttribute.Format)
                logger::warn(
                    "In pipeline `{}` attribute at location {} in vertex shader has format {} but in "
                    "fragment shader has format {}",
                    pipelineInfo.Name, outputAttribute.Location, vk::to_string(outputAttribute.Format),
                    vk::to_string(inputAttribute.Format)
                );

    std::vector<vk::PushConstantRange> pushConstants;
    if (vertexShader.Reflection.HasPushConstants())
        pushConstants.push_back(vertexShader.Reflection.PushConstants);
    if (fragmentShader.Reflection.HasPushConstants())
        pushConstants.push_back(fragmentShader.Reflection.PushConstants);

    ReflectionData reflection;
    reflection.Combine(vertexShader.Reflection);
    reflection.Combine(fragmentShader.Reflection);

    // TODO: ArraySize Hint
    DescriptorSetBuilder descriptorSetBuilder(m_LogicalDevice);
    for (const auto &binding : reflection.SetLayoutBindings)
        descriptorSetBuilder.SetDescriptor(binding);

    auto descLayout = descriptorSetBuilder.CreateLayout();

    vk::PipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setSetLayouts(descLayout);
    layoutCreateInfo.setPushConstantRanges(pushConstants);

    auto layout = m_LogicalDevice.createPipelineLayout(layoutCreateInfo);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stageInfos = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), vertexInfo.Stage, nullptr, vertexInfo.Entry.c_str()
        ),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), fragmentInfo.Stage, nullptr, fragmentInfo.Entry.c_str()
        ),
    };

    vk::ShaderModuleCreateInfo vertexModuleInfo(vk::ShaderModuleCreateFlags(), vertexShader.Code);
    vk::ShaderModuleCreateInfo fragmentModuleInfo(vk::ShaderModuleCreateFlags(), fragmentShader.Code);

    auto vs = m_LogicalDevice.createShaderModuleUnique(vertexModuleInfo);
    auto fs = m_LogicalDevice.createShaderModuleUnique(fragmentModuleInfo);

    stageInfos[0].setModule(vs.get());
    stageInfos[1].setModule(fs.get());

    {
        const size_t required = vertexShader.Reflection.InputAttributes.size();
        const size_t provided = pipelineInfo.VertexInputs.size();
        if (required > provided)
            throw std::runtime_error(
                std::format(
                    "Vertex shader requires {} inputs but pipeline {} provided only {}", required,
                    pipelineInfo.Name, provided
                )
            );

        if (required < provided)
            logger::warn(
                "Pipeline {} provided {} inputs but vertex shader requires only {}", pipelineInfo.Name,
                required, provided
            );
    }

    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    for (const auto &attribute : vertexShader.Reflection.InputAttributes)
    {
        const auto &input = pipelineInfo.VertexInputs[attribute.Location];
        attributeDescriptions.emplace_back(attribute.Location, input.Binding, attribute.Format, input.Offset);
    }

    vk::PipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.setVertexAttributeDescriptions(attributeDescriptions);
    vertexInputState.setVertexBindingDescriptions(pipelineInfo.BindingDescriptions);

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setScissorCount(1);
    viewportState.setViewportCount(1);

    for ([[maybe_unused]] const auto &attribute : fragmentShader.Reflection.OutputAttributes)
        assert(attribute.Location < pipelineInfo.AttachmentBlendStates.size());

    vk::PipelineColorBlendStateCreateInfo blendState;
    blendState.setAttachments(pipelineInfo.AttachmentBlendStates);

    vk::PipelineRenderingCreateInfo renderingInfo(
        0, pipelineInfo.ColorAttachmentFormats, pipelineInfo.DepthAttachmentFormat,
        pipelineInfo.StencilAttachmentFormat
    );

    vk::PipelineDynamicStateCreateInfo dynamicState;
    std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport,
                                                      vk::DynamicState::eScissor };
    dynamicState.setDynamicStates(dynamicStates);

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo;
    graphicsPipelineInfo.setPVertexInputState(&vertexInputState);
    graphicsPipelineInfo.setPInputAssemblyState(&pipelineInfo.InputAssemblyState);
    graphicsPipelineInfo.setPMultisampleState(&pipelineInfo.MultisampleState);
    graphicsPipelineInfo.setPRasterizationState(&pipelineInfo.RasterizationState);
    graphicsPipelineInfo.setPDepthStencilState(&pipelineInfo.DepthStencilState);
    graphicsPipelineInfo.setPColorBlendState(&blendState);
    graphicsPipelineInfo.setPViewportState(&viewportState);
    graphicsPipelineInfo.setPDynamicState(&dynamicState);
    graphicsPipelineInfo.setStages(stageInfos);
    graphicsPipelineInfo.setLayout(layout);
    graphicsPipelineInfo.setPNext(renderingInfo);

    auto result = m_LogicalDevice.createGraphicsPipeline(nullptr, graphicsPipelineInfo);
    if (!result.has_value())
    {
        logger::error("Graphics pipeline `{}` compilation failed", pipelineInfo.Name);
        m_Pipelines[pipelineIndex] = Pipeline();
        return false;
    }

    Application::GetInstance()->SetDebugName(
        layout, std::format("Pipeline Layout `{}`", pipelineInfo.Name).c_str()
    );
    Application::GetInstance()->SetDebugName(result.value, pipelineInfo.Name.c_str());

    logger::info("Successfully compiled graphics pipeline `{}`", pipelineInfo.Name);
    m_Pipelines[pipelineIndex] =
        Pipeline(std::move(descriptorSetBuilder), std::move(pushConstants), layout, result.value);
    return true;
}

bool ReflectionData::HasPushConstants() const
{
    return PushConstants.size > 0;
}

void ReflectionData::Combine(const ReflectionData &other)
{
    for (uint32_t id : other.SpecializationConstantIds)
        if (!std::ranges::contains(SpecializationConstantIds, id))
            SpecializationConstantIds.push_back(id);

    for (const auto &binding : other.SetLayoutBindings)
    {
        auto it = std::ranges::find(SetLayoutBindings, binding.binding, [](const auto &binding) {
            return binding.binding;
        });

        if (it != SetLayoutBindings.end() && *it != binding)
            throw std::runtime_error(std::format("Binding {} mismatch", binding.binding));
        else
            SetLayoutBindings.push_back(binding);
    }
}

bool Shader::IsValid() const
{
    return !Code.empty();
}

bool Pipeline::IsValid() const
{
    return Handle != nullptr;
}

}