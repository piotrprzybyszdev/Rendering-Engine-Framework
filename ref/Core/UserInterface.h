#pragma once

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace ref
{

using UserInterfaceCheckVulkanResultCallback = void (*)(VkResult result);
void DefaultUserInterfaceCheckVulkanResultCallback(VkResult result);

struct UserInterfaceVulkanSpec
{
    UserInterfaceCheckVulkanResultCallback Callback = DefaultUserInterfaceCheckVulkanResultCallback;
    GLFWwindow *Window;
    uint32_t ApiVersion;
    vk::Instance Instance;
    vk::PhysicalDevice PhysicalDevice;
    vk::Device LogicalDevice;
    uint32_t QueueFamilyIndex;
    vk::Queue Queue;
    uint32_t ImageCount;
    vk::Format ImageFormat;
};

enum class Key
{
    Unknown,
    Space,
    W,
    A,
    S,
    D,
    Q,
    E,
    H,
};

enum class KeyAction
{
    Press,
    Release,
    Repeat,
};

enum class Button
{
    Left,
    Right,
    Middle,
    Unknown,
};

enum class ButtonAction
{
    Press,
    Release,
};

struct Mods
{
    bool Shift, Control, Alt, Super, CapsLock, NumLock;
};

class UserInterfaceState
{
public:
    UserInterfaceState() = default;
    virtual ~UserInterfaceState() = default;

    UserInterfaceState(const UserInterfaceState &) = delete;
    UserInterfaceState &operator=(const UserInterfaceState &) = delete;

    virtual void OnInit() {}
    virtual void OnShutdown() {}

    virtual void OnUpdate([[maybe_unused]] float timeStep) {}

    virtual void OnKeyEvent(
        [[maybe_unused]] Key key, [[maybe_unused]] KeyAction action, [[maybe_unused]] Mods mods
    )
    {
    }
    virtual void OnMouseButtonEvent(
        [[maybe_unused]] Button button, [[maybe_unused]] ButtonAction action, [[maybe_unused]] Mods mods
    )
    {
    }
    virtual void OnCursorMoveEvent([[maybe_unused]] double xpos, [[maybe_unused]] double ypos) {}
};

class UserInterface
{
public:
    static void InitSystem();
    static void ShutdownSystem();

    static void EmitKeyEvent(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void EmitMouseButtonEvent(GLFWwindow *window, int button, int action, int mods);
    static void EmitCursorMoveEvent(GLFWwindow *window, double xpos, double ypos);

public:
    UserInterface(UserInterfaceVulkanSpec spec, UserInterfaceState &state);
    ~UserInterface();

    UserInterface(const UserInterface &) = delete;
    UserInterface &operator=(const UserInterface &) = delete;

    void OnUpdate(float timeStep);

public:
    void OnRenderVulkan(vk::CommandBuffer commandBuffer);

private:
    static inline UserInterface *s_Instance = nullptr;

private:
    static void SetInstance(UserInterface *instance);

private:
    UserInterfaceVulkanSpec m_VulkanSpec;
    UserInterfaceState &m_State;

private:
    void OnActivateVulkan();
    void OnDeactivateVulkan();
};

}