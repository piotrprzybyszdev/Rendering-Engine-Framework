include(${CMAKE_SOURCE_DIR}/cmake/ConfigureVulkan.cmake)

set(HEADER_FILES Core/Core.h Core/Window.h Vulkan/Features.h Vulkan/ApplicationBuilder.h Vulkan/Swapchain.h Vulkan/Application.h)

set(SOURCE_FILES main.cpp Core/Window.cpp Vulkan/Features.cpp Vulkan/ApplicationBuilder.cpp Vulkan/Swapchain.cpp Vulkan/Application.cpp)

add_executable(ref ${HEADER_FILES} ${SOURCE_FILES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES})

target_include_directories(ref PRIVATE ${CMAKE_SOURCE_DIR}/ref)

target_compile_definitions(ref PRIVATE
    $<$<CONFIG:Release>:REF_CONFIG_RELEASE>
    $<$<CONFIG:Profile>:REF_CONFIG_PROFILE>
    $<$<CONFIG:Debug>:REF_CONFIG_DEBUG>
    $<$<CONFIG:Trace>:REF_CONFIG_TRACE>
)

target_compile_options(ref PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /MP>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
)

target_link_options(ref PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/ignore:4099>
)

target_link_libraries(ref PRIVATE vulkan spdlog glfw)
