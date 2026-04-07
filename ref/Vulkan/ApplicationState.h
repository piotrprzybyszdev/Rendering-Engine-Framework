#pragma once

#include <vulkan/vulkan.hpp>

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

    virtual void OnEnter(ApplicationState *previous);
    virtual void OnExit(ApplicationState *next);

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
    void OnKeyRelease(Key key) override;

private:
    const std::vector<std::string> &m_Errors;
    const std::string &m_State;
};

class ErrorApplicationState final : public ApplicationState
{
public:
    ErrorApplicationState(const ApplicationStateSpec &spec);
    ~ErrorApplicationState() override;

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
    Queue m_MainQueue;
    std::unique_ptr<UserInterface> m_UserInterface;
    std::unique_ptr<FrameGraph> m_FrameGraph;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<ResourceAllocator> m_ResourceAllocator;

private:
    inline static std::vector<std::string> m_Errors;
    inline static std::string m_State;
};

}
