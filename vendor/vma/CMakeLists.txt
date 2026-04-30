add_library(vma vma/include/vk_mem_alloc.h implementation.cpp)

target_include_directories(vma PUBLIC ${PROJECT_SOURCE_DIR}/vendor/vma/vma/include)
target_link_libraries(vma PUBLIC vulkan)