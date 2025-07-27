cmake_minimum_required(VERSION 3.17)
project(acul
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d Core Utils Library")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/acul")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
target_compile_definitions(${PROJECT_NAME} PRIVATE APPLIB_BUILD)

if(USE_ASAN)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility-inlines-hidden)

option(ENABLE_HALF "Enable half support" ON)
option(ENABLE_LOG "Enable log support" ON)
option(ENABLE_SIMD "Enable SIMD support" ON)
option(ENABLE_GLM "Enable GLM support" ON)

# TBB
find_package(TBB REQUIRED)
set(ACUL_PUBLIC_LIBS TBB::tbb TBB::tbbmalloc)

set(ACUL_PRIVATE_LIBS zstd)

if(ENABLE_GLM)
    set(GLM_ENABLE_CXX_20 ON)
    add_compile_definitions(GLM_ENABLE_CXX_20 GLM_FORCE_INTRINSICS)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACUL_GLM_ENABLE)
    add_subdirectory("modules/3rd-party/glm")
    list(APPEND ACUL_PUBLIC_LIBS glm)
endif()

if(WIN32)
    list(APPEND ACUL_PUBLIC_LIBS intl)
endif()

if(ENABLE_HALF)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd-party/half/include)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACUL_HALF_ENABLE)
endif()

if(ENABLE_LOG)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACUL_LOG_ENABLE)
endif()

if(WIN32)
    list(APPEND ACUL_PUBLIC_LIBS Dbghelp Synchronization)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${ACUL_PUBLIC_LIBS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ACUL_PRIVATE_LIBS})


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

if(ENABLE_SIMD)
    add_subdirectory(simd)
endif()