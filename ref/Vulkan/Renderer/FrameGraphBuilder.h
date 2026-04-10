#pragma once

#include <vulkan/vulkan.hpp>

#include "FrameGraph.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "ShaderLibrary.h"

namespace ref::vulkan
{

class FrameGraphBuilder
{
public:
    FrameGraphBuilder() = default;
    FrameGraphBuilder(const FrameGraphBuilder &) = delete;
    FrameGraphBuilder &operator=(const FrameGraphBuilder &) = delete;

    void AddHostBuffer(std::string name, vk::BufferCreateInfo info, bool buffered, bool persistent);
    void AddDeviceBuffer(std::string name, vk::BufferCreateInfo info, bool buffered, bool persistent);

    void AddHostImage(
        std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, bool buffered,
        bool persistent

    );
    void AddDeviceImage(
        std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, bool buffered,
        bool persistent
    );

    void AddHostImage(std::string name, vk::ImageCreateInfo info, bool buffered, bool persistent);
    void AddDeviceImage(std::string name, vk::ImageCreateInfo info, bool buffered, bool persistent);

    void AddClearPass(std::string name, ClearPassSpec spec);
    void AddBlitPass(std::string name, BlitPassSpec spec);
    void AddComputePass(std::string name, ComputePassSpec spec);
    void AddGraphicsPass(std::string name, GraphicsPassSpec spec);
    void AddIndexedGraphicsPass(std::string name, IndexedGraphicsPassSpec spec);
    void AddIndexedIndirectGraphicsPass(std::string name, IndexedIndirectGraphicsPassSpec spec);
    void AddCustomGraphicsPass(std::string name, CustomGraphicsPassSpec spec);

    [[nodiscard]] std::unique_ptr<FrameGraph> CreateUnique(
        PipelineLibrary *pipelineLibrary, ResourceAllocator *resourceAllocator
    );

private:
    FrameGraphSpec m_Spec;

private:
    template<typename F, typename... Args>
    void Dispatch(const PassExecution &execution, F &&func, Args &&...args);
};

}
