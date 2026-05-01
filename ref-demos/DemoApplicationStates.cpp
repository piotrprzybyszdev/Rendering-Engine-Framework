#include <imgui.h>

#include <Vulkan/Application.h>
#include <Vulkan/Renderer/FrameGraphBuilder.h>

#include "DemoApplicationStates.h"

namespace ref::vulkan
{

DemoUserInterface::DemoUserInterface(UserInterfaceVulkanSpec spec) : UserInterface(spec) {}

void DemoUserInterface::OnDefineUI(float /* timeStep */)
{
    ImGui::Begin("Demos");

    if (ImGui::BeginListBox("Active Demo"))
    {
        for (const std::string &state : s_DemoStates)
            if (ImGui::Selectable(state.c_str(), false))
                Application::GetInstance()->SetNextState(state);
        ImGui::EndListBox();
    }

    ImGui::End();
}

void DemoUserInterface::AddState(const std::string &name)
{
    s_DemoStates.insert(name);
}

namespace
{

vk::Format GetUnormFormat(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eR8G8B8A8Srgb:
        return vk::Format::eR8G8B8A8Unorm;
    case vk::Format::eB8G8R8A8Srgb:
        return vk::Format::eB8G8R8A8Unorm;
    default:
        return format;
    }
}

vk::Format GetSurfaceFormat(SwapchainBuilder* swapchainBuilder)
{
    bool hasRGBA = false, hasBGRA = false;
    for (const auto& format : swapchainBuilder->GetSurfaceFormats())
    {
        if (format.format == vk::Format::eUndefined)
            return vk::Format::eR8G8B8A8Unorm;

        if (format.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear)
            continue;

        if (format.format == vk::Format::eR8G8B8A8Unorm)
            hasRGBA = true;
        if (format.format == vk::Format::eB8G8R8A8Unorm)
            hasBGRA = true;
    }

    if (hasRGBA)
        return vk::Format::eR8G8B8A8Unorm;
    if (hasBGRA)
        return vk::Format::eB8G8R8A8Unorm;

    return vk::Format::eUndefined;
}

uint32_t GetImageCount(SwapchainBuilder* swapchainBuilder, uint32_t imageCount)
{
    return std::clamp(imageCount, swapchainBuilder->GetMinImageCount(), swapchainBuilder->GetMaxImageCount());
}

}

DemoApplicationState::DemoApplicationState(const std::string &state) : m_State(state)
{
    DemoUserInterface::AddState(m_State);
}

void DemoApplicationState::OnEnter(ApplicationState * /* previous */)
{
    const ApplicationStateSpec &spec = Application::GetInstance()->GetApplicationStateSpec();

    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    const Queue &mainQueue = spec.Queues.at(Application::MainQueueName);
    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = mainQueue.FamilyIndex,
        .Queue = mainQueue.Handle,
        .ImageCount = m_SwapchainImageCount,
        .ImageFormat = m_UserInterfaceFormat,
    };

    m_UserInterface = std::make_unique<DemoUserInterface>(userInterfaceSpec);


    m_UserInterface->OnEnter();

    spec.SwapchainBuilder->SetSurfaceFormat(m_SwapchainFormat);
    spec.SwapchainBuilder->SetUsageFlags(m_SwapchainUsageFlags);
    spec.SwapchainBuilder->SetImageCount(m_SwapchainImageCount);
    Application::GetInstance()->SetRecreateSwapchain();
}

void DemoApplicationState::OnExit(ApplicationState * /* next */)
{
    m_UserInterface->OnExit();

    m_Renderer.reset();
    m_FrameGraph.reset();
    m_UserInterface.reset();
    m_ResourceAllocator.reset();
}

void DemoApplicationState::SetDefaultSwapchain()
{
    const auto &swapchainBuilder = Application::GetInstance()->GetApplicationStateSpec().SwapchainBuilder;
    swapchainBuilder->SetPresentMode(vk::PresentModeKHR::eFifo);
    SetSwapchainUsageFlags(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment);
    SetSwapchainImageCount(GetImageCount(swapchainBuilder, 2u));
    
    vk::Format format = GetSurfaceFormat(swapchainBuilder);
    if (format == vk::Format::eUndefined)
    {
        auto &errors = ErrorApplicationState::GetErrors();
        errors.clear();
        errors.push_back("Could not find any suitable swapchain formats");
        ErrorApplicationState::SetErrorState(m_State);
        return;
    }

    SetSwapchainFormat(format);
}

void DemoApplicationState::SetUserInterfaceFormat(vk::Format format)
{
    m_UserInterfaceFormat = format;
}

void DemoApplicationState::SetSwapchainFormat(vk::SurfaceFormatKHR format)
{
    m_SwapchainFormat = format;
}

void DemoApplicationState::SetSwapchainUsageFlags(vk::ImageUsageFlags usageFlags)
{
    m_SwapchainUsageFlags = usageFlags;
}

void DemoApplicationState::SetSwapchainImageCount(uint32_t imageCount)
{
    m_SwapchainImageCount = imageCount;
}

bool DemoApplicationState::EnsurePipelinesCompiled(std::initializer_list<ComputePipelineInstanceId> pipelines)
{
    PipelineLibrary &pipelineLibrary = Application::GetInstance()->GetPipelineLibrary();

    bool success = true;
    for (auto id : pipelines)
        success &= pipelineLibrary.CompilePipelineInstance(id);

    if (!success)
        SetShaderErrors();

    return success;
}

bool DemoApplicationState::EnsurePipelinesCompiled(std::initializer_list<GraphicsPipelineInstanceId> pipelines)
{
    PipelineLibrary &pipelineLibrary = Application::GetInstance()->GetPipelineLibrary();

    bool success = true;
    for (auto id : pipelines)
        success &= pipelineLibrary.CompilePipelineInstance(id);

    if (!success)
        SetShaderErrors();

    return success;
}

void DemoApplicationState::SetShaderErrors()
{
    auto errors = Application::GetInstance()->GetShaderLibrary().GetCompilationErrors();
    ErrorApplicationState::GetErrors() = std::vector(errors.begin(), errors.end());
    ErrorApplicationState::SetErrorState(m_State);
}

ComputeApplicationState::ComputeApplicationState(const ApplicationStateSpec &spec)
    : DemoApplicationState("Compute Demo State"), m_LogicalDevice(spec.LogicalDevice),
      m_MainQueue(spec.Queues.at(Application::MainQueueName)), m_PipelineLibrary(spec.PipelineLibrary)
{
    auto shaderId = spec.ShaderLibrary->GetShaderByPath("Shaders/gradient.comp");

    {
        auto pipeline = m_PipelineLibrary->AddPipeline(ComputePipelineInfo("Shader Toy Pipeline", shaderId));
        ComputePipelineInstanceInfo pipelineInstanceInfo("Shader Toy Pipeline Instance", pipeline);
        AddSpecializationConstant(pipelineInstanceInfo, 0, 1.0f);
        m_PipelineId = m_PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);
    }
}

ComputeApplicationState::~ComputeApplicationState() {}

void ComputeApplicationState::OnEnter(ApplicationState *previous)
{
    SetDefaultSwapchain();
    SetSwapchainUsageFlags(
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eColorAttachment
    );
    DemoApplicationState::OnEnter(previous);
    if (!DemoApplicationState::EnsurePipelinesCompiled({ m_PipelineId }))
        return;

    FrameGraphBuilder builder;

    {
        ComputePassSpec passSpec = {
            .Pipeline = m_PipelineId,
            .ImageBindings = {
                { FrameGraph::g_SwapchainImageResourceName, 0, nullptr, false, true },
            },
            .Dispatches = {
                {
                    .PushConstantData = std::as_bytes(std::span(&m_Time, 1)),
                }
            }
        };
        builder.AddComputePass("Compute Pass", passSpec);
    }

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .LoadOp = vk::AttachmentLoadOp::eLoad,
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    m_FrameGraph = builder.CreateUnique(m_PipelineLibrary, m_ResourceAllocator.get());

    RendererSpec rendererSpec = {
        .LogicalDevice = m_LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
}

void ComputeApplicationState::OnExit(ApplicationState *next)
{
    DemoApplicationState::OnExit(next);
}

void ComputeApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    m_FrameGraph->GetComputePassDynamicConfig("Compute Pass").GetDispatchCommand().front().Command = {
        static_cast<uint32_t>(std::ceil(swapchain->GetExtent().width / 8.)),
        static_cast<uint32_t>(std::ceil(swapchain->GetExtent().height / 8.)), 1u
    };

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void ComputeApplicationState::OnUpdate(float timeStep)
{
    m_FrameGraph->UpdateImage(FrameGraph::g_SwapchainImageResourceName);

    m_UserInterface->OnUpdate(timeStep);
    m_Time += timeStep / 1000.0f;

    if (m_Time > 5.0f)
    {
        m_Time -= 5.0f;
        ErrorApplicationState::ReloadShaders("Compute Demo State");
    }
}

void ComputeApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
}

TriangleApplicationState::TriangleApplicationState(const ApplicationStateSpec &spec)
    : DemoApplicationState("Triangle Demo State"), m_LogicalDevice(spec.LogicalDevice),
      m_MainQueue(spec.Queues.at(Application::MainQueueName)),
      m_PipelineLibrary(spec.PipelineLibrary)
{
    ShaderId vertexShader = spec.ShaderLibrary->GetShaderByPath("Shaders/posColor.vert");
    ShaderId fragmentShader = spec.ShaderLibrary->GetShaderByPath("Shaders/color.frag");

    struct Vertex
    {
        float Position[4];
        float Color[4];
    };

    {
        GraphicsPipelineInfo pipelineInfo = {
            .Name = "Triangle Pipeline",
            .VertexShaderId = vertexShader,
            .FragmentShaderId = fragmentShader,
        };

        auto pipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);

        GraphicsPipelineInstanceInfo pipelineInstanceInfo = {
            .Name = "Triangle Pipeline Instance",
            .PipelineId = pipelineId,
            .BindingDescriptions = { vk::VertexInputBindingDescription(0, sizeof(Vertex)) },
            .VertexInputs = { { 0, offsetof(Vertex, Position) }, { 0, offsetof(Vertex, Color) } },
            .ColorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
        };
        pipelineInstanceInfo.InputAssemblyState.setTopology(vk::PrimitiveTopology::eTriangleList);
        pipelineInstanceInfo.RasterizationState.setLineWidth(1.0f);
        pipelineInstanceInfo.AttachmentBlendStates.emplace_back().setColorWriteMask(
            vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
        );

        m_TrianglePiplineId = spec.PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);
    }
}

void TriangleApplicationState::OnEnter(ApplicationState *previous)
{
    SetDefaultSwapchain();
    DemoApplicationState::OnEnter(previous);
    if (!DemoApplicationState::EnsurePipelinesCompiled({ m_TrianglePiplineId }))
        return;

    FrameGraphBuilder builder;

    struct Vertex
    {
        float Position[4];
        float Color[4];
    };

    std::array<Vertex, 3> vertices = { {
        { { -1.0f, -1.0f, 0.5f, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
        { { 1.0f, -1.0f, 0.5f, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
        { { 0.0f, 1.0f, 0.5f, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } },
    } };

    std::array<uint32_t, 3> indices = { 0, 1, 2 };

    builder.AddDeviceBuffer(
        "Vertex Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(vertices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer),
        false, true
    );

    builder.AddDeviceBuffer(
        "Index Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(indices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer),
        false, true
    );

    {
        GraphicsPassSpec passSpec = {
            .Pipeline = m_TrianglePiplineId,
            .VertexBuffers = {
                .VertexBuffers = { { "Vertex Buffer" } },
            },
            .ColorAttachments = {
                {
                    .ImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .LoadOp = vk::AttachmentLoadOp::eClear,
                    .ClearValue = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f),
                },
            },
            .Draws = {
                {
                    .Command = {
                        .VertexCount = 3,
                        .InstanceCount = 1,
                        .FirstVertex = 0,
                        .FirstInstance = 0,
                    }
                }
            },
        };
        builder.AddGraphicsPass("Graphics Pass", passSpec);
    }

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .LoadOp = vk::AttachmentLoadOp::eLoad,
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    m_FrameGraph = builder.CreateUnique(m_PipelineLibrary, m_ResourceAllocator.get());

    auto v = std::span(vertices);
    auto i = std::span(indices);

    RendererSpec rendererSpec = {
        .LogicalDevice = m_LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);

    m_Renderer->UploadWithStaging(m_FrameGraph->GetBuffer("Vertex Buffer").front(), std::as_bytes(v));
    m_Renderer->UploadWithStaging(m_FrameGraph->GetBuffer("Index Buffer").front(), std::as_bytes(i));
}

void TriangleApplicationState::OnExit(ApplicationState *next)
{
    DemoApplicationState::OnExit(next);
}

void TriangleApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    m_FrameGraph->GetGraphicsPassDynamicConfig("Graphics Pass").GetScissors() = {
        vk::Rect2D(vk::Offset2D(0, 0), extent)
    };
    m_FrameGraph->GetGraphicsPassDynamicConfig("Graphics Pass").GetViewports() = {
        vk::Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1)
    };
    m_FrameGraph->GetGraphicsPassDynamicConfig("Graphics Pass").GetRenderArea().extent = extent;

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void TriangleApplicationState::OnUpdate(float timeStep)
{
    m_UserInterface->OnUpdate(timeStep);
}

void TriangleApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
}

ParticleApplicationState::ParticleApplicationState(const ApplicationStateSpec &spec)
    : DemoApplicationState("Particle Demo State"), m_LogicalDevice(spec.LogicalDevice),
      m_MainQueue(spec.Queues.at(Application::MainQueueName)),
      m_PipelineLibrary(spec.PipelineLibrary)
{
    struct Vertex
    {
        float Position[4];
    };

    ShaderId computeShader = spec.ShaderLibrary->GetShaderByPath("Shaders/particleDraw.comp");
    ShaderId vertexShader = spec.ShaderLibrary->GetShaderByPath("Shaders/pos.vert");
    ShaderId fragmentShader = spec.ShaderLibrary->GetShaderByPath("Shaders/color.frag");

    {
        GraphicsPipelineInfo pipelineInfo = {
            .Name = "Particle Graphics Pipeline",
            .VertexShaderId = vertexShader,
            .FragmentShaderId = fragmentShader,
        };

        auto pipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);

        GraphicsPipelineInstanceInfo pipelineInstanceInfo = {
            .Name = "Particle Graphics Pipeline Instance",
            .PipelineId = pipelineId,
            .BindingDescriptions = { vk::VertexInputBindingDescription(0, sizeof(Vertex)) },
            .VertexInputs = { { 0, offsetof(Vertex, Position) } },
            .ColorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
        };
        pipelineInstanceInfo.InputAssemblyState.setTopology(vk::PrimitiveTopology::eTriangleList);
        pipelineInstanceInfo.RasterizationState.setLineWidth(1.0f);
        pipelineInstanceInfo.AttachmentBlendStates.emplace_back().setColorWriteMask(
            vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
        );

        m_ParticlePipelineId = spec.PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);
    }

    {
        ComputePipelineInfo pipelineInfo = { .Name = "Particle Compute Pipeline",
                                             .ComputeShader = computeShader };
        auto pipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);
        ComputePipelineInstanceInfo pipelineInstanceInfo = {
            .Name = "Particle Compute Pipeline Instance",
            .PipelineId = pipelineId,
        };
        m_ComputePipelineId = spec.PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);
    }
}

void ParticleApplicationState::OnEnter(ApplicationState * previous)
{
    SetDefaultSwapchain();
    DemoApplicationState::OnEnter(previous);
    if (!DemoApplicationState::EnsurePipelinesCompiled({ m_ComputePipelineId }))
        return;
    if (!DemoApplicationState::EnsurePipelinesCompiled({ m_ParticlePipelineId }))
        return;

    struct Vertex
    {
        float Position[4];
    };

    std::array<Vertex, 9> vertices = { {
        { { -1.0f, -1.0f, 0.5f, 1.0f } },
        { { 1.0f, -1.0f, 0.5f, 1.0f } },
        { { 0.0f, 1.0f, 0.5f, 1.0f } },
        { { -1.0f, -1.0f, 0.5f, 1.0f } },
        { { 0.0f, 0.0f, 0.5f, 1.0f } },
        { { -1.0f, 1.0f, 0.5f, 1.0f } },
        { { 1.0f, 1.0f, 0.5f, 1.0f } },
        { { 0.0f, 0.0f, 0.5f, 1.0f } },
        { { 1.0f, -1.0f, 0.5f, 1.0f } },
    } };

    std::array<uint32_t, 3> indices = { 0, 1, 2 };

    FrameGraphBuilder builder;
    builder.AddDeviceBuffer(
        "Vertex Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(vertices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer),
        false, true
    );
    builder.AddDeviceBuffer(
        "Index Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(indices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer),
        false, true
    );
    builder.AddDeviceBuffer(
        "Indirect Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(vk::DrawIndexedIndirectCommand))
            .setUsage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer),
        true, false
    );

    builder.AddDeviceImage(
        "Image",
        vk::ImageCreateInfo(
            vk::ImageCreateFlags(), vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
            vk::Extent3D(1280, 720, 1), 1, 1
        )
            .setUsage(
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eColorAttachment
            ),
        true, false
    );

    {
        ClearPassSpec clearSpec = {
            .ImageResource = "Image",
            .Ranges = { vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) },
        };
        builder.AddClearPass("Clear Pass", std::move(clearSpec));
    }

    {
        ComputePassSpec computeSpec = {
            .Pipeline = m_ComputePipelineId,
            .BufferBindings = { BufferBinding("Indirect Buffer", 0, false, true) },
            .Dispatches = { {
                .Command = { 1, 1, 1 },
                .PushConstantData = std::as_bytes(std::span(&m_ParticleIdx, 1)),
            } }
        };
        builder.AddComputePass("Compute Pass", std::move(computeSpec));
    }

    {
        IndexedIndirectGraphicsPassSpec graphicsSpec = {
            .Pipeline = m_ParticlePipelineId,
            .VertexBuffers = {
                .VertexBuffers = { { "Vertex Buffer" } },
            },            
            .IndexBuffer = { "Index Buffer", 0, vk::IndexType::eUint32 },
            .ColorAttachments = { { "Image" } },
            .IndirectBufferResource = "Indirect Buffer",
            .Draws = { {
                .Command = {
                    .DrawCount = 1,
                },
            } }
        };
        builder.AddIndexedIndirectGraphicsPass("Graphics Pass", std::move(graphicsSpec));
    }

    {
        vk::ImageSubresourceLayers layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);

        BlitPassSpec blitSpec = {
            .SrcImageResource = "Image",
            .DstImageResource = FrameGraph::g_SwapchainImageResourceName,
            .Regions = { vk::ImageBlit2(layers, {}, layers, {}) },
        };
        builder.AddBlitPass("Blit Pass", blitSpec);
    }

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .LoadOp = vk::AttachmentLoadOp::eLoad,
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    m_FrameGraph = builder.CreateUnique(m_PipelineLibrary, m_ResourceAllocator.get());

    m_FrameGraph->GetClearPassDynamicConfig("Clear Pass").GetClearValue() =
        vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f);

    auto v = std::span(vertices);
    auto i = std::span(indices);

    RendererSpec rendererSpec = {
        .LogicalDevice = m_LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);

    m_Renderer->UploadWithStaging(m_FrameGraph->GetBuffer("Vertex Buffer").front(), std::as_bytes(v));
    m_Renderer->UploadWithStaging(m_FrameGraph->GetBuffer("Index Buffer").front(), std::as_bytes(i));
}

void ParticleApplicationState::OnExit(ApplicationState *next)
{
    DemoApplicationState::OnExit(next);
}

void ParticleApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    m_FrameGraph->ModifyImage("Image").Info.setExtent(vk::Extent3D(extent, 1));
    m_FrameGraph->UpdateImage("Image");

    std::array<vk::Offset3D, 2> offsets = { vk::Offset3D(), vk::Offset3D(extent.width, extent.height, 1) };

    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetSrcOffsets().front() = offsets;
    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetDstOffsets().front() = offsets;

    m_FrameGraph->GetIndexedIndirectGraphicsPassDynamicConfig("Graphics Pass").GetScissors() = {
        vk::Rect2D(vk::Offset2D(0, 0), extent)
    };
    m_FrameGraph->GetIndexedIndirectGraphicsPassDynamicConfig("Graphics Pass").GetViewports() = {
        vk::Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1)
    };
    m_FrameGraph->GetIndexedIndirectGraphicsPassDynamicConfig("Graphics Pass").GetRenderArea().extent =
        extent;

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void ParticleApplicationState::OnUpdate(float timeStep)
{
    m_Time += timeStep;
    if (m_Time >= 1000.0f)
    {
        m_Time -= 1000.0f;
        m_ParticleIdx = (m_ParticleIdx + 1) % 3;
    }

    m_UserInterface->OnUpdate(timeStep);
}

void ParticleApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
}

CubeApplicationState::CubeApplicationState(const ApplicationStateSpec &spec)
    : DemoApplicationState("Cube Demo State"), m_LogicalDevice(spec.LogicalDevice),
      m_MainQueue(spec.Queues.at(Application::MainQueueName)), m_PipelineLibrary(spec.PipelineLibrary)
{
    struct Vertex
    {
        float Position[4];
        float Color[4];
    };

    ShaderId vertexShader = spec.ShaderLibrary->GetShaderByPath("Shaders/mvp.vert");
    ShaderId fragmentShader = spec.ShaderLibrary->GetShaderByPath("Shaders/color.frag");

    {
        GraphicsPipelineInfo pipelineInfo = {
            .Name = "Cube Pipeline",
            .VertexShaderId = vertexShader,
            .FragmentShaderId = fragmentShader,
        };

        auto pipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);

        GraphicsPipelineInstanceInfo pipelineInstanceInfo = {
            .Name = "Cube Pipeline Instance",
            .PipelineId = pipelineId,
            .BindingDescriptions = { vk::VertexInputBindingDescription(0, sizeof(Vertex)) },
            .VertexInputs = { { 0, offsetof(Vertex, Position) }, { 0, offsetof(Vertex, Color) } },
            .ColorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
            .DepthAttachmentFormat = vk::Format::eD24UnormS8Uint,
            .StencilAttachmentFormat = vk::Format::eD24UnormS8Uint,
        };

        pipelineInstanceInfo.InputAssemblyState.setTopology(vk::PrimitiveTopology::eTriangleList);
        pipelineInstanceInfo.MultisampleState.setRasterizationSamples(vk::SampleCountFlagBits::e4);
        pipelineInstanceInfo.RasterizationState.setLineWidth(1.0f);
        pipelineInstanceInfo.AttachmentBlendStates.emplace_back().setColorWriteMask(
            vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
        );

        pipelineInstanceInfo.DepthStencilState.setDepthTestEnable(vk::True);
        pipelineInstanceInfo.DepthStencilState.setDepthWriteEnable(vk::True);
        pipelineInstanceInfo.DepthStencilState.setDepthCompareOp(vk::CompareOp::eLess);

        m_CubePipelineId = spec.PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);

        pipelineInstanceInfo.Name = "Mirror Pipeline Instance";
        pipelineInstanceInfo.DepthStencilState.setStencilTestEnable(vk::True);
        pipelineInstanceInfo.DepthStencilState.setBack(
            vk::StencilOpState()
                .setCompareOp(vk::CompareOp::eAlways)
                .setPassOp(vk::StencilOp::eReplace)
                .setWriteMask(0xff)
                .setReference(1)
        );

        m_MirrorPipelineId = spec.PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);

        pipelineInstanceInfo.Name = "Reflection Pipeline Instance";
        pipelineInstanceInfo.DepthStencilState.setBack(
            vk::StencilOpState().setCompareOp(vk::CompareOp::eEqual).setCompareMask(0xff).setReference(1)
        );
        pipelineInstanceInfo.AttachmentBlendStates.front()
            .setBlendEnable(vk::True)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd);

        m_ReflectionPipelineId = spec.PipelineLibrary->AddPipelineInstance(pipelineInstanceInfo);
    }
}

void CubeApplicationState::OnEnter(ApplicationState *previous)
{
    SetDefaultSwapchain();
    DemoApplicationState::OnEnter(previous);
    if (!DemoApplicationState::EnsurePipelinesCompiled(
            { m_CubePipelineId, m_MirrorPipelineId, m_ReflectionPipelineId }
        ))
        return;

    struct Vertex
    {
        float Position[4];
        float Color[4];
    };

    std::array<Vertex, 24> vertices = { {
        { { -1.0f, -1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, },
        { { 1.0f, -1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, },
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, },
        { { -1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, },

        { { 1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, },
        { { -1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, },
        { { -1.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, },
        { { 1.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, },

        { { -1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, },
        { { -1.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, },
        { { -1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, },
        { { -1.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, },

        { { 1.0f, -1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, },
        { { 1.0f, -1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, },
        { { 1.0f, 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, },
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, },

        { { -1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, },
        { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, },
        { { 1.0f, 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, },
        { { -1.0f, 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, },

        { { -1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f }, },
        { { 1.0f, -1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f }, },
        { { 1.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f }, },
        { { -1.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f }, },
    } };

    std::array<uint32_t, 36> indices = {
        0,  1,  2,  2,  3,  0,
        4,  5,  6,  6,  7,  4,
        8,  9,  10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20,
    };

    m_Matrices.CameraProjection = { {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
    } };
    m_Matrices.CameraView = { {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, -10.0f, 1.0f },
    } };
    m_Matrices.ModelTransform = { {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 5.0f, 1.0f },
    } };

    m_MirrorMatrices = m_Matrices;
    m_MirrorMatrices.ModelTransform = { {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { -8.0f, 0.0f, -5.0f, 1.0f },
    } };
    m_MirrorMatrices.ModelTransform[0][0] *= std::cos(-0.7f);
    m_MirrorMatrices.ModelTransform[0][2] = std::sin(-0.7f);
    m_MirrorMatrices.ModelTransform[2][0] = -std::sin(-0.7f);
    m_MirrorMatrices.ModelTransform[2][2] *= std::cos(-0.7f);

    m_ReflectionMatrices = m_Matrices;

    FrameGraphBuilder builder;
    builder.AddDeviceImage(
        "Image",
        vk::ImageCreateInfo(
            vk::ImageCreateFlags(), vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
            vk::Extent3D(1280, 720, 1), 1, 1
        )
            .setSamples(vk::SampleCountFlagBits::e4)
            .setUsage(
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eColorAttachment
            ),
        true, false
    );

    builder.AddDeviceBuffer(
        "Vertex Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(vertices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer),
        false, true
    );
    builder.AddDeviceBuffer(
        "Index Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(indices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer),
        false, true
    );
    builder.AddHostBuffer(
        "Matrix Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(MatrixBuffer))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer),
        true, false
    );
    builder.AddHostBuffer(
        "Mirror Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(MatrixBuffer))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer),
        true, false
    );
    builder.AddHostBuffer(
        "Reflection Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(MatrixBuffer))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer),
        true, false
    );

    builder.AddDeviceImage(
        "Depth Stencil Image",
        vk::ImageCreateInfo(
            vk::ImageCreateFlags(), vk::ImageType::e2D, vk::Format::eD24UnormS8Uint,
            vk::Extent3D(1280, 720, 1), 1, 1, vk::SampleCountFlagBits::e4
        )
            .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment),
        vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vk::Format::eD24UnormS8Uint)
            .setSubresourceRange(
                vk::ImageSubresourceRange(
                    vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1
                )
            ),
        true, false
    );

    {
        IndexedGraphicsPassSpec passSpec = {
            .Pipeline = m_CubePipelineId,
            .BufferBindings = {
                { "Matrix Buffer", 0, true, false },
            },
            .VertexBuffers = {
                .VertexBuffers = { { "Vertex Buffer" } },
            },
            .IndexBuffer = { "Index Buffer", 0, vk::IndexType::eUint32 },
            .ColorAttachments = {
                {
                    .ImageResource = "Image",
                    .LoadOp = vk::AttachmentLoadOp::eClear,
                    .ClearValue = vk::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f),
                },
            },
            .DepthAttachment = { {
                .ImageResource = "Depth Stencil Image",
                .LoadOp = vk::AttachmentLoadOp::eClear,
                .ClearValue = vk::ClearDepthStencilValue(1.0f),
            } },
            .Draws = { {
                .Command = {
                    .IndexCount = static_cast<uint32_t>(indices.size()),
                    .InstanceCount = 1,
                    .FirstIndex = 0,
                    .VertexOffset = 0,
                    .FirstInstance = 0,
                },
            } },
        };
        builder.AddIndexedGraphicsPass("Main Pass", passSpec);
    }

    {
        IndexedGraphicsPassSpec passSpec = {
            .Pipeline = m_MirrorPipelineId,
            .BufferBindings = {
                { "Mirror Buffer", 0, true, false },
            },
            .VertexBuffers = {
                .VertexBuffers = { { "Vertex Buffer" } },
            },            
            .IndexBuffer = { "Index Buffer", 0, vk::IndexType::eUint32 },
            .ColorAttachments = {
                {
                    .ImageResource = "Image",
                    .LoadOp = vk::AttachmentLoadOp::eLoad,
                },
            },
            .DepthAttachment = { {
                .ImageResource = "Depth Stencil Image",
                .LoadOp = vk::AttachmentLoadOp::eLoad,
            } },
            .StencilAttachment = { {
                .ImageResource = "Depth Stencil Image",
                .LoadOp = vk::AttachmentLoadOp::eClear,
                .ClearValue = vk::ClearDepthStencilValue(1.0f, 0),
            } },
            .Draws = {{
                .Command = {
                    .IndexCount = 6,
                    .InstanceCount = 1,
                    .FirstIndex = 0,
                    .VertexOffset = 0,
                    .FirstInstance = 0,
                },
            }},
        };
        builder.AddIndexedGraphicsPass("Mirror Pass", passSpec);
    }

    {
        IndexedGraphicsPassSpec passSpec = {
            .Pipeline = m_ReflectionPipelineId,
            .BufferBindings = {
                { "Reflection Buffer", 0, true, false },
            },
            .VertexBuffers = {
                .VertexBuffers = { { "Vertex Buffer" } },
            },
            .IndexBuffer = { "Index Buffer", 0, vk::IndexType::eUint32 },
            .ColorAttachments = {
                {
                    .ImageResource = "Image",
                    .LoadOp = vk::AttachmentLoadOp::eLoad,
                    .ResolveImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .ResolveMode = vk::ResolveModeFlagBits::eAverage,
                },
            },
            .DepthAttachment = { {
                .ImageResource = "Depth Stencil Image",
                .LoadOp = vk::AttachmentLoadOp::eClear,
                .ClearValue = vk::ClearDepthStencilValue(1.0f),
            } },
            .StencilAttachment = { {
                .ImageResource = "Depth Stencil Image",
                .LoadOp = vk::AttachmentLoadOp::eLoad,
            } },
            .Draws = {{
                .Command = {
                    .IndexCount = static_cast<uint32_t>(indices.size()),
                    .InstanceCount = 1,
                    .FirstIndex = 0,
                    .VertexOffset = 0,
                    .FirstInstance = 0,
                },
            }},
        };
        builder.AddIndexedGraphicsPass("Reflection Pass", passSpec);
    }

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageResource = FrameGraph::g_SwapchainImageResourceName,
                    .LoadOp = vk::AttachmentLoadOp::eLoad,
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    m_FrameGraph = builder.CreateUnique(m_PipelineLibrary, m_ResourceAllocator.get());

    auto v = std::span(vertices);
    auto i = std::span(indices);

    RendererSpec rendererSpec = {
        .LogicalDevice = m_LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);

    m_Renderer->UploadWithStaging(m_FrameGraph->GetBuffer("Vertex Buffer").front(), std::as_bytes(v));
    m_Renderer->UploadWithStaging(m_FrameGraph->GetBuffer("Index Buffer").front(), std::as_bytes(i));
}

void CubeApplicationState::OnExit(ApplicationState *next)
{
    DemoApplicationState::OnExit(next);
}

void CubeApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    m_FrameGraph->ModifyImage("Depth Stencil Image").Info.setExtent(vk::Extent3D(extent, 1));
    m_FrameGraph->UpdateImage("Depth Stencil Image");

    m_FrameGraph->ModifyImage("Image").Info.setExtent(vk::Extent3D(extent, 1));
    m_FrameGraph->UpdateImage("Image");

    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Main Pass").GetScissors() = {
        vk::Rect2D(vk::Offset2D(0, 0), extent)
    };
    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Main Pass").GetViewports() = {
        vk::Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1)
    };
    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Main Pass").GetRenderArea().extent = extent;

    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Mirror Pass").GetScissors() = {
        vk::Rect2D(vk::Offset2D(0, 0), extent)
    };
    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Mirror Pass").GetViewports() = {
        vk::Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1)
    };
    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Mirror Pass").GetRenderArea().extent = extent;

    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Reflection Pass").GetScissors() = {
        vk::Rect2D(vk::Offset2D(0, 0), extent)
    };
    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Reflection Pass").GetViewports() = {
        vk::Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1)
    };
    m_FrameGraph->GetIndexedGraphicsPassDynamicConfig("Reflection Pass").GetRenderArea().extent = extent;

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();

    const float near = 0.1f;
    const float far = 1000.0f;
    const float fov = 0.80f;

    const float tanHalfFovy = std::tan(fov / 2.0f);
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

    m_Matrices.CameraProjection[0][0] = 1.0f / (aspect * tanHalfFovy);
    m_Matrices.CameraProjection[1][1] = 1.0f / (tanHalfFovy);
    m_Matrices.CameraProjection[2][2] = -(far + near) / (far - near);
    m_Matrices.CameraProjection[2][3] = -1.0f;
    m_Matrices.CameraProjection[3][2] = -(far * near) / (far - near);
    m_MirrorMatrices.CameraProjection = m_Matrices.CameraProjection;
    m_ReflectionMatrices.CameraProjection = m_Matrices.CameraProjection;
}

void CubeApplicationState::OnUpdate(float timeStep)
{
    m_UserInterface->OnUpdate(timeStep);

    m_CubeAngle += timeStep * 0.002f;
    m_Matrices.ModelTransform[0][0] = std::cos(m_CubeAngle);
    m_Matrices.ModelTransform[0][2] = std::sin(m_CubeAngle);
    m_Matrices.ModelTransform[2][0] = -std::sin(m_CubeAngle);
    m_Matrices.ModelTransform[2][2] = std::cos(m_CubeAngle);
    m_ReflectionMatrices.ModelTransform = m_MirrorMatrices.ModelTransform;
    m_ReflectionMatrices.ModelTransform[0][0] = 2.0f * std::cos(-3.14f + 0.7f + m_CubeAngle);
    m_ReflectionMatrices.ModelTransform[0][2] = std::sin(-3.14f + 0.7f + m_CubeAngle);
    m_ReflectionMatrices.ModelTransform[1][1] = 2.0f;
    m_ReflectionMatrices.ModelTransform[2][0] = -std::sin(-3.14f + 0.7f + m_CubeAngle);
    m_ReflectionMatrices.ModelTransform[2][2] = 2.0f * std::cos(-3.14f + 0.7f + m_CubeAngle);
}

void CubeApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    {
        auto m = std::span(&m_Matrices, 1);
        m_ResourceAllocator->UploadToBuffer(
            m_FrameGraph->GetCurrentBuffer("Matrix Buffer"), m.data(), m.size_bytes()
        );
    }

    {
        auto m = std::span(&m_MirrorMatrices, 1);
        m_ResourceAllocator->UploadToBuffer(
            m_FrameGraph->GetCurrentBuffer("Mirror Buffer"), m.data(), m.size_bytes()
        );
    }

    {
        auto m = std::span(&m_ReflectionMatrices, 1);
        m_ResourceAllocator->UploadToBuffer(
            m_FrameGraph->GetCurrentBuffer("Reflection Buffer"), m.data(), m.size_bytes()
        );
    }
    m_Renderer->EndFrame();
}

SwapchainUserInterface::SwapchainUserInterface(
    UserInterfaceVulkanSpec spec, SwapchainBuilder *swapchainBuilder, vk::Format format, uint32_t imageCount
)
    : DemoUserInterface(spec), m_SwapchainBuilder(swapchainBuilder),
      m_CurrentFormat(format), m_ImageCount(imageCount)
{
}

void SwapchainUserInterface::OnDefineUI(float timeStep)
{
    DemoUserInterface::OnDefineUI(timeStep);

    ImGui::ShowDemoWindow();

    ImGui::Begin("Swapchain Settings");

    auto presentModes = m_SwapchainBuilder->GetPresentModes();
    static size_t presentModeIndex =
        std::distance(presentModes.begin(), std::ranges::find(presentModes, vk::PresentModeKHR::eFifo));
    if (ImGui::BeginCombo("Present Modes", vk::to_string(presentModes[presentModeIndex]).c_str()))
    {
        for (size_t i = 0; i < presentModes.size(); i++)
        {
            if (ImGui::Selectable(vk::to_string(presentModes[i]).c_str(), presentModeIndex == i))
            {
                presentModeIndex = i;
                m_SwapchainBuilder->SetPresentMode(presentModes[i]);
                Application::GetInstance()->SetRecreateSwapchain();
            }
        }
        ImGui::EndCombo();
    }

    auto surfaceFormatString = [](vk::SurfaceFormatKHR format) {
        return std::format("{} ({})", vk::to_string(format.format), vk::to_string(format.colorSpace));
    };

    auto surfaceFormats = m_SwapchainBuilder->GetSurfaceFormats();
    static size_t surfaceFormatIndex = 0;
    if (ImGui::BeginCombo("Surface Formats", surfaceFormatString(surfaceFormats[surfaceFormatIndex]).c_str()))
    {
        for (size_t i = 0; i < surfaceFormats.size(); i++)
        {
            if (ImGui::Selectable(surfaceFormatString(surfaceFormats[i]).c_str(), surfaceFormatIndex == i))
            {
                surfaceFormatIndex = i;
                m_CurrentFormat = surfaceFormats[i].format;
                m_SwapchainBuilder->SetSurfaceFormat(surfaceFormats[i]);
                Application::GetInstance()->SetRecreateSwapchain();
            }
        }
        ImGui::EndCombo();
    }

    static int frameCount = std::clamp(
        m_ImageCount, m_SwapchainBuilder->GetMinImageCount(), m_SwapchainBuilder->GetMaxImageCount()
    );
    int max = std::min(64u, m_SwapchainBuilder->GetMaxImageCount());
    if (ImGui::SliderInt("Image Count", &frameCount, m_SwapchainBuilder->GetMinImageCount(), max))
    {
        m_ImageCount = frameCount;
        m_SwapchainBuilder->SetImageCount(frameCount);
        Application::GetInstance()->SetRecreateSwapchain();
    }

    ImGui::End();
}

vk::Format SwapchainUserInterface::GetFormat() const
{
    return m_CurrentFormat;
}

uint32_t SwapchainUserInterface::GetImageCount() const
{
    return m_ImageCount;
}

SwapchainApplicationState::SwapchainApplicationState(const ApplicationStateSpec &spec)
    : m_LogicalDevice(spec.LogicalDevice), m_MainQueue(spec.Queues.at(Application::MainQueueName)),
      m_PipelineLibrary(spec.PipelineLibrary), m_SwapchainBuilder(spec.SwapchainBuilder)
{
    DemoUserInterface::AddState("Swapchain Demo State");
}

SwapchainApplicationState::~SwapchainApplicationState() {}

void SwapchainApplicationState::OnEnter(ApplicationState * /* previous */)
{
    m_SwapchainBuilder->SetUsageFlags(
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst
    );
    m_SwapchainBuilder->SetPresentMode(vk::PresentModeKHR::eFifo);
    m_SwapchainBuilder->SetImageCount(GetImageCount(m_SwapchainBuilder, 2u));
    vk::Format format = GetSurfaceFormat(m_SwapchainBuilder);
    if (format == vk::Format::eUndefined)
    {
        auto &errors = ErrorApplicationState::GetErrors();
        errors.clear();
        errors.push_back("Could not find any suitable swapchain formats");
        ErrorApplicationState::SetErrorState("Swapchain Demo State");
        return;
    }
    m_SwapchainBuilder->SetSurfaceFormat(format);
    Application::GetInstance()->SetRecreateSwapchain();

    const auto &spec = Application::GetInstance()->GetApplicationStateSpec();
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    FrameGraphBuilder builder;

    builder.AddDeviceImage(
        "Image",
        vk::ImageCreateInfo(
            vk::ImageCreateFlagBits::eMutableFormat, vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
            vk::Extent3D(1280, 720, 1), 1, 1
        )
            .setUsage(
                vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eColorAttachment
            ),
        true, false
    );

    {
        CustomGraphicsPassSpec passSpec = {
            .OnRender = [this](vk::CommandBuffer cmd){ m_UserInterface->OnRenderVulkan(cmd); },
            .ColorAttachments = {
                {
                    .ImageResource = "Image",
                    .LoadOp = vk::AttachmentLoadOp::eClear,
                    .ClearValue = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f),
                },
            },
        };

        builder.AddCustomGraphicsPass("UI Pass", passSpec);
    }

    {
        vk::ImageSubresourceLayers layers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);

        BlitPassSpec blitSpec = {
            .SrcImageResource = "Image",
            .DstImageResource = FrameGraph::g_SwapchainImageResourceName,
            .Regions = { vk::ImageBlit2(layers, {}, layers, {}) },
        };
        builder.AddBlitPass("Blit Pass", blitSpec);
    }

    m_FrameGraph = builder.CreateUnique(m_PipelineLibrary, m_ResourceAllocator.get());

    RendererSpec rendererSpec = {
        .LogicalDevice = m_LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
        .ResourceAllocator = m_ResourceAllocator.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);

    {
        const uint32_t imageCount =
            std::clamp(2u, m_SwapchainBuilder->GetMinImageCount(), m_SwapchainBuilder->GetMaxImageCount());

        UserInterfaceVulkanSpec userInterfaceSpec = {
            .Window = spec.ApplicationWindow->GetHandle(),
            .ApiVersion = spec.ApiVersion,
            .Instance = spec.Instance,
            .PhysicalDevice = spec.PhysicalDevice,
            .LogicalDevice = spec.LogicalDevice,
            .QueueFamilyIndex = m_MainQueue.FamilyIndex,
            .Queue = m_MainQueue.Handle,
            .ImageCount = imageCount,
            .ImageFormat = vk::Format::eR8G8B8A8Unorm,
        };

        m_UserInterface = std::make_unique<SwapchainUserInterface>(
            userInterfaceSpec, spec.SwapchainBuilder, vk::Format::eR8G8B8A8Unorm, imageCount
        );
    }

    m_UserInterface->OnEnter();
}

void SwapchainApplicationState::OnExit(ApplicationState * /* next */)
{
    m_UserInterface->OnExit();

    m_Renderer.reset();
    m_FrameGraph.reset();
    m_UserInterface.reset();
    m_ResourceAllocator.reset();
}

void SwapchainApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    const vk::Format format = m_UserInterface->GetFormat();
    const vk::Format unormFormat = GetUnormFormat(format);

    {
        const auto &spec = Application::GetInstance()->GetApplicationStateSpec();
        UserInterfaceVulkanSpec userInterfaceSpec = {
            .Window = spec.ApplicationWindow->GetHandle(),
            .ApiVersion = spec.ApiVersion,
            .Instance = spec.Instance,
            .PhysicalDevice = spec.PhysicalDevice,
            .LogicalDevice = spec.LogicalDevice,
            .QueueFamilyIndex = m_MainQueue.FamilyIndex,
            .Queue = m_MainQueue.Handle,
            .ImageCount = m_UserInterface->GetImageCount(),
            .ImageFormat = unormFormat,
        };

        m_UserInterface->OnExit();
        m_UserInterface = std::make_unique<SwapchainUserInterface>(
            userInterfaceSpec, m_SwapchainBuilder, format, m_UserInterface->GetImageCount()
        );
        m_UserInterface->OnEnter();
    }

    {
        auto &image = m_FrameGraph->ModifyImage("Image");
        image.Info.setFormat(format);
        image.ViewInfo.setFormat(unormFormat);
        image.Info.setExtent(vk::Extent3D(extent, 1));
        m_FrameGraph->UpdateImage("Image");
    }

    std::array<vk::Offset3D, 2> offsets = { vk::Offset3D(), vk::Offset3D(extent.width, extent.height, 1) };

    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetSrcOffsets().front() = offsets;
    m_FrameGraph->GetBlitPassDynamicConfig("Blit Pass").GetDstOffsets().front() = offsets;

    m_FrameGraph->GetCustomGraphicsPassDynamicConfig("UI Pass").GetRenderArea().extent =
        swapchain->GetExtent();
}

void SwapchainApplicationState::OnUpdate(float timeStep)
{
    m_UserInterface->OnUpdate(timeStep);
}

void SwapchainApplicationState::OnRender()
{
    m_Renderer->BeginFrame();
    m_Renderer->EndFrame();
}

}
