#pragma once

#include "api.hpp"
#include "std/basic_types.hpp"
#include "std/stream.hpp"

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

    class APPLIB_API Stream
    {
    public:
        virtual ~Stream() = default;

        virtual Block *readFromStream(BinStream &stream) = 0;

        virtual void writeToStream(BinStream &stream, Block *block) = 0;
    };

    constexpr u32 sign_block_external = SIGN_APP_PART_DEFAULT << 16 | 0x3F84;

    /**
     * @brief Registers a stream handler for a specific block signature.
     * @param signature The signature identifying the block type.
     * @param stream The stream handler to register.
     */
    APPLIB_API void addStream(u32 signature, Stream *stream);

    /**
     * @brief Initializes the stream handlers.
     * @param streams The stream handlers to initialize.
     */
    APPLIB_API void initStreams(const DArray<std::pair<u32, Stream *>> &streams);

    /**
     * @brief Clears all registered stream handlers.
     */
    APPLIB_API void clearStreams();

    /**
     * @brief Retrieves a stream handler for a specific block signature.
     * @param signature The signature identifying the block type.
     * @return The stream handler for the specified signature, or nullptr if not found.
     */
    APPLIB_API Stream *getStream(u32 signature);

    /*********************************
     **
     ** Default metadata
     **
     *********************************/

    // Meta block reserved for common external resourcees
    struct ExternalBlock : public meta::Block
    {
        char *data = nullptr;
        u64 dataSize = 0;

        virtual const u32 signature() const { return sign_block_external; }

        ~ExternalBlock() { delete data; }
    };

    class APPLIB_API ExternalStream final : public meta::Stream
    {
    public:
        virtual meta::Block *readFromStream(BinStream &stream) override;

        virtual void writeToStream(BinStream &stream, meta::Block *block) override;
    };

} // namespace meta