#pragma once

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <future>
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

class ErrorUserInterface final : public UserInterface
{
public:
    ErrorUserInterface(
        UserInterfaceVulkanSpec spec, const std::vector<std::string> &errors, const std::string &state
    );
    ~ErrorUserInterface() override = default;

    void OnEnter() override;
    void OnExit() override;

    void OnDefineUI(float timeStep) override;
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

    ErrorApplicationState(const ErrorApplicationState &) = delete;
    ErrorApplicationState &operator=(const ErrorApplicationState &) = delete;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;
    
    void OnUpdate(float timeStep) override;
    void OnRender() override;

public:
    static void AddToApplication(Application &application);
    static std::vector<std::string> &GetErrors();
    static void SetErrorState(const std::string &prevState);

    static bool ReloadShaders(const std::string &prevState);

public:
    inline static const std::string g_StateName = "REF Error State";

private:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;

    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;
    std::unique_ptr<ErrorUserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;

private:
    inline static std::vector<std::string> s_Errors;
    inline static std::string s_State;
};

class LoadingUserInterface final : public UserInterface
{
public:
    LoadingUserInterface(UserInterfaceVulkanSpec spec, const std::vector<std::string> &progressTexts);
    ~LoadingUserInterface() override = default;

    void OnEnter() override;
    void OnExit() override;

    void OnDefineUI(float timeStep) override;

    void SetProgress(size_t taskIndex, uint32_t total, uint32_t done);

private:
    const std::vector<std::string> &m_ProgressTexts;
    std::vector<std::pair<uint32_t, uint32_t>> m_Tasks;
};

class LoadingApplicationState : public ApplicationState
{
public:
    LoadingApplicationState(
        const ApplicationStateSpec &spec, const std::string &state,
        const std::vector<std::string> &progressTexts
    );
    ~LoadingApplicationState() override;

    LoadingApplicationState(const LoadingApplicationState &) = delete;
    LoadingApplicationState &operator=(const LoadingApplicationState &) = delete;

    void OnEnter(ApplicationState *previous) override;
    void OnExit(ApplicationState *next) override;

    void OnResize(const Swapchain *swapchain) override;

    void OnUpdate(float timeStep) override;
    void OnRender() override;

protected:
    vk::Device m_LogicalDevice;
    Queue m_MainQueue;

    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;
    std::unique_ptr<LoadingUserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;

    std::vector<std::pair<uint32_t, std::atomic<uint32_t>>> m_Tasks;

private:
    const std::string m_StateName = "REF Loading State";
    const std::vector<std::string> m_ProgressTexts;
};

class CompilingShadersApplicationState final : public LoadingApplicationState
{
public:
    CompilingShadersApplicationState(const ApplicationStateSpec &spec);
    ~CompilingShadersApplicationState() override = default;

    CompilingShadersApplicationState(const CompilingShadersApplicationState &) = delete;
    CompilingShadersApplicationState &operator=(const CompilingShadersApplicationState &) = delete;

    void OnEnter(ApplicationState *previous) override;

    void OnUpdate(float timeStep) override;

public:
    static void AddToApplication(Application &application, const std::string &nextState);
    static void SetNextState(const std::string &state);

public:
    static inline const std::string g_StateName = "REF Compiling Shaders State";

private:
    uint32_t m_ThreadCount = 1;
    bool m_PrevShadersCompiled = false;
    bool m_PrevPipelinesCompiled = false;
    std::promise<bool> m_ShaderCompilationPromise;
    std::promise<bool> m_CompilationPromise;

private:
    static inline std::string s_NextState = "No State";
};

}
