#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>

#include "Window.h"

#include "Vulkan/ApplicationBuilder.h"

namespace ref
{

void Window::InitSystem(WindowSystemErrorCallback callback)
{
    if (glfwInit() == GLFW_FALSE)
        throw std::runtime_error("Window System Initialization failed");

    glfwSetErrorCallback(callback);
}

void Window::ShutdownSystem()
{
    glfwTerminate();
}

void Window::EnableVulkanExtensions(vulkan::ApplicationBuilder &builder)
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (uint32_t i = 0; i < glfwExtensionCount; i++)
        builder.EnableExtension(glfwExtensions[i]);
}

Window::Window(const char *title, uint16_t width, uint16_t height)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_Handle = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (m_Handle == nullptr)
        throw std::runtime_error("Window creation failed");
}

Window::~Window()
{
    if (m_Handle != nullptr)
        glfwDestroyWindow(m_Handle);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Handle) == GLFW_TRUE;
}

std::pair<uint32_t, uint32_t> Window::GetSize() const
{
    int width, height;
    glfwGetWindowSize(m_Handle, &width, &height);
    return std::make_pair(width, height);
}

vk::SurfaceKHR Window::CreateVulkanSurface(vk::Instance instance) const
{
    VkSurfaceKHR surface = nullptr;
    VkResult result = glfwCreateWindowSurface(instance, m_Handle, nullptr, &surface);
    if (result != VkResult::VK_SUCCESS)
        throw std::runtime_error("Window surface cration failed");
    return surface;
}

void DefaultWindowSystemErrorCallback(int error, const char *description)
{
    throw std::runtime_error(std::format("GLFW error {} {}", error, description).c_str());
}

}