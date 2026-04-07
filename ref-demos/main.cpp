#include "Core/Core.h"

#include "Vulkan/Application.h"
#include "Vulkan/ApplicationBuilder.h"
#include "Vulkan/ApplicationState.h"

#include "DemoApplicationStates.h"

using namespace ref;

int main()
{
    logger::set_level(logger::level::debug);

    vulkan::ApplicationBuilder::InitSystems();

    vulkan::ApplicationBuilder builder;
    builder.EnableBase();

    {
        vulkan::Application application = builder.CreateApplication("REF");
        vulkan::ErrorApplicationState::AddToApplication(application);
        application.AddAndCreateState<vulkan::SwapchainApplicationState>("Swapchain Demo State");
        application.AddAndCreateState<vulkan::CubeApplicationState>("Cube Demo State");
        application.AddAndCreateState<vulkan::ParticleApplicationState>("Particle Demo State");
        application.AddAndCreateState<vulkan::TriangleApplicationState>("Triangle Demo State");
        application.AddAndCreateState<vulkan::ComputeApplicationState>("Compute Demo State");

        application.Run("Swapchain Demo State");
    }

    vulkan::ApplicationBuilder::ShutdownSystems();

    return 0;
}
