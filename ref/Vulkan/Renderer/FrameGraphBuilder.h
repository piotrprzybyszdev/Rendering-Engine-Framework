#pragma once

#include <vulkan/vulkan.hpp>

#include "FrameGraph.h"
#include "RenderPass.h"
#include "ResourceManager.h"

namespace ref::vulkan
{

class FrameGraphBuilder
{
public:
    FrameGraphBuilder() = default;
    FrameGraphBuilder(const FrameGraphBuilder &) = delete;
    FrameGraphBuilder &operator=(const FrameGraphBuilder &) = delete;

    void AddHostBuffer(std::string name, vk::BufferCreateInfo info, ResourceType type, bool buffered);
    void AddDeviceBuffer(std::string name, vk::BufferCreateInfo info, ResourceType type, bool buffered);

    void AddHostImage(
        std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, ResourceType type,
        bool buffered
    );
    void AddDeviceImage(
        std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, ResourceType type,
        bool buffered
    );

    void AddHostImage(std::string name, vk::ImageCreateInfo info, ResourceType type, bool buffered);
    void AddDeviceImage(std::string name, vk::ImageCreateInfo info, ResourceType type, bool buffered);

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
