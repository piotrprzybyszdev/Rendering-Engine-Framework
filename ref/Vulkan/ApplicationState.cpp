#include <imgui.h>

#include "Core/Core.h"

#include "Application.h"
#include "ApplicationState.h"

#include "Renderer/FrameGraphBuilder.h"

namespace ref::vulkan
{

namespace
{
vk::Format GetSurfaceFormat(SwapchainBuilder *swapchainBuilder)
{
    bool hasRGBA = false, hasBGRA = false;
    for (const auto &format : swapchainBuilder->GetSurfaceFormats())
    {
        if (format.format == vk::Format::eUndefined)
            return vk::Format::eR8G8B8A8Unorm;

        if (format.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear)
            continue;

        if (format.format == vk::Format::eR8G8B8A8Unorm)
            hasRGBA = true;
        if (format.format == vk::Format::eB8G8R8A8Unorm)
            hasBGRA = true;
    }

    if (hasRGBA)
        return vk::Format::eR8G8B8A8Unorm;
    if (hasBGRA)
        return vk::Format::eB8G8R8A8Unorm;

    return vk::Format::eUndefined;
}
}

ErrorUserInterface::ErrorUserInterface(
    UserInterfaceVulkanSpec spec, const std::vector<std::string> &errors, const std::string &state
)
    : UserInterface(spec), m_Errors(errors), m_State(state)
{
}

void ErrorUserInterface::OnEnter()
{
    UserInterface::OnEnter();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
}

void ErrorUserInterface::OnExit()
{
    ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable);
    UserInterface::OnExit();
}

void ErrorUserInterface::OnDefineUI([[maybe_unused]] float timeStep)
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

void ErrorUserInterface::OnKeyEvent(Key key, KeyAction action, [[maybe_unused]] Mods mods)
{
    if (action != KeyAction::Release)
        return;

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
    : m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.Queues.at(Application::MainQueueName))
{
}

ErrorApplicationState::~ErrorApplicationState() {}

void ErrorApplicationState::OnEnter(ApplicationState * /* previous */)
{
    const auto &spec = Application::GetInstance()->GetApplicationStateSpec();
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    vk::Format format = GetSurfaceFormat(spec.SwapchainBuilder);
    if (format == vk::Format::eUndefined)
        throw std::runtime_error("No suitable swapchain format found");

    spec.SwapchainBuilder->SetPresentMode(vk::PresentModeKHR::eFifo);
    const uint32_t imageCount =
        std::clamp(2u, spec.SwapchainBuilder->GetMinImageCount(), spec.SwapchainBuilder->GetMaxImageCount());
    spec.SwapchainBuilder->SetImageCount(imageCount);
    spec.SwapchainBuilder->SetUsageFlags(
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst
    );
    Application::GetInstance()->SetRecreateSwapchain();

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = imageCount,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<ErrorUserInterface>(userInterfaceSpec, s_Errors, s_State);

    FrameGraphBuilder builder;
    builder.AddDeviceImageWithView(
        "Image",
        vk::ImageCreateInfo(
            vk::ImageCreateFlagBits::eMutableFormat, vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
            vk::Extent3D(1280, 720, 1), 1, 1
        )
            .setUsage(
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eColorAttachment
            ),
        ResourceType::Transient, true
    );

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageViewResource = "Image View",
                    .LoadOp = vk::AttachmentLoadOp::eClear,
                    .ClearValue = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f),
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    {
        vk::ImageSubresourceLayers layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);

        BlitPassSpec blitSpec = {
            .SrcImageResource = "Image",
            .DstImageResource = FrameGraph::g_SwapchainImageResourceName,
            .Regions = { vk::ImageBlit2(layers, {}, layers, {}) },
        };
        builder.AddBlitPass("Blit Pass", blitSpec);
    }

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);

    m_UserInterface->OnEnter();
}

void ErrorApplicationState::OnExit(ApplicationState * /* next */)
{
    m_UserInterface->OnExit();

    m_Renderer.reset();
    m_FrameGraph.reset();
    m_UserInterface.reset();
    m_ResourceAllocator.reset();
}

void ErrorApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    m_FrameGraph->ModifyImage("Image").Info.setExtent(vk::Extent3D(extent, 1));
    m_FrameGraph->UpdateImage("Image");

    std::array<vk::Offset3D, 2> offsets = { vk::Offset3D(), vk::Offset3D(extent.width, extent.height, 1) };

    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetSrcOffsets().front() = offsets;
    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetDstOffsets().front() = offsets;

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void ErrorApplicationState::OnUpdate(float timeStep)
{
    m_UserInterface->OnUpdate(timeStep);
}

void ErrorApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
}

void ErrorApplicationState::AddToApplication(Application &application)
{
    application.AddAndCreateState<vulkan::ErrorApplicationState>(g_StateName);
}

std::vector<std::string> &ErrorApplicationState::GetErrors()
{
    return s_Errors;
}

void ErrorApplicationState::SetErrorState(const std::string &prevState)
{
    s_State = prevState;
    Application::GetInstance()->SetNextState(g_StateName);
}

bool ErrorApplicationState::ReloadShaders(const std::string &prevState)
{
    bool success = Application::GetInstance()->GetPipelineLibrary().CompilePipelines();
    if (!success)
    {
        auto errors = Application::GetInstance()->GetShaderLibrary().GetCompilationErrors();
        s_Errors = std::vector(errors.begin(), errors.end());
        SetErrorState(prevState);
    }

    return success;
}

LoadingUserInterface::LoadingUserInterface(UserInterfaceVulkanSpec spec, const std::string &progressText)
    : UserInterface(spec), m_ProgressText(progressText)
{
}

void LoadingUserInterface::OnEnter()
{
    UserInterface::OnEnter();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
}

void LoadingUserInterface::OnExit()
{
    ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable);
    UserInterface::OnExit();
}

void LoadingUserInterface::OnDefineUI([[maybe_unused]] float timeStep)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin(
        "Loading", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    ImGui::SetCursorPosX(viewport->WorkSize.x / 4.0f);
    ImGui::SetCursorPosY(viewport->WorkSize.y / 4.0f);
    const float progress = static_cast<float>(m_Done) / static_cast<float>(m_Total);
    const std::string text = std::format("{} [{}/{}]", m_ProgressText, m_Done, m_Total);
    ImGui::ProgressBar(progress, ImVec2(viewport->WorkSize.x / 2.0f, 0), text.c_str());
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void LoadingUserInterface::SetProgress(uint32_t total, uint32_t done)
{
    m_Total = total;
    m_Done = done;
}

LoadingApplicationState::LoadingApplicationState(
    const ApplicationStateSpec &spec, const std::string &state, const std::string &progressText
)
    : m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.Queues.at(Application::MainQueueName)),
      m_StateName(state), m_ProgressText(progressText)
{
}

LoadingApplicationState::~LoadingApplicationState() {}

void LoadingApplicationState::OnEnter(ApplicationState * /* previous */)
{
    const auto &spec = Application::GetInstance()->GetApplicationStateSpec();
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    vk::Format format = GetSurfaceFormat(spec.SwapchainBuilder);
    if (format == vk::Format::eUndefined)
        throw std::runtime_error("No suitable swapchain format found");

    spec.SwapchainBuilder->SetSurfaceFormat(format);
    spec.SwapchainBuilder->SetPresentMode(vk::PresentModeKHR::eFifo);
    const uint32_t imageCount =
        std::clamp(2u, spec.SwapchainBuilder->GetMinImageCount(), spec.SwapchainBuilder->GetMaxImageCount());
    spec.SwapchainBuilder->SetImageCount(imageCount);
    spec.SwapchainBuilder->SetUsageFlags(
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst
    );

    Application::GetInstance()->SetRecreateSwapchain();

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = imageCount,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<LoadingUserInterface>(userInterfaceSpec, m_ProgressText);

    FrameGraphBuilder builder;
    builder.AddDeviceImageWithView(
        "Image",
        vk::ImageCreateInfo(
            vk::ImageCreateFlagBits::eMutableFormat, vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
            vk::Extent3D(1280, 720, 1), 1, 1
        )
            .setUsage(
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eColorAttachment
            ),
        ResourceType::Transient, true
    );

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageViewResource = "Image View",
                    .LoadOp = vk::AttachmentLoadOp::eClear,
                    .ClearValue = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f),
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    {
        vk::ImageSubresourceLayers layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);

        BlitPassSpec blitSpec = {
            .SrcImageResource = "Image",
            .DstImageResource = FrameGraph::g_SwapchainImageResourceName,
            .Regions = { vk::ImageBlit2(layers, {}, layers, {}) },
        };
        builder.AddBlitPass("Blit Pass", blitSpec);
    }

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);

    m_UserInterface->OnEnter();

    m_Total = 0;
    m_Done = 0;
}

void LoadingApplicationState::OnExit(ApplicationState * /* next */)
{
    m_UserInterface->OnExit();
    m_Renderer.reset();
    m_FrameGraph.reset();
    m_UserInterface.reset();
    m_ResourceAllocator.reset();
}

void LoadingApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    m_FrameGraph->ModifyImage("Image").Info.setExtent(vk::Extent3D(extent, 1));
    m_FrameGraph->UpdateImage("Image");

    std::array<vk::Offset3D, 2> offsets = { vk::Offset3D(), vk::Offset3D(extent.width, extent.height, 1) };

    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetSrcOffsets().front() = offsets;
    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetDstOffsets().front() = offsets;

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void LoadingApplicationState::OnUpdate([[maybe_unused]] float timeStep)
{
    m_UserInterface->SetProgress(m_Total, m_Done);
    m_UserInterface->OnUpdate(timeStep);
}

void LoadingApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
}

CompilingShadersApplicationState::CompilingShadersApplicationState(const ApplicationStateSpec &spec)
    : LoadingApplicationState(spec, g_StateName, "Compiling Shaders")
{
}

void CompilingShadersApplicationState::OnEnter(ApplicationState *previous)
{
    LoadingApplicationState::OnEnter(previous);

    uint32_t threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0)
        threadCount = 1;

    Application::GetInstance()->GetShaderLibrary().LoadShadersAsync(m_Total, m_Done, threadCount);
}

void CompilingShadersApplicationState::OnUpdate(float timeStep)
{
    LoadingApplicationState::OnUpdate(timeStep);

    if (m_Total == m_Done)
    {
        auto errors = Application::GetInstance()->GetShaderLibrary().GetCompilationErrors();

        if (!errors.empty())
        {
            ErrorApplicationState::GetErrors() = std::vector(errors.begin(), errors.end());
            ErrorApplicationState::SetErrorState(g_StateName);
        }
        else
            Application::GetInstance()->SetNextState(s_NextState);
    }
}

void CompilingShadersApplicationState::AddToApplication(
    Application &application, const std::string &nextState
)
{
    application.AddAndCreateState<vulkan::CompilingShadersApplicationState>(g_StateName);
    SetNextState(nextState);
}

void CompilingShadersApplicationState::SetNextState(const std::string &state)
{
    s_NextState = state;
}

}
