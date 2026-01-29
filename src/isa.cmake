# crc32
set_source_files_properties(
    "${ACUL_SRC_DIR}/hash/crc32_sse42.cpp"
    PROPERTIES COMPILE_OPTIONS "-msse4.2;-mcrc32;-mpclmul"
)
set_source_files_properties(
    "${ACUL_SRC_DIR}/hash/crc32_avx.cpp"
    PROPERTIES COMPILE_OPTIONS "-mavx;-mcrc32;-mpclmul"
)
set_source_files_properties(
    "${ACUL_SRC_DIR}/hash/crc32_avx2.cpp"
    PROPERTIES COMPILE_OPTIONS "-mavx2;-mpclmul"
)

# string
set_source_files_properties(
    "${ACUL_SRC_DIR}/string/string_sse42.cpp"
    PROPERTIES COMPILE_OPTIONS "-msse4.2"
)
set_source_files_properties(
    "${ACUL_SRC_DIR}/string/string_avx2.cpp"
    PROPERTIES COMPILE_OPTIONS "-mavx2"
)

# Disable LTO for isa specific sources
set(ISA_SOURCES
    "${ACUL_SRC_DIR}/hash/crc32_sse42.cpp"
    "${ACUL_SRC_DIR}/hash/crc32_avx.cpp"
    "${ACUL_SRC_DIR}/hash/crc32_avx2.cpp"
    "${ACUL_SRC_DIR}/string/string_sse42.cpp"
    "${ACUL_SRC_DIR}/string/string_avx2.cpp"
)
set_source_files_properties(${ISA_SOURCES} PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)