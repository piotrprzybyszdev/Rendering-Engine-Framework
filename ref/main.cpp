#include "Core/Core.h"

#include "Vulkan/Application.h"
#include "Vulkan/ApplicationBuilder.h"

using namespace ref;

int main()
{
    logger::set_level(logger::level::debug);

    vulkan::ApplicationBuilder::InitSystems();

    vulkan::ApplicationBuilder builder;
    builder.EnableBase();

    {
        vulkan::Application application = builder.CreateApplication("REF");
        application.Run();
    }

    vulkan::ApplicationBuilder::ShutdownSystems();

    return 0;
}
