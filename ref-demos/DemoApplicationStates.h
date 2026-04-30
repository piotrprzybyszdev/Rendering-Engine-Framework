#pragma once

#include <vulkan/vulkan.hpp>

#include <Core/UserInterface.h>

#include <Vulkan/Renderer/Renderer.h>
#include <Vulkan/Renderer/ResourceManager.h>
#include <Vulkan/Renderer/ShaderLibrary.h>

#include <set>
#include <string>

namespace ref::vulkan
{

class DemoUserInterface : public UserInterface
{
public:
    DemoUserInterface(UserInterfaceVulkanSpec spec);
    ~DemoUserInterface() override = default;

    void OnDefineUI(float timeStep) override;

    static void AddState(const std::string &name);

private:
    inline static std::set<std::string> s_DemoStates;
};

class DemoApplicationState : public ApplicationState
{
public:
    DemoApplicationState(const std::string &state);
    ~DemoApplicationState() override = default;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

protected:
    void SetDefaultSwapchain();
    void SetSwapchainFormat(vk::SurfaceFormatKHR format);
    void SetSwapchainUsageFlags(vk::ImageUsageFlags usageFlags);
    void SetSwapchainImageCount(uint32_t imageCount);

    bool EnsurePipelinesCompiled(std::initializer_list<ComputePipelineInstanceId> pipelines);
    bool EnsurePipelinesCompiled(std::initializer_list<GraphicsPipelineInstanceId> pipelines);

protected:
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;
    std::unique_ptr<DemoUserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;

    std::string m_State;
    vk::SurfaceFormatKHR m_SwapchainFormat;
    vk::ImageUsageFlags m_SwapchainUsageFlags;
    uint32_t m_SwapchainImageCount = 2;

private:
    void SetShaderErrors();
};

class ComputeApplicationState final : public DemoApplicationState
{
public:
    ComputeApplicationState(const ApplicationStateSpec &spec);
    ~ComputeApplicationState() override;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;

    ShaderId m_ShaderId;
    ComputePipelineInstanceId m_PipelineId;

    float m_Time = 0.0f;

    ShaderLibrary *m_ShaderLibrary;
    PipelineLibrary *m_PipelineLibrary;
};

class TriangleApplicationState final : public DemoApplicationState
{
public:
    TriangleApplicationState(const ApplicationStateSpec &spec);

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;
    PipelineLibrary *m_PipelineLibrary;

    GraphicsPipelineInstanceId m_TrianglePiplineId;
};

class ParticleApplicationState final : public DemoApplicationState
{
public:
    ParticleApplicationState(const ApplicationStateSpec &spec);

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;
    PipelineLibrary *m_PipelineLibrary;

    ComputePipelineInstanceId m_ComputePipelineId;
    GraphicsPipelineInstanceId m_ParticlePipelineId;

    float m_Time = 0.0f;
    uint32_t m_ParticleIdx = 0;
};

class CubeApplicationState final : public DemoApplicationState
{
public:
    CubeApplicationState(const ApplicationStateSpec &spec);

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;
    PipelineLibrary *m_PipelineLibrary;

    GraphicsPipelineInstanceId m_CubePipelineId;
    GraphicsPipelineInstanceId m_MirrorPipelineId;
    GraphicsPipelineInstanceId m_ReflectionPipelineId;

    struct MatrixBuffer
    {
        std::array<std::array<float, 4>, 4> CameraProjection;
        std::array<std::array<float, 4>, 4> CameraView;
        std::array<std::array<float, 4>, 4> ModelTransform;
    } m_Matrices, m_MirrorMatrices, m_ReflectionMatrices;

    float m_CubeAngle = 0.0f;
};

class SwapchainUserInterface final : public DemoUserInterface
{
public:
    SwapchainUserInterface(UserInterfaceVulkanSpec spec, SwapchainBuilder *swapchainBuilder, vk::Format format, uint32_t imageCount);
    ~SwapchainUserInterface() override = default;

    void OnDefineUI(float timeStep) override;

    vk::Format GetFormat() const;
    uint32_t GetImageCount() const;

private:
    SwapchainBuilder *m_SwapchainBuilder;

    vk::Format m_CurrentFormat;
    uint32_t m_ImageCount = 2;
};

class SwapchainApplicationState final : public ApplicationState
{
public:
    SwapchainApplicationState(const ApplicationStateSpec &spec);
    ~SwapchainApplicationState() override;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;
    PipelineLibrary *m_PipelineLibrary;

    std::unique_ptr<SwapchainUserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;

    SwapchainBuilder *m_SwapchainBuilder;
};

}