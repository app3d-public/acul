cmake_minimum_required(VERSION 3.17)
project(acul-simd
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "ACUL Simd Module")

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(${PROJECT_NAME} SHARED ${ACUL_SIMD_SRC})
target_compile_definitions(${PROJECT_NAME} PRIVATE APPLIB_BUILD)
target_link_libraries(${PROJECT_NAME} PUBLIC acul)

if(USE_ASAN)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fno-omit-frame-pointer)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility-inlines-hidden)
target_compile_options(${PROJECT_NAME} PRIVATE -march=native)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)

if (WIN32)
    # Manifest
    gen_manifest_lib(lib${PROJECT_NAME} ${PROJECT_VERSION})
endif()