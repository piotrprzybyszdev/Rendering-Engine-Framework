#pragma once

#include <vulkan/vulkan.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "Core/Window.h"

#include "ApplicationState.h"
#include "Swapchain.h"
#include "Renderer/Renderer.h"

namespace ref::vulkan
{

struct ApplicationSpec
{
    std::unique_ptr<Window> ApplicationWindow;
    vk::SurfaceKHR Surface;
    uint32_t ApiVersion;
    vk::Instance Instance;
    std::optional<vk::DebugUtilsMessengerEXT> DebugMessenger;
    vk::detail::DispatchLoaderDynamic DispatchLoader;
    vk::PhysicalDevice PhysicalDevice;
    vk::Device LogicalDevice;
    std::map<const char *, Queue> Queues;
};

class Application
{
public:
    static inline constexpr const char *MainQueueName = "Ref Main Queue";
    static inline constexpr const char *AsyncComputeQueueName = "Ref Async Compute Queue";
    static inline constexpr const char *AsyncTransferQueueName = "Ref Async Transfer Queue";

public:
    static Application *GetInstance();

public:
    Application(ApplicationSpec &&spec);
    ~Application();

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    ShaderLibrary &GetShaderLibrary();
    PipelineLibrary &GetPipelineLibrary();
    const ApplicationStateSpec &GetApplicationStateSpec();

    void Run(const std::string &state);

    void AddState(const std::string &name, std::unique_ptr<ApplicationState> state);
    void SetNextState(const std::string &name);

    void SetRecreateSwapchain();

    template<typename T> requires vk::isVulkanHandleType<T>::value
    void SetDebugName(T handle, const char *name);

    template<typename S, typename... Args> requires std::is_base_of_v<ApplicationState, S>
    void AddAndCreateState(const std::string &name, Args&&...args);

private:
    static inline Application *s_Instance = nullptr;

private:
    std::unique_ptr<Window> m_Window;
    vk::SurfaceKHR m_Surface;

    std::unique_ptr<ShaderLibrary> m_ShaderLibrary;
    std::unique_ptr<PipelineLibrary> m_PipelineLibrary;

    uint32_t m_ApiVersion;
    vk::Instance m_Instance;
    std::optional<vk::DebugUtilsMessengerEXT> m_DebugMessenger;
    vk::detail::DispatchLoaderDynamic m_DispatchLoader;

    vk::PhysicalDevice m_PhysicalDevice;
    vk::Device m_LogicalDevice;

    Queue m_MainQueue;
    
    bool m_RecreateSwapchain = false;
    SwapchainBuilder m_SwapchainBuilder;
    std::unique_ptr<Swapchain> m_Swapchain;

    ApplicationStateSpec m_ApplicationStateSpec;

    std::map<std::string, std::unique_ptr<ApplicationState>> m_States;
    ApplicationState *m_CurrentState = nullptr;
    std::string m_CurrentStateName = "No State";
    ApplicationState *m_NextState = nullptr;
    std::string m_NextStateName = "No State";
};

template<typename T> requires vk::isVulkanHandleType<T>::value
inline void Application::SetDebugName(T handle, const char *name)
{
    if (!m_DebugMessenger.has_value())
        return;

    m_LogicalDevice.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT(
            T::objectType, reinterpret_cast<uint64_t>(static_cast<T::CType>(handle)), name
        ),
        m_DispatchLoader
    );
}

template<typename S, typename... Args> requires std::is_base_of_v<ApplicationState, S>
inline void Application::AddAndCreateState(const std::string &name, Args &&...args)
{
    m_States[name] = std::make_unique<S>(m_ApplicationStateSpec, std::forward<Args>(args)...);
}

}