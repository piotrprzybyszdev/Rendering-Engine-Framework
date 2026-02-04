#include "Features.h"

namespace ref::vulkan
{

template<typename F> static const void *GetNext(const void *features)
{
    return reinterpret_cast<const F *>(features)->pNext;
}

void Features::SetFeatures(const vk::PhysicalDeviceFeatures2 &features)
{
    m_Features2 = features;
}

void Features::SetFeatures(const vk::PhysicalDeviceSynchronization2Features &features)
{
    m_Synchronization2 = features;

    if (std::ranges::contains(m_ChainTypes, features.sType))
        return;

    *m_Tail = &m_Synchronization2;
    m_Tail = &m_Synchronization2.pNext;
    m_ChainTypes.push_back(m_Synchronization2.sType);
}

bool Features::CompareFeatures(const Features &other)
{
    if (CompareFeatures2(&other.m_Features2))
        return false;

    const void *pNext = m_Features2.pNext;
    int i = 0;
    while (pNext != nullptr)
    {
        switch (m_ChainTypes[i])
        {
        case vk::StructureType::ePhysicalDeviceSynchronization2Features:
            if (!CompareSynchronization2Features(pNext))
                return false;
            pNext = GetNext<vk::PhysicalDeviceSynchronization2Features>(pNext);
            break;
        default:
            throw std::runtime_error("Unsupported physical devcie features");
        }

        i++;
    }

    return true;
}

vk::PhysicalDeviceFeatures2 *Features::GetFeatures2()
{
    return &m_Features2;
}

bool Features::CompareFeatures2(const void *other)
{
    const auto *features = reinterpret_cast<const vk::PhysicalDeviceFeatures2 *>(other);

    if (m_Features2.features.robustBufferAccess && !features->features.robustBufferAccess)
        return false;
    if (m_Features2.features.fullDrawIndexUint32 && !features->features.fullDrawIndexUint32)
        return false;
    if (m_Features2.features.imageCubeArray && !features->features.imageCubeArray)
        return false;
    if (m_Features2.features.independentBlend && !features->features.independentBlend)
        return false;
    if (m_Features2.features.geometryShader && !features->features.geometryShader)
        return false;
    if (m_Features2.features.tessellationShader && !features->features.tessellationShader)
        return false;
    if (m_Features2.features.sampleRateShading && !features->features.sampleRateShading)
        return false;
    if (m_Features2.features.dualSrcBlend && !features->features.dualSrcBlend)
        return false;
    if (m_Features2.features.logicOp && !features->features.logicOp)
        return false;
    if (m_Features2.features.multiDrawIndirect && !features->features.multiDrawIndirect)
        return false;
    if (m_Features2.features.drawIndirectFirstInstance && !features->features.drawIndirectFirstInstance)
        return false;
    if (m_Features2.features.depthClamp && !features->features.depthClamp)
        return false;
    if (m_Features2.features.depthBiasClamp && !features->features.depthBiasClamp)
        return false;
    if (m_Features2.features.fillModeNonSolid && !features->features.fillModeNonSolid)
        return false;
    if (m_Features2.features.depthBounds && !features->features.depthBounds)
        return false;
    if (m_Features2.features.wideLines && !features->features.wideLines)
        return false;
    if (m_Features2.features.largePoints && !features->features.largePoints)
        return false;
    if (m_Features2.features.alphaToOne && !features->features.alphaToOne)
        return false;
    if (m_Features2.features.multiViewport && !features->features.multiViewport)
        return false;
    if (m_Features2.features.samplerAnisotropy && !features->features.samplerAnisotropy)
        return false;
    if (m_Features2.features.textureCompressionETC2 && !features->features.textureCompressionETC2)
        return false;
    if (m_Features2.features.textureCompressionASTC_LDR && !features->features.textureCompressionASTC_LDR)
        return false;
    if (m_Features2.features.textureCompressionBC && !features->features.textureCompressionBC)
        return false;
    if (m_Features2.features.occlusionQueryPrecise && !features->features.occlusionQueryPrecise)
        return false;
    if (m_Features2.features.pipelineStatisticsQuery && !features->features.pipelineStatisticsQuery)
        return false;
    if (m_Features2.features.vertexPipelineStoresAndAtomics &&
        !features->features.vertexPipelineStoresAndAtomics)
        return false;
    if (m_Features2.features.fragmentStoresAndAtomics && !features->features.fragmentStoresAndAtomics)
        return false;
    if (m_Features2.features.shaderTessellationAndGeometryPointSize &&
        !features->features.shaderTessellationAndGeometryPointSize)
        return false;
    if (m_Features2.features.shaderImageGatherExtended && !features->features.shaderImageGatherExtended)
        return false;
    if (m_Features2.features.shaderStorageImageExtendedFormats &&
        !features->features.shaderStorageImageExtendedFormats)
        return false;
    if (m_Features2.features.shaderStorageImageMultisample && !features->features.shaderStorageImageMultisample)
        return false;
    if (m_Features2.features.shaderStorageImageReadWithoutFormat &&
        !features->features.shaderStorageImageReadWithoutFormat)
        return false;
    if (m_Features2.features.shaderStorageImageWriteWithoutFormat &&
        !features->features.shaderStorageImageWriteWithoutFormat)
        return false;
    if (m_Features2.features.shaderUniformBufferArrayDynamicIndexing &&
        !features->features.shaderUniformBufferArrayDynamicIndexing)
        return false;
    if (m_Features2.features.shaderSampledImageArrayDynamicIndexing &&
        !features->features.shaderSampledImageArrayDynamicIndexing)
        return false;
    if (m_Features2.features.shaderStorageBufferArrayDynamicIndexing &&
        !features->features.shaderStorageBufferArrayDynamicIndexing)
        return false;
    if (m_Features2.features.shaderStorageImageArrayDynamicIndexing &&
        !features->features.shaderStorageImageArrayDynamicIndexing)
        return false;
    if (m_Features2.features.shaderClipDistance && !features->features.shaderClipDistance)
        return false;
    if (m_Features2.features.shaderCullDistance && !features->features.shaderCullDistance)
        return false;
    if (m_Features2.features.shaderFloat64 && !features->features.shaderFloat64)
        return false;
    if (m_Features2.features.shaderInt64 && !features->features.shaderInt64)
        return false;
    if (m_Features2.features.shaderInt16 && !features->features.shaderInt16)
        return false;
    if (m_Features2.features.shaderResourceResidency && !features->features.shaderResourceResidency)
        return false;
    if (m_Features2.features.shaderResourceMinLod && !features->features.shaderResourceMinLod)
        return false;
    if (m_Features2.features.sparseBinding && !features->features.sparseBinding)
        return false;
    if (m_Features2.features.sparseResidencyBuffer && !features->features.sparseResidencyBuffer)
        return false;
    if (m_Features2.features.sparseResidencyImage2D && !features->features.sparseResidencyImage2D)
        return false;
    if (m_Features2.features.sparseResidencyImage3D && !features->features.sparseResidencyImage3D)
        return false;
    if (m_Features2.features.sparseResidency2Samples && !features->features.sparseResidency2Samples)
        return false;
    if (m_Features2.features.sparseResidency4Samples && !features->features.sparseResidency4Samples)
        return false;
    if (m_Features2.features.sparseResidency8Samples && !features->features.sparseResidency8Samples)
        return false;
    if (m_Features2.features.sparseResidency16Samples && !features->features.sparseResidency16Samples)
        return false;
    if (m_Features2.features.sparseResidencyAliased && !features->features.sparseResidencyAliased)
        return false;
    if (m_Features2.features.variableMultisampleRate && !features->features.variableMultisampleRate)
        return false;
    if (m_Features2.features.inheritedQueries && !features->features.inheritedQueries)
        return false;

    return true;
}

bool Features::CompareSynchronization2Features(const void *other)
{
    const auto *features = reinterpret_cast<const vk::PhysicalDeviceSynchronization2Features *>(other);
    if (m_Synchronization2.synchronization2 && !features->synchronization2)
        return false;
    return true;
}

}