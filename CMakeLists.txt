cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CONFIGURATION_TYPES Release Profile Debug Trace)

project(ref LANGUAGES CXX C)

include(${CMAKE_SOURCE_DIR}/cmake/Utils.cmake)

add_configuration(TRACE DEBUG TRUE)
add_configuration(PROFILE RELEASE FALSE)

add_subdirectory(ref)

add_subdirectory(vendor/spdlog)

set_target_properties(spdlog PROPERTIES FOLDER vendor)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ref")
endif()
