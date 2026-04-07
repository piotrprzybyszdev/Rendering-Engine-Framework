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

    void AddHostBuffer(std::string name, vk::BufferCreateInfo info, bool buffered);
    void AddDeviceBuffer(std::string name, vk::BufferCreateInfo info, bool buffered);

    void AddHostImage(
        std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, bool buffered
    );
    void AddDeviceImage(
        std::string name, vk::ImageCreateInfo info, vk::ImageViewCreateInfo viewInfo, bool buffered
    );

    void AddHostImage(std::string name, vk::ImageCreateInfo info, bool buffered);
    void AddDeviceImage(std::string name, vk::ImageCreateInfo info, bool buffered);

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
    template<auto F, typename... Args> void Dispatch(const PassExecution &execution, Args &&...args)
    {
        switch (execution.Type)
        {
        case PassType::Clear:
            F(m_Spec.ClearPasses.at(execution.Name), args...);
            break;
        case PassType::Blit:
            F(m_Spec.BlitPasses.at(execution.Name), args...);
            break;
        case PassType::Compute:
            F(m_Spec.ComputePasses.at(execution.Name), args...);
            break;
        case PassType::Graphics:
            F(m_Spec.GraphicsPasses.at(execution.Name), args...);
            break;
        case PassType::IndexedGraphics:
            F(m_Spec.IndexedGraphicsPasses.at(execution.Name), args...);
            break;
        case PassType::IndexedIndirectGraphics:
            F(m_Spec.IndexedIndirectGraphicsPasses.at(execution.Name), args...);
            break;
        case PassType::CustomGraphics:
            F(m_Spec.CustomGraphicsPasses.at(execution.Name), args...);
            break;
        }
    }
};

}
