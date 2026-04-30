#pragma once

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <filesystem>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "Core/Core.h"

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
    std::vector<vk::PushConstantRange> PushConstantRanges;
    std::vector<uint32_t> SpecializationConstantIds;
    std::vector<vk::DescriptorSetLayoutBinding> SetLayoutBindings;

    struct Attribute
    {
        uint32_t Location;
        vk::Format Format;
    };

    std::vector<Attribute> InputAttributes;
    std::vector<Attribute> OutputAttributes;

    void Combine(const ReflectionData &other);
};

struct Shader
{
    std::vector<uint32_t> Code;
    ReflectionData Reflection;
    std::set<std::filesystem::path> Includes;
    std::filesystem::file_time_type UpdateTime = std::filesystem::file_time_type::min();

    std::optional<std::string> CompilationError;

    bool IsValid() const;
};

using ShaderId = IdType<size_t, Shader>;

class ShaderLibrary
{
public:
    ShaderLibrary(vk::Device logicalDevice, uint32_t apiVersion);

    ShaderLibrary(const ShaderLibrary &) = delete;
    ShaderLibrary &operator=(const ShaderLibrary &) = delete;

    void SetShaderCachePath(const std::filesystem::path &path);
    void PruneShaderCache();

    void AddShadersFromDirectory(const std::filesystem::path &path);
    ShaderId GetShaderByPath(const std::filesystem::path &path);

    ShaderId AddShader(ShaderInfo info);

    void LoadShader(ShaderId id);
    void LoadShaders();
    void LoadShadersAsync(uint32_t &total, std::atomic<uint32_t> &done, uint32_t threadCount);

    void WriteShaderCaches();

    const ShaderInfo &GetShaderInfo(ShaderId id) const;
    const Shader &GetShader(ShaderId id) const;

    auto GetCompilationErrors() const;

private:
    vk::Device m_LogicalDevice;
    const uint32_t m_ApiVersion;
    std::filesystem::path m_ShaderCachePath;

    std::vector<ShaderInfo> m_ShaderInfos;
    std::vector<Shader> m_Shaders;

private:
    ReflectionData ReflectShader(std::span<const uint32_t> spirv, vk::ShaderStageFlagBits stage);
    void CompileShader(ShaderId id);

    std::filesystem::path GetShaderCachePath(const std::filesystem::path &path);
    std::filesystem::file_time_type GetUpdateTime(ShaderId id) const;
    std::filesystem::file_time_type GetLastWriteTimeOrMin(const std::filesystem::path &path) const;
    std::filesystem::file_time_type GetLastWriteTimeOrMax(const std::filesystem::path &path) const;
};

inline auto ShaderLibrary::GetCompilationErrors() const
{
    return m_Shaders |
           std::views::filter([](const auto &shader) { return shader.CompilationError.has_value(); }) |
           std::views::transform([](const auto &shader) { return shader.CompilationError.value(); });
}

}
