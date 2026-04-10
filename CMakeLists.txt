cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CONFIGURATION_TYPES Release Profile Debug Trace)

project(ref LANGUAGES CXX C)

option(REF_BUILD_DEMOS "Build REF demo programs" ON)

include(${CMAKE_SOURCE_DIR}/cmake/Utils.cmake)

add_configuration(TRACE DEBUG TRUE)
add_configuration(PROFILE RELEASE FALSE)

add_subdirectory(ref)

add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/glfw)
add_subdirectory(vendor/imgui)
add_subdirectory(vendor/vma)

set_target_properties(spdlog PROPERTIES FOLDER vendor)
set_target_properties(glfw PROPERTIES FOLDER vendor/GLFW3)
set_target_properties(uninstall PROPERTIES FOLDER vendor/GLFW3)
set_target_properties(update_mappings PROPERTIES FOLDER vendor/GLFW3)
set_target_properties(imgui PROPERTIES FOLDER vendor)
set_target_properties(vma PROPERTIES FOLDER vendor)

if (REF_BUILD_DEMOS)
    add_subdirectory(ref-demos)
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ref-demos")
    endif()
endif()