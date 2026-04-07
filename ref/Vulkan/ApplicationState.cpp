#include <imgui.h>

#include "Core/Core.h"

#include "Application.h"
#include "ApplicationState.h"

#include "Renderer/FrameGraphBuilder.h"

namespace ref::vulkan
{

void ApplicationState::OnEnter(ApplicationState * /* previous */)
{
}

void ApplicationState::OnExit(ApplicationState * /* next */)
{
}

ErrorUserInterfaceState::ErrorUserInterfaceState(
    const std::vector<std::string> &errors, const std::string &state
)
    : m_Errors(errors), m_State(state)
{
}

void ErrorUserInterfaceState::OnInit()
{
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
}

void ErrorUserInterfaceState::OnShutdown()
{
    ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable);
}

void ErrorUserInterfaceState::OnUpdate(float /* timeStep */)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin(
        "Errors", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    ImGui::SeparatorText("Errors");
    if (!m_Errors.empty())
    {
        for (const auto &error : m_Errors)
            ImGui::Text("%s", error.c_str());
    }
    else
        ImGui::Text("There are no errors");
    ImGui::Dummy({ 0.0f, 5.0f });
    ImGui::Text("Press space to switch the state back");
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void ErrorUserInterfaceState::OnKeyRelease(Key key)
{
    switch (key)
    {
    case Key::Space:
        Application::GetInstance()->SetNextState(m_State);
        break;
    default:
    break;
    }
}

ErrorApplicationState::ErrorApplicationState(const ApplicationStateSpec &spec)
    : m_MainQueue(spec.Queues.at(Application::MainQueueName))
{
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = 2,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<UserInterface>(
        userInterfaceSpec, std::make_unique<ErrorUserInterfaceState>(m_Errors, m_State)
    );

    FrameGraphBuilder builder;

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .LoadOp = vk::AttachmentLoadOp::eClear,
                    .ClearValue = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f),
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
}

ErrorApplicationState::~ErrorApplicationState()
{
}

void ErrorApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void ErrorApplicationState::OnUpdate(float timeStep)
{
    m_UserInterface->OnUpdate(timeStep);
    m_Renderer->OnUpdate(timeStep);
}

void ErrorApplicationState::OnRender()
{
    m_Renderer->OnRender();
}

void ErrorApplicationState::AddToApplication(Application &application)
{
    application.AddAndCreateState<vulkan::ErrorApplicationState>(g_StateName);
}

std::vector<std::string> &ErrorApplicationState::GetErrors()
{
    return m_Errors;
}

void ErrorApplicationState::SetErrorState(const std::string &prevState)
{
    m_State = prevState;
    Application::GetInstance()->SetNextState(g_StateName);
}

}
