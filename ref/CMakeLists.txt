include(${PROJECT_SOURCE_DIR}/cmake/ConfigureVulkan.cmake)

set(HEADER_FILES Core/Core.h Core/Serialization.h Core/Window.h Core/UserInterface.h Vulkan/Features.h Vulkan/ApplicationBuilder.h Vulkan/Swapchain.h Vulkan/Application.h Vulkan/ApplicationState.h Vulkan/Renderer/ResourceManager.h Vulkan/Renderer/DescriptorSet.h Vulkan/Renderer/ShaderLibrary.h Vulkan/Renderer/PipelineLibrary.h Vulkan/Renderer/RenderPass.h Vulkan/Renderer/FrameGraph.h Vulkan/Renderer/FrameGraphBuilder.h Vulkan/Renderer/Renderer.h)

set(SOURCE_FILES Core/Window.cpp Core/UserInterface.cpp Vulkan/Features.cpp Vulkan/ApplicationBuilder.cpp Vulkan/Swapchain.cpp Vulkan/Application.cpp Vulkan/ApplicationState.cpp Vulkan/Renderer/ResourceManager.cpp Vulkan/Renderer/DescriptorSet.cpp Vulkan/Renderer/ShaderLibrary.cpp Vulkan/Renderer/PipelineLibrary.cpp Vulkan/Renderer/FrameGraph.cpp Vulkan/Renderer/FrameGraphBuilder.cpp Vulkan/Renderer/Renderer.cpp)

add_library(ref ${HEADER_FILES} ${SOURCE_FILES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES})

target_include_directories(ref PUBLIC ${PROJECT_SOURCE_DIR}/ref)

target_compile_definitions(ref PRIVATE
    $<$<CONFIG:Release>:REF_CONFIG_RELEASE>
    $<$<CONFIG:Profile>:REF_CONFIG_PROFILE>
    $<$<CONFIG:Debug>:REF_CONFIG_DEBUG>
    $<$<CONFIG:Trace>:REF_CONFIG_TRACE>
)

target_compile_options(ref PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /MP>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers>
  $<$<NOT:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:MSVC>>>:-Wno-nullability-extension>
)

target_link_options(ref PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/ignore:4099>)

target_link_libraries(ref PUBLIC spdlog imgui)
target_link_libraries(ref PRIVATE shaderc_combined spirv-cross-core glfw vma)
