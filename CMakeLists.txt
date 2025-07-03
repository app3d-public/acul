cmake_minimum_required(VERSION 3.17)
project(acul
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d Core Utils Library")

add_files(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/acul")
add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/io")
add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/string")
add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/hash")
add_files(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/exception")

if(NOT ACUL_BUILD_MIN)
    add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/gpu")
    add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/simd_fallback")
    add_subdirectory("modules/vma-impl")

    # GLM
    set(GLM_ENABLE_CXX_20 ON)
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory("modules/3rd-party/glm")
    set(BUILD_SHARED_LIBS OFF)

    if(USE_ASAN)
        target_link_options(glm PUBLIC -fsanitize=address -fno-omit-frame-pointer)
    endif(USE_ASAN)

    target_compile_definitions(glm PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
        GLM_FORCE_INTRINSICS
        GLM_FORCE_ALIGNED_GENTYPES
    )

    add_subdirectory(simd)
else()
    list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/math.cpp")
    list(REMOVE_ITEM SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/acul/io/jatc.cpp")
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_compile_definitions(${PROJECT_NAME} PRIVATE APPLIB_BUILD)

if(USE_ASAN)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility-inlines-hidden)

# TBB
find_package(TBB REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC TBB::tbb TBB::tbbmalloc zstd)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC intl)
endif()

if(NOT ACUL_BUILD_MIN)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/half/include
        ${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/glm
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACUL_HALF_ENABLE ACUL_LOG_ENABLE ACUL_GLM_ENABLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC glm vma)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Dbghelp Synchronization)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    # Manifest
    if(NOT DEFINED BUILD_EXTERNAL_MANIFEST)
        set(BUILD_EXTERNAL_MANIFEST OFF)
    endif()

    if(BUILD_EXTERNAL_MANIFEST)
        gen_manifest_lib("tbb12" ${TBB_VERSION})
        gen_manifest_lib(tbbmalloc ${TBB_VERSION})
        set(TBB_MANIFEST_DEPS tbb12:${TBB_VERSION} tbbmalloc:${TBB_VERSION} PARENT_SCOPE)
    endif()

    gen_manifest_lib(${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()