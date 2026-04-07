#pragma once

#include <vulkan/vulkan.hpp>

namespace ref::vulkan
{

class Features
{
public:
    Features();
    Features(const Features &other);
    Features &operator=(const Features &) = delete;

    bool CompareFeatures(const Features &other);
    vk::PhysicalDeviceFeatures2 *GetFeatures2();

    void SetFeatures(const vk::PhysicalDeviceFeatures2 &features);
    void SetFeatures(const vk::PhysicalDeviceVulkan11Features &features);
    void SetFeatures(const vk::PhysicalDeviceVulkan12Features &features);
    void SetFeatures(const vk::PhysicalDeviceVulkan13Features &features);
    void SetFeatures(const vk::PhysicalDeviceVulkan14Features &features);

private:
    vk::PhysicalDeviceFeatures2 m_Features2;
    vk::PhysicalDeviceVulkan11Features m_FeaturesVulkan11;
    vk::PhysicalDeviceVulkan12Features m_FeaturesVulkan12;
    vk::PhysicalDeviceVulkan13Features m_FeaturesVulkan13;
    vk::PhysicalDeviceVulkan14Features m_FeaturesVulkan14;

    std::vector<vk::StructureType> m_ChainTypes;

    bool CompareFeatures2(const void *other);
    bool CompareFeaturesVulkan11(const void *other);
    bool CompareFeaturesVulkan12(const void *other);
    bool CompareFeaturesVulkan13(const void *other);
    bool CompareFeaturesVulkan14(const void *other);

    void CombineFeatures2(const vk::PhysicalDeviceFeatures2 *other);
    void CombineFeaturesVulkan11(const vk::PhysicalDeviceVulkan11Features *other);
    void CombineFeaturesVulkan12(const vk::PhysicalDeviceVulkan12Features *other);
    void CombineFeaturesVulkan13(const vk::PhysicalDeviceVulkan13Features *other);
    void CombineFeaturesVulkan14(const vk::PhysicalDeviceVulkan14Features *other);

    void InitChain();
};

}
