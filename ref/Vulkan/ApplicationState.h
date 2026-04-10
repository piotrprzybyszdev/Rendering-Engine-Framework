#pragma once

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "Core/UserInterface.h"
#include "Core/Window.h"

#include "Swapchain.h"

#include "Renderer/Renderer.h"
#include "Renderer/ShaderLibrary.h"

namespace ref::vulkan
{

class Application;

struct ApplicationStateSpec
{
    Window *ApplicationWindow;
    ref::vulkan::ShaderLibrary *ShaderLibrary;
    ref::vulkan::PipelineLibrary *PipelineLibrary;
    uint32_t ApiVersion;
    vk::Instance Instance;
    vk::detail::DispatchLoaderDynamic *DispatchLoader;
    vk::PhysicalDevice PhysicalDevice;
    vk::Device LogicalDevice;
    std::map<const char *, Queue> Queues;
    ref::vulkan::SwapchainBuilder *SwapchainBuilder;
};

class ApplicationState
{
public:
    ApplicationState() = default;
    virtual ~ApplicationState() = default;

    ApplicationState(const ApplicationState &) = delete;
    ApplicationState &operator=(const ApplicationState &) = delete;

    virtual void OnEnter([[maybe_unused]] ApplicationState *previous) {};
    virtual void OnExit([[maybe_unused]] ApplicationState *next) {};

    virtual void OnResize(const Swapchain *swapchain) = 0;

    virtual void OnUpdate(float timeStep) = 0;
    virtual void OnRender() = 0;
};

class ErrorUserInterfaceState final : public UserInterfaceState
{
public:
    ErrorUserInterfaceState(const std::vector<std::string> &errors, const std::string &state);
    ~ErrorUserInterfaceState() = default;

    void OnInit() override;
    void OnShutdown() override;

    void OnUpdate(float timeStep) override;
    virtual void OnKeyEvent(Key key, KeyAction action, Mods mods) override;

private:
    const std::vector<std::string> &m_Errors;
    const std::string &m_State;
};

class ErrorApplicationState final : public ApplicationState
{
public:
    ErrorApplicationState(const ApplicationStateSpec &spec);
    ~ErrorApplicationState() override;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;
    
    void OnUpdate(float timeStep) override;
    void OnRender() override;

public:
    static void AddToApplication(Application &application);
    static std::vector<std::string> &GetErrors();
    static void SetErrorState(const std::string &prevState);

public:
    inline static std::string g_StateName = "REF Error State";

private:
    vk::Device m_LogicalDevice;
    ResourceManagerSpec m_ResourceManagerSpec;
    Queue m_MainQueue;

    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;
    std::unique_ptr<ErrorUserInterfaceState> m_UserInterfaceState;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;

private:
    inline static std::vector<std::string> m_Errors;
    inline static std::string m_State;
};

class CompilingShadersUserInterfaceState final : public UserInterfaceState
{
public:
    CompilingShadersUserInterfaceState();
    ~CompilingShadersUserInterfaceState() = default;

    void OnInit() override;
    void OnShutdown() override;

    void OnUpdate(float timeStep) override;

    void SetProgress(uint32_t total, uint32_t done);

private:
    uint32_t m_Total;
    uint32_t m_Done;
};

class CompilingShadersApplicationState final : public ApplicationState
{
public:
    CompilingShadersApplicationState(const ApplicationStateSpec &spec);
    ~CompilingShadersApplicationState() override;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

public:
    static void AddToApplication(Application &application, const std::string &nextState);
    static void SetNextState(const std::string &state);

public:
    inline static std::string g_StateName = "REF Compiling Shaders State";

private:
    vk::Device m_LogicalDevice;
    ResourceManagerSpec m_ResourceManagerSpec;
    Queue m_MainQueue;

    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;
    std::unique_ptr<CompilingShadersUserInterfaceState> m_UserInterfaceState;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;

    uint32_t m_Total = 0;
    std::atomic<uint32_t> m_Done = 0;

private:
    inline static std::vector<std::string> m_Errors;
    inline static std::string m_NextState = "No State";
};

}
