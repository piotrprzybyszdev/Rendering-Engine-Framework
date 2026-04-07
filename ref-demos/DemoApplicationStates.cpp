#include <imgui.h>

#include "Vulkan/Application.h"
#include "Vulkan/Renderer/FrameGraphBuilder.h"

#include "DemoApplicationStates.h"

namespace ref::vulkan
{

DemoUserInterfaceState::DemoUserInterfaceState(const std::string &state)
{
    s_DemoStates.insert(state);
}

void DemoUserInterfaceState::OnInit()
{
    ImGui::StyleColorsDark();
}

void DemoUserInterfaceState::OnShutdown()
{
}

void DemoUserInterfaceState::OnUpdate(float /* timeStep */)
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

void DemoUserInterfaceState::OnKeyRelease(Key /* key */)
{
}

ComputeApplicationState::ComputeApplicationState(const ApplicationStateSpec &spec)
    : m_MainQueue(spec.Queues.at(Application::MainQueueName)), m_ShaderLibrary(spec.ShaderLibrary),
      m_PipelineLibrary(spec.PipelineLibrary), m_SwapchainBuilder(spec.SwapchainBuilder)
{
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = 2,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<UserInterface>(
        userInterfaceSpec, std::make_unique<DemoUserInterfaceState>("Compute Demo State")
    );

    m_ShaderId = m_ShaderLibrary->AddShader(
        ShaderInfo("Shaders/gradient.comp", "main", vk::ShaderStageFlagBits::eCompute)
    );

    m_ShaderLibrary->LoadShader(m_ShaderId);
    m_PipelineId = m_PipelineLibrary->AddPipeline(ComputePipelineInfo("Shader Toy Pipeline", m_ShaderId));
    m_PipelineLibrary->CompilePipeline(m_PipelineId);

    FrameGraphBuilder builder;

    {
        ComputePassSpec passSpec = {
            .Pipeline = m_PipelineId,
            .PushConstantData = std::as_bytes(std::span(&m_Time, 1)),
            .ImageBindings = {
                { FrameGraph::g_SwapchainImageResourceName, 0, nullptr, false, true },
            },
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
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
}

ComputeApplicationState::~ComputeApplicationState()
{
}

void ComputeApplicationState::OnEnter(ApplicationState * /* previous */)
{
    m_SwapchainBuilder->SetUsageFlags(
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment |
        vk::ImageUsageFlagBits::eTransferDst
    );
    Application::GetInstance()->SetRecreateSwapchain();

    m_Time = 0.0f;
    m_ShaderLibrary->LoadShader(m_ShaderId);
    bool success = m_PipelineLibrary->CompilePipeline(m_PipelineId);
    if (!success)
    {
        auto &errors = ErrorApplicationState::GetErrors();
        errors.clear();
        for (auto &failure : m_ShaderLibrary->GetCompilationFailedShaders())
            errors.push_back(failure.Error);
        ErrorApplicationState::SetErrorState("Compute Demo State");
        return;
    }
}

void ComputeApplicationState::OnExit(ApplicationState * /* next */)
{
}

void ComputeApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    m_FrameGraph->GetComputePassDynamicConfig("Compute Pass").GetDispatchCommand() = {
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
    m_Renderer->OnUpdate(timeStep);
    m_Time += timeStep / 1000.0f;

    if (m_Time > 5.0f)
    {
        m_Time -= 5.0f;
        m_ShaderLibrary->LoadShader(m_ShaderId);
        bool success = m_PipelineLibrary->CompilePipeline(m_PipelineId);
        if (!success)
        {
            auto &errors = ErrorApplicationState::GetErrors();
            errors.clear();
            for (auto &failure : m_ShaderLibrary->GetCompilationFailedShaders())
                errors.push_back(failure.Error);
            ErrorApplicationState::SetErrorState("Compute Demo State");
            return;
        }
    }
}

void ComputeApplicationState::OnRender()
{
    m_Renderer->OnRender();
}

TriangleApplicationState::TriangleApplicationState(const ApplicationStateSpec &spec)
    : m_MainQueue(spec.Queues.at(Application::MainQueueName))
{
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = 2,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<UserInterface>(
        userInterfaceSpec, std::make_unique<DemoUserInterfaceState>("Triangle Demo State")
    );

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

    builder.AddHostBuffer(
        "Vertex Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(vertices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eVertexBuffer),
        false
    );

    builder.AddHostBuffer(
        "Index Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(indices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eIndexBuffer),
        false
    );

    ShaderId vertexShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/posColor.vert", "main", vk::ShaderStageFlagBits::eVertex)
    );
    ShaderId fragmentShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/color.frag", "main", vk::ShaderStageFlagBits::eFragment)
    );

    GraphicsPipelineId pipelineId;
    {
        GraphicsPipelineInfo pipelineInfo = {
            .Name = "Triangle Pipeline",
            .VertexShaderId = vertexShader,
            .FragmentShaderId = fragmentShader,
            .BindingDescriptions = { vk::VertexInputBindingDescription(0, sizeof(Vertex)) },
            .VertexInputs = { { 0, offsetof(Vertex, Position) }, { 0, offsetof(Vertex, Color) } },
            .ColorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
        };
        pipelineInfo.InputAssemblyState.setTopology(vk::PrimitiveTopology::eTriangleList);
        pipelineInfo.RasterizationState.setLineWidth(1.0f);
        pipelineInfo.AttachmentBlendStates.emplace_back().setColorWriteMask(
            vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
        );

        pipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);
    }

    {
        GraphicsPassSpec passSpec = {
            .Pipeline = pipelineId,
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
            .Command = { 
                .VertexCount = 3,
                .InstanceCount = 1,
                .FirstVertex = 0,
                .FirstInstance = 0,
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

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    spec.ShaderLibrary->LoadShader(vertexShader);
    spec.ShaderLibrary->LoadShader(fragmentShader);
    [[maybe_unused]] bool success = spec.PipelineLibrary->CompilePipeline(pipelineId);
    assert(success == true);

    auto v = std::span(vertices);
    auto i = std::span(indices);

    m_ResourceAllocator->UploadToBuffer(
        m_FrameGraph->GetBuffer("Vertex Buffer").front(), v.data(), v.size_bytes()
    );
    m_ResourceAllocator->UploadToBuffer(
        m_FrameGraph->GetBuffer("Index Buffer").front(), i.data(), i.size_bytes()
    );

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
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
    m_Renderer->OnUpdate(timeStep);
}

void TriangleApplicationState::OnRender()
{
    m_Renderer->OnRender();
}

ParticleApplicationState::ParticleApplicationState(const ApplicationStateSpec &spec)
    : m_MainQueue(spec.Queues.at(Application::MainQueueName))
{
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);
    
    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = 2,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<UserInterface>(
        userInterfaceSpec, std::make_unique<DemoUserInterfaceState>("Particle Demo State")
    );

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
    builder.AddHostBuffer(
        "Vertex Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(vertices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eVertexBuffer),
        false
    );
    builder.AddHostBuffer(
        "Index Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(indices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eIndexBuffer),
        false
    );
    builder.AddDeviceBuffer(
        "Indirect Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(vk::DrawIndexedIndirectCommand))
            .setUsage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer),
        true
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
        true
    );

    ShaderId computeShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/particleDraw.comp", "main", vk::ShaderStageFlagBits::eCompute)
    );
    ShaderId vertexShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/pos.vert", "main", vk::ShaderStageFlagBits::eVertex)
    );
    ShaderId fragmentShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/color.frag", "main", vk::ShaderStageFlagBits::eFragment)
    );

    GraphicsPipelineId particlePipelineId;
    ComputePipelineId computePipelineId;
    {
        GraphicsPipelineInfo pipelineInfo = {
            .Name = "Particle Graphics Pipeline",
            .VertexShaderId = vertexShader,
            .FragmentShaderId = fragmentShader,
            .BindingDescriptions = { vk::VertexInputBindingDescription(0, sizeof(Vertex)) },
            .VertexInputs = { { 0, offsetof(Vertex, Position) } },
            .ColorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
        };
        pipelineInfo.InputAssemblyState.setTopology(vk::PrimitiveTopology::eTriangleList);
        pipelineInfo.RasterizationState.setLineWidth(1.0f);
        pipelineInfo.AttachmentBlendStates.emplace_back().setColorWriteMask(
            vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
        );

        particlePipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);
    }

    {
        ComputePipelineInfo pipelineInfo = { .Name = "Particle Compute Pipeline",
                                             .ComputeShader = computeShader };
        computePipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);
    }

    {
        ClearPassSpec clearSpec = {
            .ImageResource = "Image",
            .Ranges = { vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1) },
        };
        builder.AddClearPass("Clear Pass", std::move(clearSpec));
    }

    {
        ComputePassSpec computeSpec = {
            .Pipeline = computePipelineId,
            .PushConstantData = std::as_bytes(std::span(&m_ParticleIdx, 1)),
            .BufferBindings = { BufferBinding("Indirect Buffer", 0, false, true) },
            .Command = { 1, 1, 1 },
        };
        builder.AddComputePass("Compute Pass", std::move(computeSpec));
    }

    {
        IndexedIndirectGraphicsPassSpec graphicsSpec = {
            .Pipeline = particlePipelineId,
            .VertexBuffers = {
                .VertexBuffers = { { "Vertex Buffer" } },
            },            
            .IndexBuffer = { "Index Buffer", 0, vk::IndexType::eUint32 },
            .ColorAttachments = { { "Image" } },
            .IndirectBufferResource = "Indirect Buffer",
            .Command = {
                .DrawCount = 1,
            },
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

    spec.ShaderLibrary->LoadShader(computeShader);
    spec.ShaderLibrary->LoadShader(vertexShader);
    spec.ShaderLibrary->LoadShader(fragmentShader);

    [[maybe_unused]] bool success = spec.PipelineLibrary->CompilePipeline(particlePipelineId);
    assert(success == true);
    success = spec.PipelineLibrary->CompilePipeline(computePipelineId);
    assert(success == true);

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    m_FrameGraph->GetClearPassDynamicConfig("Clear Pass").GetClearValue() =
        vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f);

    auto v = std::span(vertices);
    auto i = std::span(indices);

    m_ResourceAllocator->UploadToBuffer(
        m_FrameGraph->GetBuffer("Vertex Buffer").front(), v.data(), v.size_bytes()
    );
    m_ResourceAllocator->UploadToBuffer(
        m_FrameGraph->GetBuffer("Index Buffer").front(), i.data(), i.size_bytes()
    );

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
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
    m_Renderer->OnUpdate(timeStep);
}

void ParticleApplicationState::OnRender()
{
    m_Renderer->OnRender();
}

CubeApplicationState::CubeApplicationState(const ApplicationStateSpec &spec)
    : m_MainQueue(spec.Queues.at(Application::MainQueueName))
{
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = 2,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    m_UserInterface = std::make_unique<UserInterface>(
        userInterfaceSpec, std::make_unique<DemoUserInterfaceState>("Cube Demo State")
    );

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
        true
    );

    builder.AddHostBuffer(
        "Vertex Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(vertices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eVertexBuffer),
        false
    );
    builder.AddHostBuffer(
        "Index Buffer",
        vk::BufferCreateInfo()
            .setSize(std::span(indices).size_bytes())
            .setUsage(vk::BufferUsageFlagBits::eIndexBuffer),
        false
    );
    builder.AddHostBuffer(
        "Matrix Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(MatrixBuffer))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer),
        true
    );
    builder.AddHostBuffer(
        "Mirror Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(MatrixBuffer))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer),
        true
    );
    builder.AddHostBuffer(
        "Reflection Buffer",
        vk::BufferCreateInfo()
            .setSize(sizeof(MatrixBuffer))
            .setUsage(vk::BufferUsageFlagBits::eUniformBuffer),
        true
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
        true
    );

    ShaderId vertexShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/mvp.vert", "main", vk::ShaderStageFlagBits::eVertex)
    );
    ShaderId fragmentShader = spec.ShaderLibrary->AddShader(
        ShaderInfo("Shaders/color.frag", "main", vk::ShaderStageFlagBits::eFragment)
    );

    spec.ShaderLibrary->LoadShader(vertexShader);
    spec.ShaderLibrary->LoadShader(fragmentShader);

    GraphicsPipelineId cubePipelineId, mirrorPipelineId, reflectionPipelineId;
    {
        GraphicsPipelineInfo pipelineInfo = {
            .Name = "Cube Pipeline",
            .VertexShaderId = vertexShader,
            .FragmentShaderId = fragmentShader,
            .BindingDescriptions = { vk::VertexInputBindingDescription(0, sizeof(Vertex)) },
            .VertexInputs = { { 0, offsetof(Vertex, Position) }, { 0, offsetof(Vertex, Color) } },
            .ColorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
            .DepthAttachmentFormat = vk::Format::eD24UnormS8Uint,
            .StencilAttachmentFormat = vk::Format::eD24UnormS8Uint,
        };

        pipelineInfo.InputAssemblyState.setTopology(vk::PrimitiveTopology::eTriangleList);
        pipelineInfo.MultisampleState.setRasterizationSamples(vk::SampleCountFlagBits::e4);
        pipelineInfo.RasterizationState.setLineWidth(1.0f);
        pipelineInfo.AttachmentBlendStates.emplace_back().setColorWriteMask(
            vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags
        );

        pipelineInfo.DepthStencilState.setDepthTestEnable(vk::True);
        pipelineInfo.DepthStencilState.setDepthWriteEnable(vk::True);
        pipelineInfo.DepthStencilState.setDepthCompareOp(vk::CompareOp::eLess);

        cubePipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);

        pipelineInfo.Name = "Mirror Pipeline";
        pipelineInfo.DepthStencilState.setStencilTestEnable(vk::True);
        pipelineInfo.DepthStencilState.setBack(
            vk::StencilOpState()
                .setCompareOp(vk::CompareOp::eAlways)
                .setPassOp(vk::StencilOp::eReplace)
                .setWriteMask(0xff)
                .setReference(1)
        );

        mirrorPipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);

        pipelineInfo.DepthStencilState.setBack(
            vk::StencilOpState().setCompareOp(vk::CompareOp::eEqual).setCompareMask(0xff).setReference(1)
        );
        pipelineInfo.AttachmentBlendStates.front()
            .setBlendEnable(vk::True)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd);

        reflectionPipelineId = spec.PipelineLibrary->AddPipeline(pipelineInfo);
    }

    [[maybe_unused]] bool success = spec.PipelineLibrary->CompilePipeline(cubePipelineId);
    assert(success == true);
    success = spec.PipelineLibrary->CompilePipeline(mirrorPipelineId);
    assert(success == true);
    success = spec.PipelineLibrary->CompilePipeline(reflectionPipelineId);
    assert(success == true);

    {
        IndexedGraphicsPassSpec passSpec = {
            .Pipeline = cubePipelineId,
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
            .Command = {
                .IndexCount = static_cast<uint32_t>(indices.size()),
                .InstanceCount = 1,
                .FirstIndex = 0,
                .VertexOffset = 0,
                .FirstInstance = 0,
            },
        };
        builder.AddIndexedGraphicsPass("Main Pass", passSpec);
    }

    {
        IndexedGraphicsPassSpec passSpec = {
            .Pipeline = mirrorPipelineId,
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
            .Command = {
                .IndexCount = 6,
                .InstanceCount = 1,
                .FirstIndex = 0,
                .VertexOffset = 0,
                .FirstInstance = 0,
            },
        };
        builder.AddIndexedGraphicsPass("Mirror Pass", passSpec);
    }

    {
        IndexedGraphicsPassSpec passSpec = {
            .Pipeline = reflectionPipelineId,
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
            .Command = {
                .IndexCount = static_cast<uint32_t>(indices.size()),
                .InstanceCount = 1,
                .FirstIndex = 0,
                .VertexOffset = 0,
                .FirstInstance = 0,
            },
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

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    auto v = std::span(vertices);
    auto i = std::span(indices);

    m_ResourceAllocator->UploadToBuffer(
        m_FrameGraph->GetBuffer("Vertex Buffer").front(), v.data(), v.size_bytes()
    );
    m_ResourceAllocator->UploadToBuffer(
        m_FrameGraph->GetBuffer("Index Buffer").front(), i.data(), i.size_bytes()
    );

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
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
    m_Renderer->OnUpdate(timeStep);

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
}

void CubeApplicationState::OnRender()
{
    m_Renderer->OnRender();
}

SwapchainUserInterfaceState::SwapchainUserInterfaceState(
    SwapchainBuilder *swapchainBuilder, vk::Format format, uint32_t imageCount
)
    : DemoUserInterfaceState("Swapchain Demo State"), m_SwapchainBuilder(swapchainBuilder), m_CurrentFormat(format),
      m_ImageCount(imageCount)
{
}

void SwapchainUserInterfaceState::OnInit()
{
    DemoUserInterfaceState::OnInit();
}

void SwapchainUserInterfaceState::OnShutdown()
{
    DemoUserInterfaceState::OnShutdown();
}

void SwapchainUserInterfaceState::OnUpdate(float timeStep)
{
    DemoUserInterfaceState::OnUpdate(timeStep);

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

    static int frameCount = std::clamp(m_ImageCount, m_SwapchainBuilder->GetMinImageCount(), m_SwapchainBuilder->GetMaxImageCount());
    int max = std::min(64u, m_SwapchainBuilder->GetMaxImageCount());
    if (ImGui::SliderInt("Image Count", &frameCount, m_SwapchainBuilder->GetMinImageCount(), max))
    {
        m_ImageCount = frameCount;
        m_SwapchainBuilder->SetImageCount(frameCount);
        Application::GetInstance()->SetRecreateSwapchain();
    }

    ImGui::End();
}

void SwapchainUserInterfaceState::OnKeyRelease(Key key)
{
    DemoUserInterfaceState::OnKeyRelease(key);
}

vk::Format SwapchainUserInterfaceState::GetFormat() const
{
    return m_CurrentFormat;
}

uint32_t SwapchainUserInterfaceState::GetImageCount() const
{
    return m_ImageCount;
}

SwapchainApplicationState::SwapchainApplicationState(const ApplicationStateSpec &spec)
    : m_MainQueue(spec.Queues.at(Application::MainQueueName)), m_SwapchainBuilder(spec.SwapchainBuilder)
{
    ResourceManagerSpec resourceManagerSpec = {
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
    };

    m_ResourceAllocator = std::make_unique<ResourceAllocator>(resourceManagerSpec);

    UserInterfaceVulkanSpec userInterfaceSpec = {
        .Window = spec.ApplicationWindow->GetHandle(),
        .ApiVersion = spec.ApiVersion,
        .Instance = spec.Instance,
        .PhysicalDevice = spec.PhysicalDevice,
        .LogicalDevice = spec.LogicalDevice,
        .QueueFamilyIndex = m_MainQueue.FamilyIndex,
        .Queue = m_MainQueue.Handle,
        .ImageCount = 2,
        .ImageFormat = vk::Format::eR8G8B8A8Unorm,
    };

    {
        uint32_t imageCount = std::clamp(2u, m_SwapchainBuilder->GetMinImageCount(), m_SwapchainBuilder->GetMaxImageCount());
        auto ptr = std::make_unique<SwapchainUserInterfaceState>(
            spec.SwapchainBuilder, vk::Format::eR8G8B8A8Unorm, imageCount
        );
        m_UserInterfaceState = ptr.get();
        m_UserInterface = std::make_unique<UserInterface>(userInterfaceSpec, std::move(ptr));
    }

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
        true
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

    m_FrameGraph = builder.CreateUnique(spec.PipelineLibrary, m_ResourceAllocator.get());

    RendererSpec rendererSpec = {
        .LogicalDevice = spec.LogicalDevice,
        .MainQueue = m_MainQueue,
        .FrameGraph = m_FrameGraph.get(),
    };

    m_Renderer = std::make_unique<Renderer>(rendererSpec);
}

SwapchainApplicationState::~SwapchainApplicationState()
{
}

void SwapchainApplicationState::OnEnter(ApplicationState * /* previous */)
{
    m_SwapchainBuilder->SetUsageFlags(
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst
    );
    Application::GetInstance()->SetRecreateSwapchain();
}

void SwapchainApplicationState::OnExit(ApplicationState * /* next */)
{
    m_SwapchainBuilder->SetPresentMode(vk::PresentModeKHR::eFifo);
    m_SwapchainBuilder->SetImageCount(std::clamp(2u, m_SwapchainBuilder->GetMinImageCount(), m_SwapchainBuilder->GetMaxImageCount()));
    Application::GetInstance()->SetRecreateSwapchain();
}

void SwapchainApplicationState::OnResize(const Swapchain *swapchain)
{
    m_Renderer->OnResize(swapchain);

    const vk::Extent2D extent = swapchain->GetExtent();

    vk::Format format = m_UserInterfaceState->GetFormat();
    auto getUnormFormat = [](vk::Format format) {
        switch (format)
        {
        case vk::Format::eR8G8B8A8Srgb:
            return vk::Format::eR8G8B8A8Unorm;
        case vk::Format::eB8G8R8A8Srgb:
            return vk::Format::eB8G8R8A8Unorm;
        default:
            return format;
        }
    };

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
            .ImageCount = m_UserInterfaceState->GetImageCount(),
            .ImageFormat = getUnormFormat(format),
        };

        auto ptr = std::make_unique<SwapchainUserInterfaceState>(
            m_SwapchainBuilder, format, m_UserInterfaceState->GetImageCount()
        );
        m_UserInterfaceState = ptr.get();
        m_UserInterface = std::make_unique<UserInterface>(userInterfaceSpec, std::move(ptr));
    }

    {
        auto &image = m_FrameGraph->ModifyImage("Image");
        image.Info.setFormat(format);
        image.ViewInfo.setFormat(getUnormFormat(format));
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
    m_Renderer->OnUpdate(timeStep);
}

void SwapchainApplicationState::OnRender()
{
    m_Renderer->OnRender();
}

}
