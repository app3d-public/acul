cmake_minimum_required(VERSION 3.17)
project(app3d-core
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d Core Library")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/glm")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/half/include")

# GLM
set(GLM_ENABLE_CXX_20 ON)
set(BUILD_SHARED_LIBS ON)
add_subdirectory("modules/3rd-party/glm")
set(BUILD_SHARED_LIBS OFF)

if(USE_ASAN)
    target_link_options(glm PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif(USE_ASAN)

target_compile_definitions(glm PUBLIC GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_ENABLE_EXPERIMENTAL)

add_subdirectory("modules/vma-impl")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_compile_definitions(${PROJECT_NAME} PRIVATE -DAPPLIB_BUILD)

if(USE_ASAN)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
find_package(TBB REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC
    TBB::tbb TBB::tbbmalloc
    vma
    intl
    zstd
    glm)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        Synchronization
        Dbghelp)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC $<TARGET_PROPERTY:vma,INCLUDE_DIRECTORIES>)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)