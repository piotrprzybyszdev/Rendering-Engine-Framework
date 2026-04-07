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
    H,
};

class UserInterfaceState
{
public:
    UserInterfaceState() = default;
    virtual ~UserInterfaceState() = default;

    UserInterfaceState(const UserInterfaceState &) = delete;
    UserInterfaceState &operator=(const UserInterfaceState &) = delete;

    virtual void OnInit() = 0;
    virtual void OnShutdown() = 0;

    virtual void OnUpdate(float timeStep) = 0;
    virtual void OnKeyRelease(Key key) = 0;
};

class UserInterface
{
public:
    static void InitSystem();
    static void ShutdownSystem();

    static void EmitKeyRelease(Key key);

public:
    UserInterface(UserInterfaceVulkanSpec spec, std::unique_ptr<UserInterfaceState> state);
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
    std::unique_ptr<UserInterfaceState> m_State;

private:
    void OnActivateVulkan();
    void OnDeactivateVulkan();
};  

}