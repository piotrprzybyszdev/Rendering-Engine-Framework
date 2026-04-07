#include "Core/Core.h"

#include "DescriptorSet.h"

namespace ref::vulkan
{

DescriptorSet::DescriptorSet(
    vk::Device logicalDevice, uint32_t framesInFlight, vk::DescriptorSetLayout layout,
    vk::DescriptorPool pool, const std::vector<vk::DescriptorType> &types
)
    : m_LogicalDevice(logicalDevice), m_FramesInFlight(framesInFlight), m_Pool(pool), m_Types(types),
      m_Descriptors(framesInFlight)
{
    std::vector<vk::DescriptorSetLayout> layouts(m_FramesInFlight, layout);

    vk::DescriptorSetAllocateInfo allocateInfo(m_Pool, layouts);
    m_Sets = m_LogicalDevice.allocateDescriptorSets(allocateInfo);
}

DescriptorSet::~DescriptorSet()
{
    m_LogicalDevice.destroyDescriptorPool(m_Pool);
}

vk::DescriptorSet DescriptorSet::GetSet(uint32_t frameIndex) const
{
    return m_Sets[frameIndex];
}

void DescriptorSet::UpdateAccelerationStructures(
    uint32_t binding, uint32_t frameIndex, std::vector<vk::AccelerationStructureKHR> &&structures
)
{
    assert(frameIndex < m_FramesInFlight);
    assert(m_Types[binding] == vk::DescriptorType::eAccelerationStructureKHR);

    FrameDescriptors &desc = m_Descriptors[frameIndex];

    desc.AccelerationStructures.push_back(structures);
    desc.AccelerationStructureInfos.emplace_back(desc.AccelerationStructures.back());
    AddWrite(binding, frameIndex);
    desc.Writes.back().setPNext(&desc.AccelerationStructureInfos.back());
}

void DescriptorSet::UpdateBuffer(uint32_t binding, uint32_t frameIndex, vk::Buffer buffer, vk::DeviceSize size, vk::DeviceSize offset)
{
    assert(frameIndex < m_FramesInFlight);

    FrameDescriptors &desc = m_Descriptors[frameIndex];

    desc.BufferInfos.emplace_back(buffer, offset, size);
    AddWrite(binding, frameIndex);
    desc.Writes.back().setPBufferInfo(&desc.BufferInfos.back());
}

void DescriptorSet::UpdateImage(
    uint32_t binding, uint32_t frameIndex, vk::ImageView view, vk::Sampler sampler, vk::ImageLayout layout,
    uint32_t index
)
{
    assert(frameIndex < m_FramesInFlight);

    FrameDescriptors &desc = m_Descriptors[frameIndex];

    desc.ImageInfos.push_back({ vk::DescriptorImageInfo(sampler, view, layout) });
    AddWrite(binding, frameIndex, index);
    desc.Writes.back().setPImageInfo(&desc.ImageInfos.back().back());
}

void DescriptorSet::AddWrite(uint32_t binding, uint32_t frameIndex, uint32_t arrayIndex, uint32_t count)
{
    std::erase_if(m_Descriptors[frameIndex].Writes, [binding, arrayIndex](vk::WriteDescriptorSet write) {
        assert(
            !(write.dstBinding == binding && write.dstArrayElement == arrayIndex) ||
            write.descriptorCount == 1
        );
        return write.dstBinding == binding && write.dstArrayElement == arrayIndex;
    });

    m_Descriptors[frameIndex].Writes.emplace_back(
        m_Sets[frameIndex], binding, arrayIndex, count, m_Types[binding]
    );
}

void DescriptorSet::FlushUpdate(uint32_t frameIndex)
{
    FrameDescriptors &desc = m_Descriptors[frameIndex];

    m_LogicalDevice.updateDescriptorSets(desc.Writes, {});

    desc.AccelerationStructures.clear();
    desc.AccelerationStructureInfos.clear();
    desc.BufferInfos.clear();
    desc.ImageInfos.clear();
    desc.Writes.clear();
}

DescriptorSetBuilder::DescriptorSetBuilder(vk::Device logicalDevice) : m_LogicalDevice(logicalDevice)
{
}

DescriptorSetBuilder::~DescriptorSetBuilder()
{
    if (m_LogicalDevice)
        m_LogicalDevice.destroyDescriptorSetLayout(m_Layout);
}

DescriptorSetBuilder::DescriptorSetBuilder(DescriptorSetBuilder &&descriptorSetBuilder) noexcept
    : m_LogicalDevice(std::move(descriptorSetBuilder.m_LogicalDevice)),
      m_Bindings(std::move(descriptorSetBuilder.m_Bindings)),
      m_Types(std::move(descriptorSetBuilder.m_Types)), m_Flags(std::move(descriptorSetBuilder.m_Flags)),
      m_Layout(std::move(descriptorSetBuilder.m_Layout))
{
    descriptorSetBuilder.m_LogicalDevice = nullptr;
}

DescriptorSetBuilder &vulkan::DescriptorSetBuilder::operator=(
    DescriptorSetBuilder &&descriptorSetBuilder
) noexcept
{
    m_LogicalDevice = std::move(descriptorSetBuilder.m_LogicalDevice);
    m_Bindings = std::move(descriptorSetBuilder.m_Bindings);
    m_Types = std::move(descriptorSetBuilder.m_Types);
    m_Flags = std::move(descriptorSetBuilder.m_Flags);
    m_Layout = std::move(descriptorSetBuilder.m_Layout);

    descriptorSetBuilder.m_LogicalDevice = nullptr;
    return *this;
}

DescriptorSetBuilder &DescriptorSetBuilder::SetDescriptor(
    vk::DescriptorSetLayoutBinding binding, bool partial
)
{
    const uint32_t bindingIndex = binding.binding;

    if (bindingIndex + 1 > m_Bindings.size())
    {
        m_Types.resize(bindingIndex + 1);
        m_Flags.resize(bindingIndex + 1);
        m_Bindings.resize(bindingIndex + 1);
        m_IsUsed.resize(bindingIndex + 1);
    }

    m_Types[bindingIndex] = binding.descriptorType;

    m_Flags[bindingIndex] = !partial && binding.descriptorCount == 1
                                ? vk::DescriptorBindingFlags()
                                : vk::DescriptorBindingFlagBits::ePartiallyBound;
    m_Bindings[bindingIndex] = binding;
    m_IsUsed[bindingIndex] = true;

    return *this;
}

vk::DescriptorSetLayout DescriptorSetBuilder::CreateLayout()
{
    std::vector<vk::DescriptorBindingFlags> usedFlags;
    std::vector<vk::DescriptorSetLayoutBinding> usedBindings;

    for (size_t i = 0; i < m_Bindings.size(); i++)
    {
        if (!m_IsUsed[i])
            continue;

        usedFlags.push_back(m_Flags[i]);
        usedBindings.push_back(m_Bindings[i]);
    }

    vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsCreateInfo(usedFlags);
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), usedBindings);
    layoutCreateInfo.setPNext(&flagsCreateInfo);

    m_Layout = m_LogicalDevice.createDescriptorSetLayout(layoutCreateInfo);
    return m_Layout;
}

std::unique_ptr<DescriptorSet> DescriptorSetBuilder::CreateSetUnique(uint32_t framesInFlight) const
{
    std::vector<vk::DescriptorPoolSize> poolSizes = {};
    poolSizes.reserve(m_Types.size());
    for (const auto &binding : m_Bindings)
        poolSizes.emplace_back(binding.descriptorType, binding.descriptorCount * framesInFlight);

    vk::DescriptorPoolCreateInfo poolCreateInfo(vk::DescriptorPoolCreateFlags(), framesInFlight, poolSizes);
    auto pool = m_LogicalDevice.createDescriptorPool(poolCreateInfo);

    return std::make_unique<DescriptorSet>(m_LogicalDevice, framesInFlight, m_Layout, pool, m_Types);
}

}
