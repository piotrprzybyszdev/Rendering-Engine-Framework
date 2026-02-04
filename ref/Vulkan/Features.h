#pragma once

#include <vulkan/vulkan.hpp>

namespace ref::vulkan
{

class Features
{
public:
    bool CompareFeatures(const Features &other);
    vk::PhysicalDeviceFeatures2 *GetFeatures2();

    void SetFeatures(const vk::PhysicalDeviceFeatures2 &features);
    void SetFeatures(const vk::PhysicalDeviceSynchronization2Features &features);

private:
    vk::PhysicalDeviceFeatures2 m_Features2;
    vk::PhysicalDeviceSynchronization2Features m_Synchronization2;

    std::vector<vk::StructureType> m_ChainTypes;
    void **m_Tail = &m_Features2.pNext;

    bool CompareFeatures2(const void *other);
    bool CompareSynchronization2Features(const void *other);
};

}