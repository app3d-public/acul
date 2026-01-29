cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(cmake/project.cmake)
endif()

project(acul
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "App3d Core Utils Library")

if(NOT DEFINED ACBT_LOADED)
    include(cmake/utils.cmake)
endif()

option(ACUL_INTL_ENABLE "Enable intl support" ON)
option(ACUL_ZSTD_ENABLE "Enable zstd support" ON)

# Dependencies check


set(ACUL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_files(${PROJECT_NAME} ${ACUL_SRC_DIR})
add_files(${PROJECT_NAME} RECURSE "${ACUL_SRC_DIR}/hash")
add_files(${PROJECT_NAME} RECURSE "${ACUL_SRC_DIR}/exception")
add_files(${PROJECT_NAME} RECURSE "${ACUL_SRC_DIR}/string")
add_files(${PROJECT_NAME} "${ACUL_SRC_DIR}/io")

if(ACUL_INTL_ENABLE)
    add_files(${PROJECT_NAME} "${ACUL_SRC_DIR}/locales")
endif()

if(ACUL_ZSTD_ENABLE)
    add_files(${PROJECT_NAME} "${ACUL_SRC_DIR}/io/fs")
endif()

include("${ACUL_SRC_DIR}/isa.cmake")

add_library(${PROJECT_NAME} SHARED ${ACUL_SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE APPLIB_BUILD)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility-inlines-hidden)

# TBB
find_package(TBB REQUIRED)
set(ACUL_PUBLIC_LIBS TBB::tbb TBB::tbbmalloc)

if(ACUL_ZSTD_ENABLE)
    set(ACUL_PRIVATE_LIBS zstd)
endif()

if(WIN32 AND ACUL_INTL_ENABLE)
    list(APPEND ACUL_PUBLIC_LIBS intl)
endif()

if(ACUL_INTL_ENABLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACUL_INTL_ENABLE)
endif()

if(ACUL_ZSTD_ENABLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ACUL_ZSTD_ENABLE)
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
        gen_manifest_lib(libtbb12 ${TBB_VERSION})
        gen_manifest_lib(libtbbmalloc ${TBB_VERSION})
        set(TBB_MANIFEST_DEPS libtbb12:${TBB_VERSION} libtbbmalloc:${TBB_VERSION} PARENT_SCOPE)
    endif()

    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_EXTENSIONS YES
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()