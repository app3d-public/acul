cmake_minimum_required(VERSION 3.17)

gather_sources("app3d-core" 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/std"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mem"
)

list(APPEND SOURCE_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/event.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/log.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/task.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/file.cpp"
    )

if(PCLMUL_AVAILABLE)
    if(AVX512_AVAILABLE)
        list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/hash_avx512.cpp")
    elseif(AVX2_AVAILABLE)
        list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/hash_avx2.cpp")
    elseif(AVX_AVAILABLE)
        list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/hash_avx.cpp")
    else()
        list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/hash_sse.cpp")
    endif()
else()
    list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/hash_nopcmul.cpp")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/deps/external/half/include")
add_library("app3d-core" SHARED ${SOURCE_FILES})
target_compile_definitions("app3d-core" PUBLIC -DAPPLIB_BUILD)
set_target_properties("app3d-core" PROPERTIES CXX_VISIBILITY_PRESET hidden)

find_package(TBB REQUIRED)
target_link_libraries("app3d-core" TBB::tbb TBB::tbbmalloc)

set_target_properties("app3d-core"
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)