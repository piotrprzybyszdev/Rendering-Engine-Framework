#include <Core/Core.h>

#include <Vulkan/Application.h>
#include <Vulkan/ApplicationBuilder.h>
#include <Vulkan/ApplicationState.h>

#include "DemoApplicationStates.h"

using namespace ref;

void ConfigureShaders(ref::vulkan::Application& application)
{
    std::filesystem::path cache = "RefCache";
    auto &options = application.GetShaderLibrary().ModifyCompilationOptions();

#ifdef NDEBUG
    cache /= "Release";
    options.Optimization = ref::vulkan::OptimizationMode::Performance;
    options.GenerateDebugInfo = false;
    options.MacroDefinitions.push_back("REF_SHADER_RELEASE");
#else
    cache /= "Debug";
    options.Optimization = ref::vulkan::OptimizationMode::None;
    options.GenerateDebugInfo = true;
    options.MacroDefinitions.push_back("REF_SHADER_DEBUG");
#endif

    application.GetShaderLibrary().SetShaderCachePath(cache / "Shaders");
    application.GetPipelineLibrary().SetPipelineCachePath(cache);
    application.GetShaderLibrary().AddShadersFromDirectory("Shaders");
}

int main()
{
    logger::set_level(logger::level::debug);

    vulkan::ApplicationBuilder::InitSystems();

    vulkan::ApplicationBuilder builder;
    builder.EnableBase();

    {
        vulkan::Application application = builder.CreateApplication("REF");
        ConfigureShaders(application);

        vulkan::ErrorApplicationState::AddToApplication(application);
        vulkan::CompilingShadersApplicationState::AddToApplication(application, "Swapchain Demo State");

        application.AddAndCreateState<vulkan::SwapchainApplicationState>("Swapchain Demo State");
        application.AddAndCreateState<vulkan::CubeApplicationState>("Cube Demo State");
        application.AddAndCreateState<vulkan::ParticleApplicationState>("Particle Demo State");
        application.AddAndCreateState<vulkan::TriangleApplicationState>("Triangle Demo State");
        application.AddAndCreateState<vulkan::ComputeApplicationState>("Compute Demo State");

        application.Run(vulkan::CompilingShadersApplicationState::g_StateName);

        application.GetShaderLibrary().PruneShaderCache();
    }

    vulkan::ApplicationBuilder::ShutdownSystems();

    return 0;
}
