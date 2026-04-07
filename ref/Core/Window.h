#pragma once

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace ref
{

namespace vulkan
{
class ApplicationBuilder;
}

using WindowSystemErrorCallback = void(*)(int error, const char *description);
void DefaultWindowSystemErrorCallback(int error, const char *description);

class Window
{
public:
    static void InitSystem(WindowSystemErrorCallback callback = DefaultWindowSystemErrorCallback);
    static void PollEvents();
    static void ShutdownSystem();

public:
    Window(const char *title, uint16_t width, uint16_t height);
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    bool ShouldClose() const;
    bool IsMinimized() const;

    GLFWwindow *GetHandle() const;
    std::pair<uint32_t, uint32_t> GetSize() const;

public:
    vk::SurfaceKHR CreateVulkanSurface(vk::Instance instnace) const;
    static void EnableVulkanExtensions(vulkan::ApplicationBuilder &builder);

private:
    GLFWwindow *m_Handle = nullptr;
};

}