cmake_minimum_required(VERSION 3.17)
project(vma
    VERSION 3.1.0
    LANGUAGES C
    DESCRIPTION "Vulkan Memory Allocator")

include_directories("../deps/external/vma/include")

add_library(${PROJECT_NAME} SHARED vma.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wno-unused-variable
    -Wno-nullability-completeness
    -Wno-unused-function)
