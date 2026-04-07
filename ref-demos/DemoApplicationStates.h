#pragma once

#include <vulkan/vulkan.hpp>

#include "Core/UserInterface.h"

#include "Vulkan/Renderer/Renderer.h"
#include "Vulkan/Renderer/ResourceManager.h"
#include "Vulkan/Renderer/ShaderLibrary.h"

#include <string>
#include <set>

namespace ref::vulkan
{

class DemoUserInterfaceState : public UserInterfaceState
{
public:
    DemoUserInterfaceState(const std::string &state);
    ~DemoUserInterfaceState() override = default;

    void OnInit() override;
    void OnShutdown() override;

    void OnUpdate(float timeStep) override;
    void OnKeyRelease(Key key) override;

private:
    inline static std::set<std::string> s_DemoStates;
};

class ComputeApplicationState final : public ApplicationState
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
    Queue m_MainQueue;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;

    ShaderId m_ShaderId;
    ComputePipelineId m_PipelineId;

    float m_Time = 0.0f;

    ShaderLibrary *m_ShaderLibrary;
    PipelineLibrary *m_PipelineLibrary;
    SwapchainBuilder *m_SwapchainBuilder;
};

class TriangleApplicationState final : public ApplicationState
{
public:
    TriangleApplicationState(const ApplicationStateSpec &spec);

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    Queue m_MainQueue;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;
};

class ParticleApplicationState final : public ApplicationState
{
public:
    ParticleApplicationState(const ApplicationStateSpec &spec);

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    Queue m_MainQueue;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;

    float m_Time = 0.0f;
    uint32_t m_ParticleIdx = 0;
};

class CubeApplicationState final : public ApplicationState
{
public:
    CubeApplicationState(const ApplicationStateSpec &spec);

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

private:
    Queue m_MainQueue;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;

    struct MatrixBuffer
    {
        std::array<std::array<float, 4>, 4> CameraProjection;
        std::array<std::array<float, 4>, 4> CameraView;
        std::array<std::array<float, 4>, 4> ModelTransform;
    } m_Matrices, m_MirrorMatrices, m_ReflectionMatrices;

    float m_CubeAngle = 0.0f;
};

class SwapchainUserInterfaceState final : public DemoUserInterfaceState
{
public:
    SwapchainUserInterfaceState(SwapchainBuilder *swapchainBuilder, vk::Format format, uint32_t imageCount);
    ~SwapchainUserInterfaceState() override = default;

    void OnInit() override;
    void OnShutdown() override;

    void OnUpdate(float timeStep) override;
    void OnKeyRelease(Key key) override;

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
    Queue m_MainQueue;
    SwapchainUserInterfaceState *m_UserInterfaceState;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;

    SwapchainBuilder *m_SwapchainBuilder;
};

}