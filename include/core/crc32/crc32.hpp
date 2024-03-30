#ifndef APP_CORE_CRC32_H
#define APP_CORE_CRC32_H

#include <filesystem>
#include "../std/types@basic.hpp"

namespace core
{

    /**
     * Loads the SIMD library based on the available instruction sets.
     *
     * @param parentFolder The path to the parent folder containing the library files.
     *
     * @return True if the SIMD library is successfully loaded, false otherwise.
     *
     * @throws None
     */
    bool loadSIMDLib(const std::filesystem::path &parentFolder);

    /**
     * Destroys the SIMD library if it is currently loaded.
     */
    void destroySIMDLib();

    /**
     * Calculate the CRC-32 checksum for the given input buffer.
     *
     * This function implements the CRC-32 checksum algorithm using SIMD instructions.
     * The algorithm is commonly used for error detection and is based on the polynomial representation of the CRC-32C
     * algorithm. The function takes an initial CRC value, a pointer to the input buffer, and the length of the buffer.
     * It iterates over the buffer, updating the CRC value using the SIMD instructions.
     * The final CRC value is returned after the iteration is complete.
     * @param crc The initial CRC value
     * @param buf Pointer to the input buffer
     * @param len Length of the input buffer
     *
     * @return The calculated CRC-32 checksum
     */
    extern u32 (*crc32)(u32, const char *, u32);
} // namespace core

#endif