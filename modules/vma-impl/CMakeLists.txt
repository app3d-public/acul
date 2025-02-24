cmake_minimum_required(VERSION 3.17)
project(vma
    VERSION 3.2.0
    LANGUAGES C
    DESCRIPTION "Vulkan Memory Allocator")

add_library(${PROJECT_NAME} SHARED vma.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/vma/include
)

if(USE_ASAN)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif(USE_ASAN)

target_compile_options(${PROJECT_NAME} PUBLIC
    -Wno-unused-variable
    -Wno-nullability-completeness
    -Wno-unused-function)

gen_manifest_lib("vma" ${vma_VERSION})