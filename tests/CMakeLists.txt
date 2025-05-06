cmake_minimum_required(VERSION 3.17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

set(TEST_LIBRARIES ${ACUL_LIB_NAME})
set(TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/data")
set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/tests/artifacts")
set(TEST_ENV "TEST_DATA_DIR=${TEST_DATA_DIR};TEST_OUTPUT_DIR=${TEST_OUTPUT_DIR}")
set(TEST_SOURCES "")

add_test_files(acul flags flags.cpp)
add_test_files(acul event event.cpp)
add_test_files(acul exception exception.cpp)
add_test_files(acul memory memory.cpp)
add_test_files(acul type_traits type_traits.cpp)
add_test_files(acul "string" "string.cpp")
add_test_files(acul hash hash.cpp)
add_test_files(acul file io/file.cpp)
add_test_files(acul "path" "io/path.cpp")
add_test_files(acul jatc io/jatc.cpp)
add_test_files(acul log log.cpp)
add_test_files(acul task task.cpp)
add_test_files(acul shared_mutex shared_mutex.cpp)
add_test_files(acul vector vector.cpp)
add_test_files(acul list list.cpp)
add_test_files(acul forward_list forward_list.cpp)
add_test_files(acul comparator comparator.cpp)
add_test_files(acul stream stream.cpp)
add_test_files(acul math math.cpp)
add_test_files(acul meta meta.cpp)
add_test_files(acul disposal_queue disposal_queue.cpp)
add_test_files(acul locales locales.cpp)

# GPU
list(APPEND TEST_SOURCES
    "gpu/device.cpp"
    "gpu/buffer.cpp"
    "gpu/descriptors.cpp"
    "gpu/utils.cpp"
    "gpu/pipeline.cpp"
    "gpu/vector.cpp"
)

# Prepare Shaders
function(add_shader INPUT_NAME OUTPUT_NAME)
    add_custom_command(
        OUTPUT ${OUTPUT_NAME}
        COMMAND glslc ${INPUT_NAME} -o ${OUTPUT_NAME}
        DEPENDS ${INPUT_NAME}
        COMMENT "Compiling shader ${INPUT_NAME} to SPIR-V binary ${OUTPUT_NAME}"
    )
endfunction()

add_shader(${CMAKE_CURRENT_SOURCE_DIR}/data/test.vert ${TEST_DATA_DIR}/vs_test.spv)
add_shader(${CMAKE_CURRENT_SOURCE_DIR}/data/test.frag ${TEST_DATA_DIR}/fs_test.spv)
add_custom_target(SHADERS ALL DEPENDS ${TEST_DATA_DIR}/vs_test.spv ${TEST_DATA_DIR}/fs_test.spv)
add_test_files(acul gpu gpu/gpu.cpp)

if(ENABLE_COVERAGE)
    add_test_coverage(acul)
endif()