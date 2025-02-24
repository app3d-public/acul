#pragma once

#include "api.hpp"
#include "../astl/stream.hpp"
#include "../astl/scalars.hpp"

#define SIGN_APP_PART_DEFAULT 0x5828

namespace meta
{
    struct Header
    {
        u32 signature;
        u64 blockSize;
    };
    struct Block
    {
        virtual ~Block() = default;

        virtual const u32 signature() const = 0;
    };

    struct Stream
    {
        Block *(*read)(astl::bin_stream &stream);
        void (*write)(astl::bin_stream &stream, Block *block);
    };

    /**
     * @brief Registers a stream handler for a specific block signature.
     * @param signature The signature identifying the block type.
     * @param stream The stream handler to register.
     */
    APPLIB_API void addStream(u32 signature, const Stream *stream);

    /**
     * @brief Initializes the stream handlers.
     * @param streams The stream handlers to initialize.
     */
    APPLIB_API void initStreams(const astl::vector<std::pair<u32, const Stream *>> &streams);

    /**
     * @brief Clears all registered stream handlers.
     */
    APPLIB_API void clearStreams();

    /**
     * @brief Retrieves a stream handler for a specific block signature.
     * @param signature The signature identifying the block type.
     * @return The stream handler for the specified signature, or nullptr if not found.
     */
    APPLIB_API const Stream *getStream(u32 signature);

    /*********************************
     **
     ** Default metadata
     **
     *********************************/

    namespace sign_block
    {
        constexpr u32 external_block = SIGN_APP_PART_DEFAULT << 16 | 0x3F84;
    }

    // Meta block reserved for common external resourcees
    struct ExternalBlock : public meta::Block
    {
        char *data = nullptr;
        u64 dataSize = 0;

        virtual const u32 signature() const { return sign_block::external_block; }

        ~ExternalBlock() { astl::release(data); }
    };

    namespace streams
    {
        APPLIB_API Block *readExternalBlock(astl::bin_stream &stream);
        APPLIB_API void writeExternalBlock(astl::bin_stream &stream, Block *block);

        inline Stream external_block = {readExternalBlock, writeExternalBlock};
    } // namespace streams
} // namespace meta