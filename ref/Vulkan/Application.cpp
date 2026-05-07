#include <chrono>
#include <thread>

#include "Core/Core.h"

#include "Application.h"

namespace ref::vulkan
{

Application *Application::GetInstance()
{
    return s_Instance;
}

Application::Application(ApplicationSpec &&spec)
    : m_Window(std::move(spec.ApplicationWindow)), m_Surface(spec.Surface),
      m_ApiVersion(spec.ApiVersion), m_Instance(spec.Instance),
      m_DebugMessenger(spec.DebugMessenger),
      m_DispatchLoader(spec.DispatchLoader), m_PhysicalDevice(spec.PhysicalDevice),
      m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.Queues.at(MainQueueName)),
      m_SwapchainBuilder(spec.PhysicalDevice, spec.Surface)
{
    if (s_Instance != nullptr)
        throw std::runtime_error("Only one instance of the application must exist");

    s_Instance = this;

    for (const auto &[name, queue] : spec.Queues)
        SetDebugName(queue.Handle, name);

    m_SwapchainBuilder.SetPresentQueueFamilies({ m_MainQueue.FamilyIndex });
    vk::Extent2D extent;
    std::tie(extent.width, extent.height) = m_Window->GetSize();
    m_SwapchainBuilder.SetExtent(extent);

    m_ShaderLibrary = std::make_unique<ShaderLibrary>(m_LogicalDevice, m_ApiVersion);
    m_PipelineLibrary = std::make_unique<PipelineLibrary>(m_LogicalDevice, m_ShaderLibrary.get());

    m_ApplicationStateSpec = ApplicationStateSpec {
        .ApplicationWindow = m_Window.get(),
        .ShaderLibrary = m_ShaderLibrary.get(),
        .PipelineLibrary = m_PipelineLibrary.get(),
        .ApiVersion = m_ApiVersion,
        .Instance = m_Instance,
        .DispatchLoader = &m_DispatchLoader,
        .PhysicalDevice = m_PhysicalDevice,
        .LogicalDevice = m_LogicalDevice,
        .Queues = spec.Queues,
        .SwapchainBuilder = &m_SwapchainBuilder,
    };
}

Application::~Application()
{
    m_States.clear();

    m_ShaderLibrary->WriteShaderCaches();
    m_PipelineLibrary->WritePipelineCache();
    s_Instance = nullptr;

    m_PipelineLibrary.reset();
    m_ShaderLibrary.reset();
    m_Swapchain.reset();
    m_LogicalDevice.destroy();
    m_Instance.destroySurfaceKHR(m_Surface);
    if (m_DebugMessenger.has_value())
        m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger.value(), nullptr, m_DispatchLoader);
    m_Instance.destroy();
}

ShaderLibrary &Application::GetShaderLibrary()
{
    return *m_ShaderLibrary.get();
}

PipelineLibrary &Application::GetPipelineLibrary()
{
    return *m_PipelineLibrary.get();
}

const ApplicationStateSpec &Application::GetApplicationStateSpec()
{
    return m_ApplicationStateSpec;
}

void Application::Run(const std::string &state)
{
    m_CurrentState = m_NextState = m_States.at(state).get();
    m_CurrentStateName = m_NextStateName = state;
    m_CurrentState->OnEnter(nullptr);

    auto time = std::chrono::steady_clock::now();

    while (!m_Window->ShouldClose())
    {
        const auto newTime = std::chrono::steady_clock::now();
        const float timeStep = std::chrono::duration<float, std::milli>(newTime - time).count();
        time = newTime;

        Window::PollEvents();

        if (m_Window->IsMinimized())
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(16.6ms);
            continue;
        }

        if (m_NextState == nullptr)
            break;

        uint32_t tries = 5;
        while (m_CurrentState != m_NextState)
        {
            logger::debug("Transitioning from state `{}` to state `{}`", m_CurrentStateName, m_NextStateName);
            m_LogicalDevice.waitIdle();

            {
                ApplicationState *exitingState = m_CurrentState;
                ApplicationState *enteringState = m_NextState;
                std::string enteringName = m_NextStateName;
                exitingState->OnExit(enteringState);
                enteringState->OnEnter(exitingState);
                m_CurrentStateName = std::move(enteringName);
                m_CurrentState = enteringState;
            }

            if (--tries == 0)
            {
                logger::critical("Application state transition failed 5 times");
                break;
            }

            if (m_CurrentState == m_NextState && m_Swapchain != nullptr)
                m_CurrentState->OnResize(m_Swapchain.get());
        }

        auto [width, height] = m_Window->GetSize();
        if (m_Swapchain == nullptr || m_RecreateSwapchain ||
            m_Swapchain->GetExtent() != vk::Extent2D(width, height))
        {
            m_SwapchainBuilder.RefreshSurfaceCabilities();
            m_SwapchainBuilder.SetExtent(vk::Extent2D(width, height));
            m_MainQueue.Handle.waitIdle();
            m_Swapchain = m_SwapchainBuilder.CreateSwapchainUnique(m_LogicalDevice, m_Swapchain.get());
            m_CurrentState->OnResize(m_Swapchain.get());
            m_RecreateSwapchain = false;
            continue;
        }

        m_CurrentState->OnUpdate(timeStep);
        if (m_CurrentState != m_NextState)
            continue;

        m_Swapchain->AcquireImage();
        m_CurrentState->OnRender();
        m_Swapchain->Present(m_MainQueue.Handle);
    }

    m_LogicalDevice.waitIdle();
    if (m_CurrentState != nullptr)
        m_CurrentState->OnExit(nullptr);
}

void Application::AddState(const std::string &name, std::unique_ptr<ApplicationState> state)
{
    m_States[name] = std::move(state);
}

void Application::SetNextState(const std::string &name)
{
    m_NextStateName = name;
    m_NextState = m_States.at(name).get();
}

void Application::SetRecreateSwapchain()
{
    m_RecreateSwapchain = true;
}

}
