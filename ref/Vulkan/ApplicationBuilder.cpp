#include <ranges>

#include "Core/Core.h"
#include "Core/UserInterface.h"
#include "Core/Window.h"

#include "ApplicationBuilder.h"

namespace ref::vulkan
{

static bool s_ValidationEnabled = false;
static uint32_t s_ValidationWarningCount = 0;
static uint32_t s_ValidationErrorCount = 0;

VKAPI_ATTR vk::Bool32 VKAPI_CALL DefaultDebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT /* messageTypes */,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void * /* pUserData */
)
{
    switch (messageSeverity)
    {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        logger::error(pCallbackData->pMessage);
        s_ValidationErrorCount++;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        logger::warn(pCallbackData->pMessage);
        s_ValidationWarningCount++;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
        logger::info(pCallbackData->pMessage);
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        logger::debug(pCallbackData->pMessage);
        break;
    }

    s_ValidationEnabled = true;
    return VK_FALSE;
}

void ApplicationBuilder::InitSystems()
{
    Window::InitSystem();
    UserInterface::InitSystem();
}

void ApplicationBuilder::ShutdownSystems()
{
    UserInterface::ShutdownSystem();
    Window::ShutdownSystem();

    if (s_ValidationEnabled)
    {
        if (s_ValidationWarningCount)
            logger::warn("Encountered {} validation layer warnings", s_ValidationWarningCount);
        if (s_ValidationErrorCount)
            logger::error("Encountered {} validation layer errors", s_ValidationErrorCount);
        if (s_ValidationWarningCount == 0 && s_ValidationErrorCount == 0)
            logger::info("Validation layers raised no issues");
    }
}

ApplicationBuilder &ApplicationBuilder::EnableBase()
{
    SetApiVersion(vk::ApiVersion13);
    EnableFeatures(vk::PhysicalDeviceVulkan13Features().setSynchronization2(vk::True));
    SetWindow(nullptr, 1280u, 720u);
    EnableUserInterface();

#if defined(REF_CONFIG_DEBUG) || defined(REF_CONFIG_TRACE)
    EnableValidationLayers();
#elif defined(REF_CONFIG_PROFILE)
    EnableExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    RequestDefaultQueues();
    return *this;
}

ApplicationBuilder &ApplicationBuilder::EnablePortability()
{
    m_EnumeratePortability = true;
    EnableExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    EnableExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    return *this;
}

ApplicationBuilder &ApplicationBuilder::EnableValidationLayers(
    std::optional<vk::DebugUtilsMessengerCreateInfoEXT> debugInfo
)
{
    EnableExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    EnableLayer("VK_LAYER_KHRONOS_validation");
    m_DebugInfo = debugInfo.value_or(
        vk::DebugUtilsMessengerCreateInfoEXT(
            vk::DebugUtilsMessengerCreateFlagsEXT(),
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            &DefaultDebugCallback
        )
    );
    return *this;
}

ApplicationBuilder &ApplicationBuilder::RequestDefaultQueues()
{
    AddQueue(
        QueueSpec { .Name = Application::MainQueueName,
                    .Flags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute |
                             vk::QueueFlagBits::eTransfer,
                    .Present = true,
                    .Optional = false,
                    .Exclusive = {} }
    );
    AddQueue(
        QueueSpec { .Name = Application::AsyncComputeQueueName,
                    .Flags = vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer,
                    .Present = false,
                    .Optional = true,
                    .Exclusive = { Application::MainQueueName } }
    );
    AddQueue(
        QueueSpec { .Name = Application::AsyncTransferQueueName,
                    .Flags = vk::QueueFlagBits::eTransfer,
                    .Present = false,
                    .Optional = true,
                    .Exclusive = { Application::MainQueueName, Application::AsyncComputeQueueName } }
    );
    return *this;
}

ApplicationBuilder &ApplicationBuilder::EnableUserInterface()
{
    if (m_ApiVersion < vk::ApiVersion13)
        EnableDeviceExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    EnableFeatures(vk::PhysicalDeviceVulkan13Features().setDynamicRendering(vk::True));
    return *this;
}

ApplicationBuilder &ApplicationBuilder::SetApiVersion(uint32_t version)
{
    m_ApiVersion = version;
    return *this;
}

ApplicationBuilder &ApplicationBuilder::EnableExtension(const char *extension)
{
    m_Extensions.push_back(extension);
    return *this;
}

ApplicationBuilder &ApplicationBuilder::EnableLayer(const char *layer)
{
    m_Layers.push_back(layer);
    return *this;
}

ApplicationBuilder &ApplicationBuilder::EnableDeviceExtension(const char *extension)
{
    m_DeviceExtensions.push_back(extension);
    return *this;
}

ApplicationBuilder &ApplicationBuilder::SetEngine(const char *name, uint32_t version)
{
    m_EngineName = name;
    m_EngineVersion = version;
    return *this;
}

ApplicationBuilder &ApplicationBuilder::SetWindow(const char *title, uint16_t width, uint16_t height)
{
    Window::EnableVulkanExtensions(*this);
    EnableDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    m_CreateWindow = true;
    m_WindowTitle = title;
    m_WindowWidth = width;
    m_WindowHeight = height;
    return *this;
}

ApplicationBuilder &ApplicationBuilder::AddQueue(QueueSpec queueSpec)
{
    for (const char *excludes : queueSpec.Exclusive)
        if (!std::ranges::contains(m_QueueSpecs, excludes, [](auto &spec) { return spec.Name; }))
            throw configuration_error(
                std::format(
                    "Queue `{}` excludes queue `{}` that was not added yet. Add that queue first.",
                    queueSpec.Name, excludes
                )
            );

    if (std::ranges::contains(m_QueueSpecs, queueSpec.Name, [](auto &spec) { return spec.Name; }))
        throw configuration_error(std::format("Queue with name `{}` is already added", queueSpec.Name));

    m_QueueSpecs.push_back(queueSpec);
    return *this;
}

std::string ApiVersionToString(uint32_t version)
{
    return std::format(
        "{}.{}.{}", vk::apiVersionMajor(version), vk::apiVersionMinor(version), vk::apiVersionPatch(version)
    );
}

Application ApplicationBuilder::CreateApplication(const char *name, uint32_t version)
{
    // Instance
    if (!CheckInstance())
        throw initialization_error("Instance does not support requested api version, extensions or layers");

    logger::info("Selected Vulkan Version: {}", ApiVersionToString(m_ApiVersion));
    vk::ApplicationInfo applicationInfo(name, version, m_EngineName, m_EngineVersion, m_ApiVersion);

    const auto instanceCreateFlags = m_EnumeratePortability
                                         ? vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR
                                         : vk::InstanceCreateFlags();
    vk::InstanceCreateInfo instanceCreateInfo(instanceCreateFlags, &applicationInfo, m_Layers, m_Extensions);

    vk::Instance instance = vk::createInstance(instanceCreateInfo);

    auto dispatchLoader = vk::detail::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr);

    std::optional<vk::DebugUtilsMessengerEXT> messenger;
    if (m_DebugInfo.has_value())
        messenger = instance.createDebugUtilsMessengerEXT(m_DebugInfo.value(), nullptr, dispatchLoader);

    // Surface
    std::unique_ptr<Window> window = nullptr;
    vk::SurfaceKHR surface = nullptr;
    if (m_CreateWindow)
    {
        const char *title = m_WindowTitle == nullptr ? name : m_WindowTitle;
        window = std::make_unique<Window>(title, m_WindowWidth, m_WindowHeight);
        surface = window->CreateVulkanSurface(instance);
    }

    // Physical Device
    for (const char *extension : m_DeviceExtensions)
        logger::info("Device Extension {} is required", extension);

    std::vector<vk::PhysicalDevice> suitableDevices;
    for (vk::PhysicalDevice device : instance.enumeratePhysicalDevices())
    {
        if (!CheckPhysicalDevice(device, surface))
            continue;

        suitableDevices.push_back(device);
    }

    if (suitableDevices.empty())
        throw initialization_error("No suitable devices found");

    vk::PhysicalDevice physicalDevice = *std::ranges::max_element(
        suitableDevices, [](vk::PhysicalDevice device1, vk::PhysicalDevice device2) {
            auto properties1 = device1.getProperties();
            auto properties2 = device2.getProperties();

            if (properties1.deviceType == properties2.deviceType)
            {
                auto memoryProperties1 = device1.getMemoryProperties();
                auto memoryProperties2 = device2.getMemoryProperties();
                return memoryProperties1.memoryHeapCount < memoryProperties2.memoryHeapCount;
            }

            if (properties1.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                return false;
            if (properties2.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                return true;
            if (properties1.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
                return false;
            if (properties2.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
                return true;
            return properties1.deviceType < properties2.deviceType;
        }
    );

    // Logical Device
    std::vector<uint32_t> queueFamilyIndices = FindQueueFamilyIndices(physicalDevice, surface);

    std::map<uint32_t, std::vector<uint32_t>> familyIndexToQueueIndices;
    for (size_t i = 0; i < queueFamilyIndices.size(); i++)
        if (queueFamilyIndices[i] != vk::QueueFamilyIgnored)
            familyIndexToQueueIndices[queueFamilyIndices[i]].push_back(static_cast<uint32_t>(i));

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::vector<std::vector<float>> priorities;

    for (auto &[familyIndex, queueIndices] : familyIndexToQueueIndices)
    {
        const uint32_t queueCount = static_cast<uint32_t>(queueIndices.size());
        priorities.push_back(std::vector<float>(queueCount, 0.0f));
        queueCreateInfos.push_back(
            vk::DeviceQueueCreateInfo()
                .setQueueFamilyIndex(familyIndex)
                .setQueueCount(queueCount)
                .setQueuePriorities(priorities.back())
        );
    }

    vk::DeviceCreateInfo createInfo(
        vk::DeviceCreateFlags(), queueCreateInfos, {}, m_DeviceExtensions, nullptr, m_Features.GetFeatures2()
    );

    vk::Device logicalDevice = physicalDevice.createDevice(createInfo);

    // Queues
    for (auto &[familyIndex, queueIndices] : familyIndexToQueueIndices)
        for (size_t i = 0; i < queueIndices.size(); i++)
        {
            const QueueSpec &spec = m_QueueSpecs[queueIndices[i]];
            vk::Queue handle = logicalDevice.getQueue2(
                vk::DeviceQueueInfo2()
                    .setQueueFamilyIndex(familyIndex)
                    .setQueueIndex(static_cast<uint32_t>(i))
            );
            m_Queues[spec.Name] = Queue(familyIndex, handle);
            logger::trace("Queue `{}` is set to queue family {}", spec.Name, familyIndex);
        }

    return Application(ApplicationSpec(
        std::move(window), surface, m_ApiVersion, instance, messenger, dispatchLoader, physicalDevice,
        logicalDevice, m_Queues
    ));
}

bool ApplicationBuilder::CheckInstance()
{
    // Api Version
    const uint32_t instanceVersion = vk::enumerateInstanceVersion();
    const uint32_t variant = vk::apiVersionVariant(instanceVersion);
    if (variant != 0u)
        logger::warn("Vulkan version variant is {}, not 0", variant);

    logger::debug("Highest supported Vulkan version: {}", ApiVersionToString(instanceVersion));

    if (instanceVersion < m_ApiVersion)
    {
        logger::error("Vulkan version {} not supported", ApiVersionToString(m_ApiVersion));
        return false;
    }

    // Extensions
    std::vector<vk::ExtensionProperties> supportedExtensions = vk::enumerateInstanceExtensionProperties();
    auto supportedExtensionNames =
        supportedExtensions | std::views::transform([](auto &props) { return props.extensionName.data(); });
    for (const auto &extension : supportedExtensionNames)
        logger::trace("Instance supports extension {}", extension);

    for (std::string_view extension : m_Extensions)
    {
        logger::info("Instance Extension {} is required", extension);
        if (std::ranges::find(supportedExtensionNames, extension) == supportedExtensionNames.end())
        {
            logger::error("Instance Extension {} not supported", extension);
            return false;
        }
    }

    // Layers
    std::vector<vk::LayerProperties> supportedLayers = vk::enumerateInstanceLayerProperties();
    auto supportedLayerNames =
        supportedLayers | std::views::transform([](auto &props) { return props.layerName.data(); });

    for (const auto &extension : supportedLayerNames)
        logger::trace("Instance supports layer {}", extension);

    for (std::string_view layer : m_Layers)
    {
        logger::info("Layer {} is required", layer);
        if (std::ranges::find(supportedLayerNames, layer) == supportedLayerNames.end())
        {
            logger::error("Instance Layer {} not supported", layer);
            return false;
        }
    }

    return true;
}

bool ApplicationBuilder::CheckPhysicalDevice(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    const auto &properties = device.getProperties2();

    const char *deviceName = properties.properties.deviceName.data();
    logger::info(
        "Found physical device {} ({})", deviceName, vk::to_string(properties.properties.deviceType)
    );

    // Api Version
    logger::debug(
        "Device {} supports at most Vulkan version: {}", deviceName,
        ApiVersionToString(properties.properties.apiVersion)
    );
    if (properties.properties.apiVersion < m_ApiVersion)
    {
        logger::warn("Device {} does not support Vulkan version {}", deviceName, ApiVersionToString(m_ApiVersion));
        return false;
    }

    // Extensions
    std::vector<vk::ExtensionProperties> supportedExtensions = device.enumerateDeviceExtensionProperties();
    auto supportedExtensionNames =
        supportedExtensions | std::views::transform([](auto &props) { return props.extensionName.data(); });
    for (const auto &extension : supportedExtensionNames)
        logger::trace("{} supports extension {}", deviceName, extension);

    for (std::string_view extension : m_DeviceExtensions)
    {
        if (std::ranges::find(supportedExtensionNames, extension) == supportedExtensionNames.end())
        {
            logger::warn("{} does not support Extension {}", deviceName, extension);
            return false;
        }
    }

    // Features
    Features supportedFeatures = m_Features;
    device.getFeatures2(supportedFeatures.GetFeatures2());
    if (m_Features.CompareFeatures(supportedFeatures))
    {
        logger::warn("{} does not support requested features", deviceName);
        return false;
    }

    // Queues
    auto queueFamilies = device.getQueueFamilyProperties2();
    for (size_t i = 0; i < queueFamilies.size(); i++)
        logger::trace(
            "{} has Queue Family {} ({}): {}", deviceName, i,
            queueFamilies[i].queueFamilyProperties.queueCount,
            vk::to_string(queueFamilies[i].queueFamilyProperties.queueFlags)
        );

    std::vector<uint32_t> queueFamilyIndices = FindQueueFamilyIndices(device, surface);

    for (size_t i = 0; i < m_QueueSpecs.size(); i++)
    {
        const QueueSpec &spec = m_QueueSpecs[i];
        const uint32_t queueFamilyIndex = queueFamilyIndices[i];
        if (queueFamilyIndex == vk::QueueFamilyIgnored && spec.Optional == false)
        {
            logger::warn(
                "Device {} does not have a queue supporting specification of queue {}", deviceName, spec.Name
            );
            return false;
        }

        queueFamilyIndices.push_back(queueFamilyIndex);
    }

    logger::info("{} is a suitable device", deviceName);
    return true;
}

std::vector<uint32_t> ApplicationBuilder::FindQueueFamilyIndices(
    vk::PhysicalDevice device, vk::SurfaceKHR surface
) const
{
    auto queueFamilies = device.getQueueFamilyProperties2();

    std::vector<uint32_t> queueFamilyIndices;

    auto checkPresent = [&](const QueueSpec &spec, uint32_t queueFamilyIndex) {
        if (surface == nullptr)
            throw configuration_error(
                std::format(
                    "Queue `{}` was specified to have the ability to present but no surface was "
                    "provided",
                    spec.Name
                )
            );

        return device.getSurfaceSupportKHR(queueFamilyIndex, surface);
    };

    auto checkExclusive = [&](const QueueSpec &spec, uint32_t familyIndex) {
        for (size_t i = 0; i < m_QueueSpecs.size(); i++)
        {
            const QueueSpec &otherSpec = m_QueueSpecs[i];
            if (std::ranges::contains(spec.Exclusive, otherSpec.Name) == false)
                continue;

            if (i >= queueFamilyIndices.size())
            {
                throw configuration_error(
                    std::format(
                        "Queue `{}` excludes queue `{}` and queue `{}` was declared before queue `{}`. Queue "
                        "`{}` should be declared before queue `{}`",
                        spec.Name, otherSpec.Name, spec.Name, otherSpec.Name, otherSpec.Name, spec.Name
                    )
                );
            }

            if (queueFamilyIndices[i] == familyIndex)
                return false;
        }

        return true;
    };

    auto findQueueFamilyIndex = [&](const QueueSpec &spec) {
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilies.size(); queueFamilyIndex++)
        {
            auto &queueProperties = queueFamilies[queueFamilyIndex].queueFamilyProperties;

            if (spec.Present && !checkPresent(spec, queueFamilyIndex))
                continue;

            if ((queueProperties.queueFlags & spec.Flags) != spec.Flags)
                continue;

            if (!checkExclusive(spec, queueFamilyIndex))
                continue;

            if (queueProperties.queueCount == 0)
                continue;

            return queueFamilyIndex;
        }

        return vk::QueueFamilyIgnored;
    };

    for (const QueueSpec &spec : m_QueueSpecs)
    {
        const uint32_t queueFamilyIndex = findQueueFamilyIndex(spec);
        if (queueFamilyIndex != vk::QueueFamilyIgnored)
            queueFamilies[queueFamilyIndex].queueFamilyProperties.queueCount--;
        queueFamilyIndices.push_back(queueFamilyIndex);
    }

    return queueFamilyIndices;
}

}