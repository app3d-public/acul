cmake_minimum_required(VERSION 3.17)
project(app3d-core
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d Core Library")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/core")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/deps/external/half/include")

add_subdirectory("src/vma")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DAPPLIB_BUILD)

if(USE_ASAN)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
find_package(TBB REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC TBB::tbb TBB::tbbmalloc vma intl)
target_include_directories(${PROJECT_NAME} PUBLIC $<TARGET_PROPERTY:vma,INCLUDE_DIRECTORIES>)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)