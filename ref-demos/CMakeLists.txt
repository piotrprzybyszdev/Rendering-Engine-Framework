include(${PROJECT_SOURCE_DIR}/cmake/Utils.cmake)

set(HEADER_FILES DemoApplicationStates.h)

set(SOURCE_FILES DemoApplicationStates.cpp main.cpp)

set(SHADER_HEADER_FILES Shaders/gradient.glsl)
set(SHADER_SOURCE_FILES Shaders/gradient.comp Shaders/color.frag Shaders/pos.vert Shaders/particleDraw.comp Shaders/posColor.vert Shaders/mvp.vert)

create_directory_link(${CMAKE_CURRENT_SOURCE_DIR}/Shaders ${CMAKE_CURRENT_BINARY_DIR}/Shaders)

add_executable(ref-demos ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_HEADER_FILES} ${SHADER_SOURCE_FILES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Shaders PREFIX "Shaders/Headers" FILES ${SHADER_HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Shaders PREFIX "Shaders/Sources" FILES ${SHADER_SOURCE_FILES})

target_include_directories(ref-demos PRIVATE ${PROJECT_SOURCE_DIR}/ref-demos)

target_compile_options(ref-demos PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /MP>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers>
  $<$<CXX_COMPILER_ID:Clang>:-Wno-nullability-extension>
)

target_link_libraries(ref-demos PRIVATE ref)
