#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
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

namespace
{

Key ToKey(int key)
{
    switch (key)
    {
    case GLFW_KEY_SPACE:
        return Key::Space;
    case GLFW_KEY_H:
        return Key::H;
    case GLFW_KEY_W:
        return Key::W;
    case GLFW_KEY_A:
        return Key::A;
    case GLFW_KEY_S:
        return Key::S;
    case GLFW_KEY_D:
        return Key::D;
    case GLFW_KEY_Q:
        return Key::Q;
    case GLFW_KEY_E:
        return Key::E;
    default:
        return Key::Unknown;
    }
}

KeyAction ToKeyAction(int action)
{
    switch (action)
    {
    case GLFW_PRESS:
        return KeyAction::Press;
    case GLFW_RELEASE:
        return KeyAction::Release;
    case GLFW_REPEAT:
        return KeyAction::Repeat;
    default:
        std::terminate();
    }
}

Button ToButton(int button)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        return Button::Left;
    case GLFW_MOUSE_BUTTON_RIGHT:
        return Button::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        return Button::Middle;
    default:
        return Button::Unknown;
    }
}

ButtonAction ToButtonAction(int action)
{
    switch (action)
    {
    case GLFW_PRESS:
        return ButtonAction::Press;
    case GLFW_RELEASE:
        return ButtonAction::Release;
    default:
        std::terminate();
    }
}

Mods ToMods(int mods)
{
    return {
        .Shift = (mods & GLFW_MOD_SHIFT) != GLFW_FALSE,
        .Control = (mods & GLFW_MOD_CONTROL) != GLFW_FALSE,
        .Alt = (mods & GLFW_MOD_ALT) != GLFW_FALSE,
        .Super = (mods & GLFW_MOD_SUPER) != GLFW_FALSE,
        .CapsLock = (mods & GLFW_MOD_CAPS_LOCK) != GLFW_FALSE,
        .NumLock = (mods & GLFW_MOD_NUM_LOCK) != GLFW_FALSE,
    };
}

}

void UserInterface::EmitKeyEvent(
    [[maybe_unused]] GLFWwindow *window, int key, [[maybe_unused]] int scancode, int action, int mods
)
{
    if (s_Instance != nullptr)
        s_Instance->m_State.OnKeyEvent(ToKey(key), ToKeyAction(action), ToMods(mods));
}

void UserInterface::EmitMouseButtonEvent(
    [[maybe_unused]] GLFWwindow *window, int button, int action, int mods
)
{
    if (s_Instance != nullptr)
        s_Instance->m_State.OnMouseButtonEvent(ToButton(button), ToButtonAction(action), ToMods(mods));
}

void UserInterface::EmitCursorMoveEvent([[maybe_unused]] GLFWwindow *window, double xpos, double ypos)
{
    if (s_Instance != nullptr)
        s_Instance->m_State.OnCursorMoveEvent(xpos, ypos);
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

UserInterface::UserInterface(UserInterfaceVulkanSpec spec, UserInterfaceState &state)
    : m_VulkanSpec(spec), m_State(state)
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
    m_State.OnUpdate(timeStep);
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
    glfwSetKeyCallback(m_VulkanSpec.Window, UserInterface::EmitKeyEvent);
    glfwSetMouseButtonCallback(m_VulkanSpec.Window, UserInterface::EmitMouseButtonEvent);
    glfwSetCursorPosCallback(m_VulkanSpec.Window, UserInterface::EmitCursorMoveEvent);

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

    m_State.OnInit();
}

void UserInterface::OnDeactivateVulkan()
{
    m_State.OnShutdown();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}

}
