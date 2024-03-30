cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -msse4.2 -mpclmul")
add_definitions(-D_LIBCPP_NO_VCRUNTIME)

add_library("app3d-simd-sse42-pclmulqdq" SHARED "${CMAKE_CURRENT_SOURCE_DIR}/crc32@impl.cpp")

set_target_properties("app3d-simd-sse42-pclmulqdq"
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    ARCHIVE_OUTPUT_DIRECTORY ""
)