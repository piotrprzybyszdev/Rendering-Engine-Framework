#include <GLFW/glfw3.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>

#include "Core.h"
#include "UserInterface.h"

namespace ref
{

void DefaultUserInterfaceCheckVulkanResultCallback(VkResult result)
{
    if (result == VkResult::VK_SUCCESS)
        return;

    logger::error("ImGui Vulkan Error: {}", vk::to_string(static_cast<vk::Result>(result)));
}

static Key GetKey(int key)
{
    switch (key)
    {
    case GLFW_KEY_SPACE:
        return Key::Space;
    case GLFW_KEY_H:
        return Key::H;
    default:
        return Key::Unknown;
    }
}

static void GlfwKeyCallback(GLFWwindow * /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action == GLFW_RELEASE)
        UserInterface::EmitKeyRelease(GetKey(key));
}

void UserInterface::EmitKeyRelease(Key key)
{
    s_Instance->m_State->OnKeyRelease(key);
}

void UserInterface::InitSystem()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
}

void UserInterface::ShutdownSystem()
{
    ImGui::DestroyContext();
}

UserInterface::UserInterface(UserInterfaceVulkanSpec spec, std::unique_ptr<UserInterfaceState> state)
    : m_VulkanSpec(spec), m_State(std::move(state))
{
}

UserInterface::~UserInterface()
{
    if (s_Instance == this)
        SetInstance(nullptr);
}

void UserInterface::OnUpdate(float timeStep)
{
    SetInstance(this);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    m_State->OnUpdate(timeStep);
    ImGui::EndFrame();
    ImGui::UpdatePlatformWindows();
}

void UserInterface::OnRenderVulkan(vk::CommandBuffer commandBuffer)
{
    ImGui::Render();
    ImGui::RenderPlatformWindowsDefault();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void UserInterface::SetInstance(UserInterface *instance)
{
    if (s_Instance == instance)
        return;
    if (s_Instance != nullptr)
        s_Instance->OnDeactivateVulkan();
    if (instance != nullptr)
        instance->OnActivateVulkan();
    s_Instance = instance;
}

void UserInterface::OnActivateVulkan()
{
    glfwSetKeyCallback(m_VulkanSpec.Window, GlfwKeyCallback);

    ImGui_ImplGlfw_InitForVulkan(m_VulkanSpec.Window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {
        .ApiVersion = m_VulkanSpec.ApiVersion,
        .Instance = m_VulkanSpec.Instance,
        .PhysicalDevice = m_VulkanSpec.PhysicalDevice,
        .Device = m_VulkanSpec.LogicalDevice,
        .QueueFamily = m_VulkanSpec.QueueFamilyIndex,
        .Queue = m_VulkanSpec.Queue,
        .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
        .MinImageCount = m_VulkanSpec.ImageCount,
        .ImageCount = m_VulkanSpec.ImageCount,
        .UseDynamicRendering = true,
        .CheckVkResultFn = m_VulkanSpec.Callback,
    };

    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo =
        vk::PipelineRenderingCreateInfoKHR(0, m_VulkanSpec.ImageFormat);

    bool imguiResult = ImGui_ImplVulkan_Init(&initInfo);
    if (imguiResult == false)
        throw std::runtime_error("Failed to initialize ImGui");

    m_State->OnInit();
}

void UserInterface::OnDeactivateVulkan()
{
    m_State->OnShutdown();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}

}
