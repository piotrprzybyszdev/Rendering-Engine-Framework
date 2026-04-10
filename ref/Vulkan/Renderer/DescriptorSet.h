#pragma once

#include <vulkan/vulkan.hpp>

#include <list>
#include <span>
#include <vector>

namespace ref::vulkan
{

class DescriptorSet
{
public:
    DescriptorSet(
        vk::Device logicalDevice, uint32_t framesInFlight, vk::DescriptorSetLayout layout, vk::DescriptorPool pool,
        const std::vector<vk::DescriptorType> &types
    );
    ~DescriptorSet();

    DescriptorSet(const DescriptorSet &) = delete;
    DescriptorSet &operator=(const DescriptorSet &) = delete;

    [[nodiscard]] vk::DescriptorSet GetSet(uint32_t frameIndex) const;

    void UpdateAccelerationStructures(
        uint32_t binding, uint32_t frameIndex, std::vector<vk::AccelerationStructureKHR> &&structures
    );
    void UpdateBuffer(
        uint32_t binding, uint32_t frameIndex, vk::Buffer buffer, vk::DeviceSize size,
        vk::DeviceSize offset = 0
    );
    void UpdateImage(
        uint32_t binding, uint32_t frameIndex, vk::ImageView view, vk::Sampler sampler,
        vk::ImageLayout layout, uint32_t index = 0
    );

    void FlushUpdate(uint32_t frameIndex);

private:
    const vk::Device m_LogicalDevice;
    const uint32_t m_FramesInFlight;
    const vk::DescriptorPool m_Pool;

    const std::vector<vk::DescriptorType> m_Types;

    std::vector<vk::DescriptorSet> m_Sets;

    struct FrameDescriptors
    {
        std::list<std::vector<vk::AccelerationStructureKHR>> AccelerationStructures;
        std::list<vk::WriteDescriptorSetAccelerationStructureKHR> AccelerationStructureInfos;
        std::list<vk::DescriptorBufferInfo> BufferInfos;
        std::list<std::vector<vk::DescriptorImageInfo>> ImageInfos;

        std::vector<vk::WriteDescriptorSet> Writes;
    };

    std::vector<FrameDescriptors> m_Descriptors;

private:
    void AddWrite(uint32_t binding, uint32_t frameIndex, uint32_t arrayIndex = 0, uint32_t count = 1);
};

class DescriptorSetBuilder
{
public:
    DescriptorSetBuilder(vk::Device logicalDevice = nullptr);
    ~DescriptorSetBuilder();

    DescriptorSetBuilder(const DescriptorSetBuilder &) = delete;
    DescriptorSetBuilder &operator=(const DescriptorSetBuilder &) = delete;

    DescriptorSetBuilder(DescriptorSetBuilder &&descriptorSetBuilder) noexcept;
    DescriptorSetBuilder &operator=(DescriptorSetBuilder &&descriptorSetBuilder) noexcept;

    DescriptorSetBuilder &SetDescriptor(vk::DescriptorSetLayoutBinding binding, bool partial = false);
    
    int32_t GetMaxBinding() const;
    bool HasBinding(uint32_t binding) const;
    const vk::DescriptorSetLayoutBinding &GetBinding(uint32_t binding) const;

    [[nodiscard]] vk::DescriptorSetLayout CreateLayout();
    [[nodiscard]] std::unique_ptr<DescriptorSet> CreateSetUnique(uint32_t framesInFlight) const;

private:
    vk::Device m_LogicalDevice;

    std::vector<vk::DescriptorSetLayoutBinding> m_Bindings;
    std::vector<vk::DescriptorType> m_Types;
    std::vector<vk::DescriptorBindingFlags> m_Flags;
    std::vector<bool> m_IsUsed;

    vk::DescriptorSetLayout m_Layout;
};

}
