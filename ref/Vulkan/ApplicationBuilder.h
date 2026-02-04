#pragma once

#include <vulkan/vulkan.hpp>

#include "Core/Window.h"

#include "Application.h"
#include "Features.h"

#include <map>
#include <optional>
#include <vector>

namespace ref::vulkan
{

struct QueueSpec
{
    const char *Name;
    vk::QueueFlags Flags;
    bool Present = false;
    bool Optional = false;
    std::vector<const char *> Exclusive;
};

class ApplicationBuilder
{
public:
    static void InitSystems();
    static void ShutdownSystems();

public:
    ApplicationBuilder &EnableBase();
    ApplicationBuilder &EnablePortability();
    ApplicationBuilder &EnableValidationLayers(
        std::optional<vk::DebugUtilsMessengerCreateInfoEXT> debugInfo = std::nullopt
    );
    ApplicationBuilder &RequestDefaultQueues();

    ApplicationBuilder &SetApiVersion(uint32_t version);
    ApplicationBuilder &EnableExtension(const char *extension);
    ApplicationBuilder &EnableLayer(const char *layer);
    ApplicationBuilder &EnableDeviceExtension(const char *extension);
    template<typename F> ApplicationBuilder &EnableFeatures(const F &features);

    ApplicationBuilder &SetEngine(const char *name, uint32_t version = 0);
    ApplicationBuilder &SetWindow(const char *title, uint16_t width, uint16_t height);

    ApplicationBuilder &AddQueue(QueueSpec queueSpec);

    Application CreateApplication(const char *applicationName, uint32_t version = 0);

private:
    uint32_t m_ApiVersion = vk::ApiVersion10;
    std::vector<const char *> m_Extensions;
    std::vector<const char *> m_Layers;
    std::vector<const char *> m_DeviceExtensions;
    Features m_Features;
    bool m_EnumeratePortability = false;
    std::optional<vk::DebugUtilsMessengerCreateInfoEXT> m_DebugInfo;
    const char *m_EngineName = "REF";
    uint32_t m_EngineVersion = 0;
    bool m_CreateWindow = false;
    const char *m_WindowTitle = "REF";
    uint16_t m_WindowWidth = 1280u, m_WindowHeight = 720u;

    std::vector<QueueSpec> m_QueueSpecs;
    std::map<const char *, Queue> m_Queues;

private:
    bool CheckInstance();
    bool CheckPhysicalDevice(vk::PhysicalDevice device, vk::SurfaceKHR surface);

    std::vector<uint32_t> FindQueueFamilyIndices(vk::PhysicalDevice device, vk::SurfaceKHR surface) const;
};

template<typename F> inline ApplicationBuilder &ApplicationBuilder::EnableFeatures(const F &features)
{
    m_Features.SetFeatures(features);
    return *this;
}

}