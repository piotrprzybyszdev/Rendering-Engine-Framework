#include "Features.h"

namespace ref::vulkan
{

template<typename F> static const void *GetNext(const void *features)
{
    return reinterpret_cast<const F *>(features)->pNext;
}

void Features::SetFeatures(const vk::PhysicalDeviceFeatures2 &features)
{
    CombineFeatures2(&features);
}

void Features::SetFeatures(const vk::PhysicalDeviceVulkan11Features &features)
{
    CombineFeaturesVulkan11(&features);
}

void Features::SetFeatures(const vk::PhysicalDeviceVulkan12Features &features)
{
    CombineFeaturesVulkan12(&features);
}

void Features::SetFeatures(const vk::PhysicalDeviceVulkan13Features &features)
{
    CombineFeaturesVulkan13(&features);
}

void Features::SetFeatures(const vk::PhysicalDeviceVulkan14Features &features)
{
    CombineFeaturesVulkan14(&features);
}

Features::Features()
{
    InitChain();
}

Features::Features(const Features &other)
{
    m_Features2 = other.m_Features2;
    m_FeaturesVulkan11 = other.m_FeaturesVulkan11;
    m_FeaturesVulkan12 = other.m_FeaturesVulkan12;
    m_FeaturesVulkan13 = other.m_FeaturesVulkan13;
    m_FeaturesVulkan14 = other.m_FeaturesVulkan14;

    InitChain();

    const void **pNext = const_cast<const void **>(&m_Features2.pNext);

    int i = 0;
    while (*pNext != nullptr)
    {
        switch (other.m_ChainTypes[i])
        {
        case vk::StructureType::ePhysicalDeviceVulkan11Features:
            *pNext = GetNext<vk::PhysicalDeviceVulkan11Features>(pNext);
            break;
        case vk::StructureType::ePhysicalDeviceVulkan12Features:
            *pNext = GetNext<vk::PhysicalDeviceVulkan12Features>(pNext);
            break;
        case vk::StructureType::ePhysicalDeviceVulkan13Features:
            *pNext = GetNext<vk::PhysicalDeviceVulkan13Features>(pNext);
            break;
        case vk::StructureType::ePhysicalDeviceVulkan14Features:
            *pNext = GetNext<vk::PhysicalDeviceVulkan14Features>(pNext);
            break;
        default:
            std::terminate();
        }
        i++;
    }
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
        case vk::StructureType::ePhysicalDeviceVulkan11Features:
            if (!CompareFeaturesVulkan11(pNext))
                return false;
            pNext = GetNext<vk::PhysicalDeviceVulkan11Features>(pNext);
            break;
        case vk::StructureType::ePhysicalDeviceVulkan12Features:
            if (!CompareFeaturesVulkan12(pNext))
                return false;
            pNext = GetNext<vk::PhysicalDeviceVulkan12Features>(pNext);
            break;
        case vk::StructureType::ePhysicalDeviceVulkan13Features:
            if (!CompareFeaturesVulkan13(pNext))
                return false;
            pNext = GetNext<vk::PhysicalDeviceVulkan13Features>(pNext);
            break;
        case vk::StructureType::ePhysicalDeviceVulkan14Features:
            if (!CompareFeaturesVulkan14(pNext))
                return false;
            pNext = GetNext<vk::PhysicalDeviceVulkan14Features>(pNext);
            break;
        default:
            std::terminate();
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
    if (m_Features2.features.shaderStorageImageMultisample &&
        !features->features.shaderStorageImageMultisample)
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

bool Features::CompareFeaturesVulkan11(const void *other)
{
    const auto *features = reinterpret_cast<const vk::PhysicalDeviceVulkan11Features *>(other);
    if (m_FeaturesVulkan11.storageBuffer16BitAccess && !features->storageBuffer16BitAccess)
        return false;
    if (m_FeaturesVulkan11.uniformAndStorageBuffer16BitAccess &&
        !features->uniformAndStorageBuffer16BitAccess)
        return false;
    if (m_FeaturesVulkan11.storagePushConstant16 && !features->storagePushConstant16)
        return false;
    if (m_FeaturesVulkan11.storageInputOutput16 && !features->storageInputOutput16)
        return false;
    if (m_FeaturesVulkan11.multiview && !features->multiview)
        return false;
    if (m_FeaturesVulkan11.multiviewGeometryShader && !features->multiviewGeometryShader)
        return false;
    if (m_FeaturesVulkan11.multiviewTessellationShader && !features->multiviewTessellationShader)
        return false;
    if (m_FeaturesVulkan11.variablePointersStorageBuffer && !features->variablePointersStorageBuffer)
        return false;
    if (m_FeaturesVulkan11.variablePointers && !features->variablePointers)
        return false;
    if (m_FeaturesVulkan11.protectedMemory && !features->protectedMemory)
        return false;
    if (m_FeaturesVulkan11.samplerYcbcrConversion && !features->samplerYcbcrConversion)
        return false;
    if (m_FeaturesVulkan11.shaderDrawParameters && !features->shaderDrawParameters)
        return false;
    return true;
}

bool Features::CompareFeaturesVulkan12(const void *other)
{
    const auto *features = reinterpret_cast<const vk::PhysicalDeviceVulkan12Features *>(other);
    if (m_FeaturesVulkan12.samplerMirrorClampToEdge && features->samplerMirrorClampToEdge)
        return false;
    if (m_FeaturesVulkan12.drawIndirectCount && features->drawIndirectCount)
        return false;
    if (m_FeaturesVulkan12.storageBuffer8BitAccess && features->storageBuffer8BitAccess)
        return false;
    if (m_FeaturesVulkan12.uniformAndStorageBuffer8BitAccess && features->uniformAndStorageBuffer8BitAccess)
        return false;
    if (m_FeaturesVulkan12.storagePushConstant8 && features->storagePushConstant8)
        return false;
    if (m_FeaturesVulkan12.shaderBufferInt64Atomics && features->shaderBufferInt64Atomics)
        return false;
    if (m_FeaturesVulkan12.shaderSharedInt64Atomics && features->shaderSharedInt64Atomics)
        return false;
    if (m_FeaturesVulkan12.shaderFloat16 && features->shaderFloat16)
        return false;
    if (m_FeaturesVulkan12.shaderInt8 && features->shaderInt8)
        return false;
    if (m_FeaturesVulkan12.descriptorIndexing && features->descriptorIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderInputAttachmentArrayDynamicIndexing &&
        features->shaderInputAttachmentArrayDynamicIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderUniformTexelBufferArrayDynamicIndexing &&
        features->shaderUniformTexelBufferArrayDynamicIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderStorageTexelBufferArrayDynamicIndexing &&
        features->shaderStorageTexelBufferArrayDynamicIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderUniformBufferArrayNonUniformIndexing &&
        features->shaderUniformBufferArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderSampledImageArrayNonUniformIndexing &&
        features->shaderSampledImageArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderStorageBufferArrayNonUniformIndexing &&
        features->shaderStorageBufferArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderStorageImageArrayNonUniformIndexing &&
        features->shaderStorageImageArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderInputAttachmentArrayNonUniformIndexing &&
        features->shaderInputAttachmentArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderUniformTexelBufferArrayNonUniformIndexing &&
        features->shaderUniformTexelBufferArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.shaderStorageTexelBufferArrayNonUniformIndexing &&
        features->shaderStorageTexelBufferArrayNonUniformIndexing)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingUniformBufferUpdateAfterBind &&
        features->descriptorBindingUniformBufferUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingSampledImageUpdateAfterBind &&
        features->descriptorBindingSampledImageUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingStorageImageUpdateAfterBind &&
        features->descriptorBindingStorageImageUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingStorageBufferUpdateAfterBind &&
        features->descriptorBindingStorageBufferUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingUniformTexelBufferUpdateAfterBind &&
        features->descriptorBindingUniformTexelBufferUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingStorageTexelBufferUpdateAfterBind &&
        features->descriptorBindingStorageTexelBufferUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingUpdateUnusedWhilePending &&
        features->descriptorBindingUpdateUnusedWhilePending)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingPartiallyBound && features->descriptorBindingPartiallyBound)
        return false;
    if (m_FeaturesVulkan12.descriptorBindingVariableDescriptorCount &&
        features->descriptorBindingVariableDescriptorCount)
        return false;
    if (m_FeaturesVulkan12.runtimeDescriptorArray && features->runtimeDescriptorArray)
        return false;
    if (m_FeaturesVulkan12.samplerFilterMinmax && features->samplerFilterMinmax)
        return false;
    if (m_FeaturesVulkan12.scalarBlockLayout && features->scalarBlockLayout)
        return false;
    if (m_FeaturesVulkan12.imagelessFramebuffer && features->imagelessFramebuffer)
        return false;
    if (m_FeaturesVulkan12.uniformBufferStandardLayout && features->uniformBufferStandardLayout)
        return false;
    if (m_FeaturesVulkan12.shaderSubgroupExtendedTypes && features->shaderSubgroupExtendedTypes)
        return false;
    if (m_FeaturesVulkan12.separateDepthStencilLayouts && features->separateDepthStencilLayouts)
        return false;
    if (m_FeaturesVulkan12.hostQueryReset && features->hostQueryReset)
        return false;
    if (m_FeaturesVulkan12.timelineSemaphore && features->timelineSemaphore)
        return false;
    if (m_FeaturesVulkan12.bufferDeviceAddress && features->bufferDeviceAddress)
        return false;
    if (m_FeaturesVulkan12.bufferDeviceAddressCaptureReplay && features->bufferDeviceAddressCaptureReplay)
        return false;
    if (m_FeaturesVulkan12.bufferDeviceAddressMultiDevice && features->bufferDeviceAddressMultiDevice)
        return false;
    if (m_FeaturesVulkan12.vulkanMemoryModel && features->vulkanMemoryModel)
        return false;
    if (m_FeaturesVulkan12.vulkanMemoryModelDeviceScope && features->vulkanMemoryModelDeviceScope)
        return false;
    if (m_FeaturesVulkan12.vulkanMemoryModelAvailabilityVisibilityChains &&
        features->vulkanMemoryModelAvailabilityVisibilityChains)
        return false;
    if (m_FeaturesVulkan12.shaderOutputViewportIndex && features->shaderOutputViewportIndex)
        return false;
    if (m_FeaturesVulkan12.shaderOutputLayer && features->shaderOutputLayer)
        return false;
    if (m_FeaturesVulkan12.subgroupBroadcastDynamicId && features->subgroupBroadcastDynamicId)
        return false;
    return true;
}

bool Features::CompareFeaturesVulkan13(const void *other)
{
    const auto *features = reinterpret_cast<const vk::PhysicalDeviceVulkan13Features *>(other);
    if (m_FeaturesVulkan13.robustImageAccess && features->robustImageAccess)
        return false;
    if (m_FeaturesVulkan13.inlineUniformBlock && features->inlineUniformBlock)
        return false;
    if (m_FeaturesVulkan13.descriptorBindingInlineUniformBlockUpdateAfterBind &&
        features->descriptorBindingInlineUniformBlockUpdateAfterBind)
        return false;
    if (m_FeaturesVulkan13.pipelineCreationCacheControl && features->pipelineCreationCacheControl)
        return false;
    if (m_FeaturesVulkan13.privateData && features->privateData)
        return false;
    if (m_FeaturesVulkan13.shaderDemoteToHelperInvocation && features->shaderDemoteToHelperInvocation)
        return false;
    if (m_FeaturesVulkan13.shaderTerminateInvocation && features->shaderTerminateInvocation)
        return false;
    if (m_FeaturesVulkan13.subgroupSizeControl && features->subgroupSizeControl)
        return false;
    if (m_FeaturesVulkan13.computeFullSubgroups && features->computeFullSubgroups)
        return false;
    if (m_FeaturesVulkan13.synchronization2 && features->synchronization2)
        return false;
    if (m_FeaturesVulkan13.textureCompressionASTC_HDR && features->textureCompressionASTC_HDR)
        return false;
    if (m_FeaturesVulkan13.shaderZeroInitializeWorkgroupMemory &&
        features->shaderZeroInitializeWorkgroupMemory)
        return false;
    if (m_FeaturesVulkan13.dynamicRendering && features->dynamicRendering)
        return false;
    if (m_FeaturesVulkan13.shaderIntegerDotProduct && features->shaderIntegerDotProduct)
        return false;
    if (m_FeaturesVulkan13.maintenance4 && features->maintenance4)
        return false;
    return true;
}

bool Features::CompareFeaturesVulkan14(const void *other)
{
    const auto *features = reinterpret_cast<const vk::PhysicalDeviceVulkan14Features *>(other);
    if (m_FeaturesVulkan14.globalPriorityQuery && features->globalPriorityQuery)
        return false;
    if (m_FeaturesVulkan14.shaderSubgroupRotate && features->shaderSubgroupRotate)
        return false;
    if (m_FeaturesVulkan14.shaderSubgroupRotateClustered && features->shaderSubgroupRotateClustered)
        return false;
    if (m_FeaturesVulkan14.shaderFloatControls2 && features->shaderFloatControls2)
        return false;
    if (m_FeaturesVulkan14.shaderExpectAssume && features->shaderExpectAssume)
        return false;
    if (m_FeaturesVulkan14.rectangularLines && features->rectangularLines)
        return false;
    if (m_FeaturesVulkan14.bresenhamLines && features->bresenhamLines)
        return false;
    if (m_FeaturesVulkan14.smoothLines && features->smoothLines)
        return false;
    if (m_FeaturesVulkan14.stippledRectangularLines && features->stippledRectangularLines)
        return false;
    if (m_FeaturesVulkan14.stippledBresenhamLines && features->stippledBresenhamLines)
        return false;
    if (m_FeaturesVulkan14.stippledSmoothLines && features->stippledSmoothLines)
        return false;
    if (m_FeaturesVulkan14.vertexAttributeInstanceRateDivisor && features->vertexAttributeInstanceRateDivisor)
        return false;
    if (m_FeaturesVulkan14.vertexAttributeInstanceRateZeroDivisor &&
        features->vertexAttributeInstanceRateZeroDivisor)
        return false;
    if (m_FeaturesVulkan14.indexTypeUint8 && features->indexTypeUint8)
        return false;
    if (m_FeaturesVulkan14.dynamicRenderingLocalRead && features->dynamicRenderingLocalRead)
        return false;
    if (m_FeaturesVulkan14.maintenance5 && features->maintenance5)
        return false;
    if (m_FeaturesVulkan14.maintenance6 && features->maintenance6)
        return false;
    if (m_FeaturesVulkan14.pipelineProtectedAccess && features->pipelineProtectedAccess)
        return false;
    if (m_FeaturesVulkan14.pipelineRobustness && features->pipelineRobustness)
        return false;
    if (m_FeaturesVulkan14.hostImageCopy && features->hostImageCopy)
        return false;
    if (m_FeaturesVulkan14.pushDescriptor && features->pushDescriptor)
        return false;
    return true;
}

void Features::CombineFeatures2(const vk::PhysicalDeviceFeatures2 *other)
{
    m_Features2.features.robustBufferAccess |= other->features.robustBufferAccess;
    m_Features2.features.fullDrawIndexUint32 |= other->features.fullDrawIndexUint32;
    m_Features2.features.imageCubeArray |= other->features.imageCubeArray;
    m_Features2.features.independentBlend |= other->features.independentBlend;
    m_Features2.features.geometryShader |= other->features.geometryShader;
    m_Features2.features.tessellationShader |= other->features.tessellationShader;
    m_Features2.features.sampleRateShading |= other->features.sampleRateShading;
    m_Features2.features.dualSrcBlend |= other->features.dualSrcBlend;
    m_Features2.features.logicOp |= other->features.logicOp;
    m_Features2.features.multiDrawIndirect |= other->features.multiDrawIndirect;
    m_Features2.features.drawIndirectFirstInstance |= other->features.drawIndirectFirstInstance;
    m_Features2.features.depthClamp |= other->features.depthClamp;
    m_Features2.features.depthBiasClamp |= other->features.depthBiasClamp;
    m_Features2.features.fillModeNonSolid |= other->features.fillModeNonSolid;
    m_Features2.features.depthBounds |= other->features.depthBounds;
    m_Features2.features.wideLines |= other->features.wideLines;
    m_Features2.features.largePoints |= other->features.largePoints;
    m_Features2.features.alphaToOne |= other->features.alphaToOne;
    m_Features2.features.multiViewport |= other->features.multiViewport;
    m_Features2.features.samplerAnisotropy |= other->features.samplerAnisotropy;
    m_Features2.features.textureCompressionETC2 |= other->features.textureCompressionETC2;
    m_Features2.features.textureCompressionASTC_LDR |= other->features.textureCompressionASTC_LDR;
    m_Features2.features.textureCompressionBC |= other->features.textureCompressionBC;
    m_Features2.features.occlusionQueryPrecise |= other->features.occlusionQueryPrecise;
    m_Features2.features.pipelineStatisticsQuery |= other->features.pipelineStatisticsQuery;
    m_Features2.features.vertexPipelineStoresAndAtomics |= other->features.vertexPipelineStoresAndAtomics;
    m_Features2.features.fragmentStoresAndAtomics |= other->features.fragmentStoresAndAtomics;
    m_Features2.features.shaderTessellationAndGeometryPointSize |=
        other->features.shaderTessellationAndGeometryPointSize;
    m_Features2.features.shaderImageGatherExtended |= other->features.shaderImageGatherExtended;
    m_Features2.features.shaderStorageImageExtendedFormats |=
        other->features.shaderStorageImageExtendedFormats;
    m_Features2.features.shaderStorageImageMultisample |= other->features.shaderStorageImageMultisample;
    m_Features2.features.shaderStorageImageReadWithoutFormat |=
        other->features.shaderStorageImageReadWithoutFormat;
    m_Features2.features.shaderStorageImageWriteWithoutFormat |=
        other->features.shaderStorageImageWriteWithoutFormat;
    m_Features2.features.shaderUniformBufferArrayDynamicIndexing |=
        other->features.shaderUniformBufferArrayDynamicIndexing;
    m_Features2.features.shaderSampledImageArrayDynamicIndexing |=
        other->features.shaderSampledImageArrayDynamicIndexing;
    m_Features2.features.shaderStorageBufferArrayDynamicIndexing |=
        other->features.shaderStorageBufferArrayDynamicIndexing;
    m_Features2.features.shaderStorageImageArrayDynamicIndexing |=
        other->features.shaderStorageImageArrayDynamicIndexing;
    m_Features2.features.shaderClipDistance |= other->features.shaderClipDistance;
    m_Features2.features.shaderCullDistance |= other->features.shaderCullDistance;
    m_Features2.features.shaderFloat64 |= other->features.shaderFloat64;
    m_Features2.features.shaderInt64 |= other->features.shaderInt64;
    m_Features2.features.shaderInt16 |= other->features.shaderInt16;
    m_Features2.features.shaderResourceResidency |= other->features.shaderResourceResidency;
    m_Features2.features.shaderResourceMinLod |= other->features.shaderResourceMinLod;
    m_Features2.features.sparseBinding |= other->features.sparseBinding;
    m_Features2.features.sparseResidencyBuffer |= other->features.sparseResidencyBuffer;
    m_Features2.features.sparseResidencyImage2D |= other->features.sparseResidencyImage2D;
    m_Features2.features.sparseResidencyImage3D |= other->features.sparseResidencyImage3D;
    m_Features2.features.sparseResidency2Samples |= other->features.sparseResidency2Samples;
    m_Features2.features.sparseResidency4Samples |= other->features.sparseResidency4Samples;
    m_Features2.features.sparseResidency8Samples |= other->features.sparseResidency8Samples;
    m_Features2.features.sparseResidency16Samples |= other->features.sparseResidency16Samples;
    m_Features2.features.sparseResidencyAliased |= other->features.sparseResidencyAliased;
    m_Features2.features.variableMultisampleRate |= other->features.variableMultisampleRate;
    m_Features2.features.inheritedQueries |= other->features.inheritedQueries;
}

void Features::CombineFeaturesVulkan11(const vk::PhysicalDeviceVulkan11Features *other)
{
    m_FeaturesVulkan11.storageBuffer16BitAccess |= other->storageBuffer16BitAccess;
    m_FeaturesVulkan11.uniformAndStorageBuffer16BitAccess |= other->uniformAndStorageBuffer16BitAccess;
    m_FeaturesVulkan11.storagePushConstant16 |= other->storagePushConstant16;
    m_FeaturesVulkan11.storageInputOutput16 |= other->storageInputOutput16;
    m_FeaturesVulkan11.multiview |= other->multiview;
    m_FeaturesVulkan11.multiviewGeometryShader |= other->multiviewGeometryShader;
    m_FeaturesVulkan11.multiviewTessellationShader |= other->multiviewTessellationShader;
    m_FeaturesVulkan11.variablePointersStorageBuffer |= other->variablePointersStorageBuffer;
    m_FeaturesVulkan11.variablePointers |= other->variablePointers;
    m_FeaturesVulkan11.protectedMemory |= other->protectedMemory;
    m_FeaturesVulkan11.samplerYcbcrConversion |= other->samplerYcbcrConversion;
    m_FeaturesVulkan11.shaderDrawParameters |= other->shaderDrawParameters;
}

void Features::CombineFeaturesVulkan12(const vk::PhysicalDeviceVulkan12Features *other)
{
    m_FeaturesVulkan12.samplerMirrorClampToEdge |= other->samplerMirrorClampToEdge;
    m_FeaturesVulkan12.drawIndirectCount |= other->drawIndirectCount;
    m_FeaturesVulkan12.storageBuffer8BitAccess |= other->storageBuffer8BitAccess;
    m_FeaturesVulkan12.uniformAndStorageBuffer8BitAccess |= other->uniformAndStorageBuffer8BitAccess;
    m_FeaturesVulkan12.storagePushConstant8 |= other->storagePushConstant8;
    m_FeaturesVulkan12.shaderBufferInt64Atomics |= other->shaderBufferInt64Atomics;
    m_FeaturesVulkan12.shaderSharedInt64Atomics |= other->shaderSharedInt64Atomics;
    m_FeaturesVulkan12.shaderFloat16 |= other->shaderFloat16;
    m_FeaturesVulkan12.shaderInt8 |= other->shaderInt8;
    m_FeaturesVulkan12.descriptorIndexing |= other->descriptorIndexing;
    m_FeaturesVulkan12.shaderInputAttachmentArrayDynamicIndexing |=
        other->shaderInputAttachmentArrayDynamicIndexing;
    m_FeaturesVulkan12.shaderUniformTexelBufferArrayDynamicIndexing |=
        other->shaderUniformTexelBufferArrayDynamicIndexing;
    m_FeaturesVulkan12.shaderStorageTexelBufferArrayDynamicIndexing |=
        other->shaderStorageTexelBufferArrayDynamicIndexing;
    m_FeaturesVulkan12.shaderUniformBufferArrayNonUniformIndexing |=
        other->shaderUniformBufferArrayNonUniformIndexing;
    m_FeaturesVulkan12.shaderSampledImageArrayNonUniformIndexing |=
        other->shaderSampledImageArrayNonUniformIndexing;
    m_FeaturesVulkan12.shaderStorageBufferArrayNonUniformIndexing |=
        other->shaderStorageBufferArrayNonUniformIndexing;
    m_FeaturesVulkan12.shaderStorageImageArrayNonUniformIndexing |=
        other->shaderStorageImageArrayNonUniformIndexing;
    m_FeaturesVulkan12.shaderInputAttachmentArrayNonUniformIndexing |=
        other->shaderInputAttachmentArrayNonUniformIndexing;
    m_FeaturesVulkan12.shaderUniformTexelBufferArrayNonUniformIndexing |=
        other->shaderUniformTexelBufferArrayNonUniformIndexing;
    m_FeaturesVulkan12.shaderStorageTexelBufferArrayNonUniformIndexing |=
        other->shaderStorageTexelBufferArrayNonUniformIndexing;
    m_FeaturesVulkan12.descriptorBindingUniformBufferUpdateAfterBind |=
        other->descriptorBindingUniformBufferUpdateAfterBind;
    m_FeaturesVulkan12.descriptorBindingSampledImageUpdateAfterBind |=
        other->descriptorBindingSampledImageUpdateAfterBind;
    m_FeaturesVulkan12.descriptorBindingStorageImageUpdateAfterBind |=
        other->descriptorBindingStorageImageUpdateAfterBind;
    m_FeaturesVulkan12.descriptorBindingStorageBufferUpdateAfterBind |=
        other->descriptorBindingStorageBufferUpdateAfterBind;
    m_FeaturesVulkan12.descriptorBindingUniformTexelBufferUpdateAfterBind |=
        other->descriptorBindingUniformTexelBufferUpdateAfterBind;
    m_FeaturesVulkan12.descriptorBindingStorageTexelBufferUpdateAfterBind |=
        other->descriptorBindingStorageTexelBufferUpdateAfterBind;
    m_FeaturesVulkan12.descriptorBindingUpdateUnusedWhilePending |=
        other->descriptorBindingUpdateUnusedWhilePending;
    m_FeaturesVulkan12.descriptorBindingPartiallyBound |= other->descriptorBindingPartiallyBound;
    m_FeaturesVulkan12.descriptorBindingVariableDescriptorCount |=
        other->descriptorBindingVariableDescriptorCount;
    m_FeaturesVulkan12.runtimeDescriptorArray |= other->runtimeDescriptorArray;
    m_FeaturesVulkan12.samplerFilterMinmax |= other->samplerFilterMinmax;
    m_FeaturesVulkan12.scalarBlockLayout |= other->scalarBlockLayout;
    m_FeaturesVulkan12.imagelessFramebuffer |= other->imagelessFramebuffer;
    m_FeaturesVulkan12.uniformBufferStandardLayout |= other->uniformBufferStandardLayout;
    m_FeaturesVulkan12.shaderSubgroupExtendedTypes |= other->shaderSubgroupExtendedTypes;
    m_FeaturesVulkan12.separateDepthStencilLayouts |= other->separateDepthStencilLayouts;
    m_FeaturesVulkan12.hostQueryReset |= other->hostQueryReset;
    m_FeaturesVulkan12.timelineSemaphore |= other->timelineSemaphore;
    m_FeaturesVulkan12.bufferDeviceAddress |= other->bufferDeviceAddress;
    m_FeaturesVulkan12.bufferDeviceAddressCaptureReplay |= other->bufferDeviceAddressCaptureReplay;
    m_FeaturesVulkan12.bufferDeviceAddressMultiDevice |= other->bufferDeviceAddressMultiDevice;
    m_FeaturesVulkan12.vulkanMemoryModel |= other->vulkanMemoryModel;
    m_FeaturesVulkan12.vulkanMemoryModelDeviceScope |= other->vulkanMemoryModelDeviceScope;
    m_FeaturesVulkan12.vulkanMemoryModelAvailabilityVisibilityChains |=
        other->vulkanMemoryModelAvailabilityVisibilityChains;
    m_FeaturesVulkan12.shaderOutputViewportIndex |= other->shaderOutputViewportIndex;
    m_FeaturesVulkan12.shaderOutputLayer |= other->shaderOutputLayer;
    m_FeaturesVulkan12.subgroupBroadcastDynamicId |= other->subgroupBroadcastDynamicId;
}

void Features::CombineFeaturesVulkan13(const vk::PhysicalDeviceVulkan13Features *other)
{
    m_FeaturesVulkan13.robustImageAccess |= other->robustImageAccess;
    m_FeaturesVulkan13.inlineUniformBlock |= other->inlineUniformBlock;
    m_FeaturesVulkan13.descriptorBindingInlineUniformBlockUpdateAfterBind |=
        other->descriptorBindingInlineUniformBlockUpdateAfterBind;
    m_FeaturesVulkan13.pipelineCreationCacheControl |= other->pipelineCreationCacheControl;
    m_FeaturesVulkan13.privateData |= other->privateData;
    m_FeaturesVulkan13.shaderDemoteToHelperInvocation |= other->shaderDemoteToHelperInvocation;
    m_FeaturesVulkan13.shaderTerminateInvocation |= other->shaderTerminateInvocation;
    m_FeaturesVulkan13.subgroupSizeControl |= other->subgroupSizeControl;
    m_FeaturesVulkan13.computeFullSubgroups |= other->computeFullSubgroups;
    m_FeaturesVulkan13.synchronization2 |= other->synchronization2;
    m_FeaturesVulkan13.textureCompressionASTC_HDR |= other->textureCompressionASTC_HDR;
    m_FeaturesVulkan13.shaderZeroInitializeWorkgroupMemory |= other->shaderZeroInitializeWorkgroupMemory;
    m_FeaturesVulkan13.dynamicRendering |= other->dynamicRendering;
    m_FeaturesVulkan13.shaderIntegerDotProduct |= other->shaderIntegerDotProduct;
    m_FeaturesVulkan13.maintenance4 |= other->maintenance4;
}

void Features::CombineFeaturesVulkan14(const vk::PhysicalDeviceVulkan14Features *other)
{
    m_FeaturesVulkan14.globalPriorityQuery |= other->globalPriorityQuery;
    m_FeaturesVulkan14.shaderSubgroupRotate |= other->shaderSubgroupRotate;
    m_FeaturesVulkan14.shaderSubgroupRotateClustered |= other->shaderSubgroupRotateClustered;
    m_FeaturesVulkan14.shaderFloatControls2 |= other->shaderFloatControls2;
    m_FeaturesVulkan14.shaderExpectAssume |= other->shaderExpectAssume;
    m_FeaturesVulkan14.rectangularLines |= other->rectangularLines;
    m_FeaturesVulkan14.bresenhamLines |= other->bresenhamLines;
    m_FeaturesVulkan14.smoothLines |= other->smoothLines;
    m_FeaturesVulkan14.stippledRectangularLines |= other->stippledRectangularLines;
    m_FeaturesVulkan14.stippledBresenhamLines |= other->stippledBresenhamLines;
    m_FeaturesVulkan14.stippledSmoothLines |= other->stippledSmoothLines;
    m_FeaturesVulkan14.vertexAttributeInstanceRateDivisor |= other->vertexAttributeInstanceRateDivisor;
    m_FeaturesVulkan14.vertexAttributeInstanceRateZeroDivisor |=
        other->vertexAttributeInstanceRateZeroDivisor;
    m_FeaturesVulkan14.indexTypeUint8 |= other->indexTypeUint8;
    m_FeaturesVulkan14.dynamicRenderingLocalRead |= other->dynamicRenderingLocalRead;
    m_FeaturesVulkan14.maintenance5 |= other->maintenance5;
    m_FeaturesVulkan14.maintenance6 |= other->maintenance6;
    m_FeaturesVulkan14.pipelineProtectedAccess |= other->pipelineProtectedAccess;
    m_FeaturesVulkan14.pipelineRobustness |= other->pipelineRobustness;
    m_FeaturesVulkan14.hostImageCopy |= other->hostImageCopy;
    m_FeaturesVulkan14.pushDescriptor |= other->pushDescriptor;
}

void Features::InitChain()
{
    m_Features2.pNext = &m_FeaturesVulkan11;
    m_FeaturesVulkan11.pNext = &m_FeaturesVulkan12;
    m_FeaturesVulkan12.pNext = &m_FeaturesVulkan13;
    m_FeaturesVulkan13.pNext = &m_FeaturesVulkan14;
    m_FeaturesVulkan14.pNext = nullptr;

    m_ChainTypes = {
        vk::StructureType::ePhysicalDeviceVulkan11Features,
        vk::StructureType::ePhysicalDeviceVulkan12Features,
        vk::StructureType::ePhysicalDeviceVulkan13Features,
        vk::StructureType::ePhysicalDeviceVulkan14Features,
    };
}

}