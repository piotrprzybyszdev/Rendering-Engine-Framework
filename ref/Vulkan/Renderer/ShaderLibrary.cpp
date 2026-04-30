#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <system_error>
#include <thread>

#include "Core/Core.h"
#include "Core/Serialization.h"

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
    case vk::ShaderStageFlagBits::eGeometry:
        return shaderc_shader_kind::shaderc_geometry_shader;
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
    if (type == Type::UInt && components == 1)
        return vk::Format::eR32Uint;
    if (type == Type::UInt && components == 2)
        return vk::Format::eR32G32Uint;
    if (type == Type::UInt && components == 3)
        return vk::Format::eR32G32B32Uint;
    if (type == Type::UInt && components == 4)
        return vk::Format::eR32G32B32A32Uint;

    throw std::runtime_error("Unsupported base type or component count");
}

struct FileInfo
{
    std::filesystem::file_time_type UpdateTime;
    std::string Path;
    size_t ContentSize;
    std::unique_ptr<char[]> Content;
};

FileInfo ReadFileInfo(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error(std::format("Shader file {} cannot be opened", path.string()));

    size_t size = file.tellg();
    file.seekg(0);

    auto data = std::make_unique<char[]>(size);
    file.read(data.get(), size);

    return FileInfo {
        .UpdateTime = std::filesystem::last_write_time(path),
        .Path = path.string(),
        .ContentSize = size,
        .Content = std::move(data),
    };
}

struct IncludeInfo
{
    std::set<std::filesystem::path> IncludedFiles;
    std::filesystem::file_time_type MaxUpdateTime = std::filesystem::file_time_type::min();
};

class Includer : public shaderc::CompileOptions::IncluderInterface
{
public:
    shaderc_include_result *GetInclude(
        const char *requested_source, shaderc_include_type type, const char *requesting_source,
        size_t include_depth
    ) override;

    void ReleaseInclude(shaderc_include_result *data) override;

    IncludeInfo ClearIncludeInfo();

private:
    const uint32_t m_MaxIncludeDepth = 5;
    std::set<std::filesystem::path> m_SystemIncludePaths;
    IncludeInfo m_IncludeInfo;

private:
    std::optional<std::filesystem::path> GetFilePath(
        const char *requested_source, shaderc_include_type type, const char *requesting_source
    );
};

shaderc_include_result *Includer::GetInclude(
    const char *requested_source, shaderc_include_type type, const char *requesting_source,
    size_t include_depth
)
{
    if (include_depth > m_MaxIncludeDepth)
    {
        FileInfo *fileInfo = new FileInfo();
        fileInfo->Path = std::format("MaxIncludeDepth exceeded {}/{}", include_depth, m_MaxIncludeDepth);
        logger::warn(fileInfo->Path);
        return new shaderc_include_result {
            "", 0, fileInfo->Path.c_str(), fileInfo->Path.size(), fileInfo,
        };
    }

    auto filePath = GetFilePath(requested_source, type, requesting_source);

    if (!filePath.has_value())
    {
        std::string include = type == shaderc_include_type_standard ? std::format("<{}>", requested_source)
                                                                    : std::format("\"{}\"", requested_source);
        FileInfo *fileInfo = new FileInfo();
        fileInfo->Path = std::format("File not found when including {}", include);
        return new shaderc_include_result {
            "", 0, fileInfo->Path.c_str(), fileInfo->Path.size(), fileInfo,
        };
    }

    const auto &path = filePath.value();

    if (m_IncludeInfo.IncludedFiles.contains(path))
    {
        // File was already included - return empty file as to not include it twice (#pragma once)
        FileInfo *fileInfo = new FileInfo();
        fileInfo->Path = path.string();
        return new shaderc_include_result {
            fileInfo->Path.c_str(), fileInfo->Path.size(), nullptr, 0, fileInfo,
        };
    }

    FileInfo *fileInfo = new FileInfo();
    *fileInfo = ReadFileInfo(path);

    m_IncludeInfo.IncludedFiles.insert(path);
    m_IncludeInfo.MaxUpdateTime = std::max(m_IncludeInfo.MaxUpdateTime, fileInfo->UpdateTime);

    return new shaderc_include_result { fileInfo->Path.c_str(), fileInfo->Path.size(),
                                        fileInfo->Content.get(), fileInfo->ContentSize, nullptr };
}

void Includer::ReleaseInclude(shaderc_include_result *data)
{
    delete static_cast<FileInfo *>(data->user_data);
    delete data;
}

IncludeInfo Includer::ClearIncludeInfo()
{
    return std::move(m_IncludeInfo);
}

std::optional<std::filesystem::path> Includer::GetFilePath(
    const char *requested_source, shaderc_include_type type, const char *requesting_source
)
{
    std::filesystem::path requested(requested_source);
    std::filesystem::path requesting(requesting_source);

    switch (type)
    {
    case shaderc_include_type_relative:
    {
        std::filesystem::path relativePath = requesting.remove_filename() / requested;
        if (std::filesystem::is_regular_file(relativePath))
            return relativePath;
        break;
    }
    case shaderc_include_type_standard:
    {
        for (const std::filesystem::path &path : m_SystemIncludePaths)
        {
            std::filesystem::path absolutePath = path / requested;
            if (std::filesystem::is_regular_file(absolutePath))
                return absolutePath;
        }
        break;
    }
    default:
        std::terminate();
    }

    return std::optional<std::filesystem::path>();
}

struct ShaderCacheHeader
{
    static inline constexpr uint32_t g_Magic = serialization::MakeMagic("rsch");
    static inline constexpr uint32_t g_Version = serialization::MakeVersion(0u, 0u, 2u, 0u);
    static inline constexpr uint64_t g_ItemAlignment = 16;

    uint32_t Magic;
    uint32_t Version;
    struct ReflectionInfo
    {
        uint32_t PushConstantRangeCount;
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
    uint32_t IncludeCount;
    std::filesystem::file_time_type UpdateTime;
};

void SerializeShader(const std::filesystem::path &path, const Shader &shader)
{
    assert(shader.IsValid());
    logger::trace("Writing cache for shader {}", path.string());

    auto addOffsetVector = [&](uint64_t &offset, const auto &data) {
        serialization::AddOffset(offset, ShaderCacheHeader::g_ItemAlignment, std::span(data));
    };

    uint64_t offset = 0;
    serialization::AddOffset<ShaderCacheHeader>(offset, ShaderCacheHeader::g_ItemAlignment);
    addOffsetVector(offset, shader.Reflection.PushConstantRanges);
    addOffsetVector(offset, shader.Reflection.SpecializationConstantIds);
    addOffsetVector(offset, shader.Reflection.SetLayoutBindings);
    addOffsetVector(offset, shader.Reflection.InputAttributes);
    addOffsetVector(offset, shader.Reflection.OutputAttributes);

    ShaderCacheHeader header = {
        .Magic = ShaderCacheHeader::g_Magic,
        .Version = ShaderCacheHeader::g_Version,
        .Reflection = {
            .PushConstantRangeCount = static_cast<uint32_t>(shader.Reflection.PushConstantRanges.size()),
            .SpecilizationConstantCount = static_cast<uint32_t>(shader.Reflection.SpecializationConstantIds.size()),
            .BindingCount = static_cast<uint32_t>(shader.Reflection.SetLayoutBindings.size()),
            .InputAttributeCount = static_cast<uint32_t>(shader.Reflection.InputAttributes.size()),
            .OutputAttributeCount = static_cast<uint32_t>(shader.Reflection.OutputAttributes.size()),
        },
        .Code = {
            .Offset = static_cast<uint32_t>(offset),
            .Size = static_cast<uint32_t>(shader.Code.size()),
        },
        .IncludeCount = static_cast<uint32_t>(shader.Includes.size()),
        .UpdateTime = shader.UpdateTime,
    };

    std::ofstream file(path, std::ios::binary | std::ios::out);
    assert(file.is_open());

    auto serializeVector = [&](const auto &data) {
        serialization::Serialize(file, ShaderCacheHeader::g_ItemAlignment, std::span(data));
    };

    serialization::Serialize(file, ShaderCacheHeader::g_ItemAlignment, header);
    serializeVector(shader.Reflection.PushConstantRanges);
    serializeVector(shader.Reflection.SpecializationConstantIds);
    serializeVector(shader.Reflection.SetLayoutBindings);
    serializeVector(shader.Reflection.InputAttributes);
    serializeVector(shader.Reflection.OutputAttributes);
    assert(file.tellp() == header.Code.Offset);
    serialization::Serialize(file, ShaderCacheHeader::g_ItemAlignment, std::span(shader.Code));
    for (const auto &include : shader.Includes)
        file << include;

    logger::info("Successfully written cache for shader `{}`", path.string());
}

Shader DeserializeShader(const std::filesystem::path &path)
{
    logger::trace("Reading shader {} from cache", path.string());

    std::ifstream file(path, std::ios::binary | std::ios::in);
    assert(file.is_open());

    ShaderCacheHeader header = {};
    serialization::Deserialize(file, ShaderCacheHeader::g_ItemAlignment, header);
    assert(header.Magic == ShaderCacheHeader::g_Magic);
    assert(header.Version == ShaderCacheHeader::g_Version);

    auto deserializeVector = [&](const size_t count, auto &data) {
        serialization::Deserialize(file, ShaderCacheHeader::g_ItemAlignment, count, data);
    };

    Shader shader;
    deserializeVector(header.Reflection.PushConstantRangeCount, shader.Reflection.PushConstantRanges);
    deserializeVector(
        header.Reflection.SpecilizationConstantCount, shader.Reflection.SpecializationConstantIds
    );
    deserializeVector(header.Reflection.BindingCount, shader.Reflection.SetLayoutBindings);
    deserializeVector(header.Reflection.InputAttributeCount, shader.Reflection.InputAttributes);
    deserializeVector(header.Reflection.OutputAttributeCount, shader.Reflection.OutputAttributes);
    assert(header.Code.Offset == file.tellg());
    serialization::Deserialize(file, ShaderCacheHeader::g_ItemAlignment, header.Code.Size, shader.Code);
    shader.UpdateTime = header.UpdateTime;
    for (uint32_t i = 0; i < header.IncludeCount; i++)
    {
        std::filesystem::path include;
        file >> include;
        shader.Includes.insert(std::move(include));
    }

    file.ignore();
    assert(file.eof());

    logger::info("Successfully read cache for shader `{}`", path.string());
    return shader;
}

}

ShaderLibrary::ShaderLibrary(vk::Device logicalDevice, uint32_t apiVersion)
    : m_LogicalDevice(logicalDevice), m_ApiVersion(apiVersion)
{
}

void ShaderLibrary::SetShaderCachePath(const std::filesystem::path &path)
{
    m_ShaderCachePath = path;
}

void ShaderLibrary::PruneShaderCache()
{
    [[maybe_unused]] std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(m_ShaderCachePath, error))
    {
        if (!entry.is_regular_file())
            continue;

        auto it = std::ranges::find_if(m_ShaderInfos, [&entry](const auto &info) {
            return info.Path.filename().concat(".rsc") == entry.path().filename();
        });

        if (it != m_ShaderInfos.end())
            continue;

        if (std::filesystem::remove(entry.path()))
            logger::info("Removing cache for shader `{}`", entry.path().string());
    }
}

void ShaderLibrary::AddShadersFromDirectory(const std::filesystem::path &path)
{
    auto extensionToStage =
        [](const std::filesystem::path &extension) -> std::optional<vk::ShaderStageFlagBits> {
        if (extension == ".comp")
            return vk::ShaderStageFlagBits::eCompute;
        if (extension == ".frag")
            return vk::ShaderStageFlagBits::eFragment;
        if (extension == ".vert")
            return vk::ShaderStageFlagBits::eVertex;
        if (extension == ".geom")
            return vk::ShaderStageFlagBits::eGeometry;
        return std::nullopt;
    };

    const auto opts = std::filesystem::directory_options::follow_directory_symlink;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(path, opts))
    {
        if (!entry.is_regular_file())
            continue;

        auto stage = extensionToStage(entry.path().extension());
        if (!stage.has_value())
            continue;

        AddShader(ShaderInfo(entry.path(), "main", stage.value()));
    }
}

ShaderId ShaderLibrary::GetShaderByPath(const std::filesystem::path &path)
{
    auto it = std::ranges::find_if(m_ShaderInfos, [&path](const auto &info) { return info.Path == path; });
    if (it != m_ShaderInfos.end())
        return ShaderId(std::distance(m_ShaderInfos.begin(), it));

    return ShaderId();
}

ShaderId ShaderLibrary::AddShader(ShaderInfo info)
{
    {
        auto it = std::ranges::find_if(m_ShaderInfos, [&info](const auto &other) {
            return info.Path.filename() == other.Path.filename();
        });
        if (it != m_ShaderInfos.end())
            throw std::runtime_error(
                std::format(
                    "Two shaders with the same filename `{}` are not allowed", info.Path.filename().string()
                )
            );
    }

    auto it = std::ranges::find_if(m_ShaderInfos, [&info](const auto &other) { return info == other; });

    if (it != m_ShaderInfos.end())
        return ShaderId(std::distance(m_ShaderInfos.begin(), it));

    m_ShaderInfos.push_back(std::move(info));
    m_Shaders.push_back(Shader());
    return ShaderId(m_ShaderInfos.size() - 1);
}

void ShaderLibrary::LoadShader(ShaderId id)
{
    const ShaderInfo &info = m_ShaderInfos[id];
    logger::debug("Loading Shader {}", info.Path.string());

    const auto updateTime = GetUpdateTime(id);
    if (updateTime <= m_Shaders[id].UpdateTime)
    {
        logger::debug("Shader {} is already up to date", info.Path.string());
        return;
    }

    const std::filesystem::path cache = GetShaderCachePath(info.Path);
    if (updateTime < GetLastWriteTimeOrMin(cache))
        m_Shaders[id] = DeserializeShader(cache);

    // The fact that the shader cache was written after the shader was last modified
    // does not mean that the cache is up to date but it's a good heuristic initially
    if (m_Shaders[id].UpdateTime < updateTime)
        CompileShader(id);
}

void ShaderLibrary::LoadShaders()
{
    for (size_t id = 0; id < m_ShaderInfos.size(); id++)
        LoadShader(ShaderId(id));
}

void ShaderLibrary::LoadShadersAsync(uint32_t &total, std::atomic<uint32_t> &done, uint32_t threadCount)
{
    total = static_cast<uint32_t>(m_ShaderInfos.size());

    std::thread mainThread([threadCount, &done, this]() {
        std::atomic<uint32_t> index = 0;
        std::vector<std::thread> threads;

        for (uint32_t i = 0; i < threadCount; i++)
        {
            threads.push_back(std::thread([threadCount, &done, &index, this]() {
                uint32_t id;
                while ((id = index++) < m_ShaderInfos.size())
                {
                    LoadShader(ShaderId(id));
                    done++;
                }
            }));
        }

        for (auto &thread : threads)
            thread.join();
    });
    mainThread.detach();
}

void ShaderLibrary::WriteShaderCaches()
{
    std::filesystem::create_directories(m_ShaderCachePath);

    for (size_t id = 0; id < m_ShaderInfos.size(); id++)
    {
        const ShaderInfo &info = m_ShaderInfos[id];
        const Shader &shader = m_Shaders[id];
        std::filesystem::path cache = GetShaderCachePath(info.Path);
        const auto updateTime = GetUpdateTime(ShaderId(id));
        if (m_Shaders[id].IsValid() && updateTime <= shader.UpdateTime &&
            shader.UpdateTime > GetLastWriteTimeOrMin(cache))
            SerializeShader(cache, shader);
    }
}

void ShaderLibrary::CompileShader(ShaderId id)
{
    const auto &shaderInfo = m_ShaderInfos[id];

    FileInfo info = ReadFileInfo(shaderInfo.Path);
    logger::debug("Compiling shader {}", shaderInfo.Path.string());

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, m_ApiVersion);
    Includer *includer = new Includer();
    options.SetIncluder(std::unique_ptr<Includer>(includer));

    const std::string path = shaderInfo.Path.string();
    auto result = compiler.CompileGlslToSpv(
        info.Content.get(), info.ContentSize, ToShaderKind(shaderInfo.Stage), path.c_str(),
        shaderInfo.Entry.c_str(), options
    );

    IncludeInfo includeInfo = includer->ClearIncludeInfo();
    const auto updateTime = std::max(includeInfo.MaxUpdateTime, info.UpdateTime);

    if (result.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success)
    {
        m_Shaders[id] = {
            .UpdateTime = updateTime,
            .CompilationError = result.GetErrorMessage(),
        };
        logger::error("Shader `{}` failed compilation", shaderInfo.Path.string());
        logger::error("{}", result.GetErrorMessage());
        return;
    }

    const auto spv = std::span(result);

    logger::info("Successfully compiled shader `{}`", shaderInfo.Path.string());
    m_Shaders[id] = {
        .Code = std::vector(spv.begin(), spv.end()),
        .Reflection = ReflectShader(spv, shaderInfo.Stage),
        .Includes = std::move(includeInfo.IncludedFiles),
        .UpdateTime = updateTime,
    };
}

const ShaderInfo &ShaderLibrary::GetShaderInfo(ShaderId id) const
{
    return m_ShaderInfos[id];
}

const Shader &ShaderLibrary::GetShader(ShaderId id) const
{
    return m_Shaders[id];
}

std::filesystem::file_time_type ShaderLibrary::GetUpdateTime(ShaderId id) const
{
    auto time = GetLastWriteTimeOrMax(m_ShaderInfos[id].Path);
    if (m_Shaders[id].Includes.empty())
        return time;

    auto proj = std::views::transform([this](const auto &p) { return GetLastWriteTimeOrMax(p); });
    return std::max(time, std::ranges::max(m_Shaders[id].Includes | proj));
}

std::filesystem::file_time_type ShaderLibrary::GetLastWriteTimeOrMin(const std::filesystem::path &path) const
{
    [[maybe_unused]] std::error_code error;
    return std::filesystem::last_write_time(path, error);
}

std::filesystem::file_time_type ShaderLibrary::GetLastWriteTimeOrMax(const std::filesystem::path &path) const
{
    [[maybe_unused]] std::error_code error;
    auto time = std::filesystem::last_write_time(path, error);
    if (time == std::filesystem::file_time_type::min())
        return std::filesystem::file_time_type::min();
    return time;
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

        reflection.PushConstantRanges = { { stage, offset, size } };
    }

    auto specializationConstants = compiler.get_specialization_constants();
    for (const auto &specialization : specializationConstants)
        reflection.SpecializationConstantIds.push_back(specialization.constant_id);

    return reflection;
}

std::filesystem::path ShaderLibrary::GetShaderCachePath(const std::filesystem::path &path)
{
    return m_ShaderCachePath / path.filename().concat(".rsc");
}

void ReflectionData::Combine(const ReflectionData &other)
{
    for (uint32_t id : other.SpecializationConstantIds)
        if (!std::ranges::contains(SpecializationConstantIds, id))
            SpecializationConstantIds.push_back(id);

    for (const auto &range : other.PushConstantRanges)
        PushConstantRanges.push_back(range);

    for (const auto &binding : other.SetLayoutBindings)
    {
        auto it = std::ranges::find(SetLayoutBindings, binding.binding, [](const auto &binding) {
            return binding.binding;
        });

        if (it != SetLayoutBindings.end() && *it != binding)
        {
            if (it->descriptorCount != binding.descriptorCount ||
                it->descriptorType != binding.descriptorType)
                throw std::runtime_error(std::format("Binding {} mismatch", binding.binding));
            it->stageFlags |= binding.stageFlags;
        }
        else
            SetLayoutBindings.push_back(binding);
    }
}

bool Shader::IsValid() const
{
    return !Code.empty();
}

}