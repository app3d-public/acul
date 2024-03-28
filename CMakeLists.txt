cmake_minimum_required(VERSION 3.17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
add_definitions(-D_LIBCPP_NO_VCRUNTIME)

gather_sources("app3d-core" RECURSE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/"
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/deps/external/half/include")
add_library("app3d-core" SHARED ${SOURCE_FILES})

find_package(TBB REQUIRED)
target_link_libraries("app3d-core" TBB::tbb)

set_target_properties("app3d-core"
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)