#pragma once

#include "scalars.hpp"
#include "stream.hpp"
#include "api.hpp"
#include "hash/hashmap.hpp"

namespace acul
{
    namespace meta
    {
        struct header
        {
            u32 signature;
            u64 blockSize;
        };
        struct block
        {
            virtual ~block() = default;

            virtual u32 signature() const = 0;
        };

        struct stream
        {
            block *(*read)(acul::bin_stream &stream);
            void (*write)(acul::bin_stream &stream, block *block);
        };

        APPLIB_API const stream *get_stream(u32 signature);
        extern APPLIB_API acul::hashmap<u32, const stream *> g_Streams;

        /*********************************
         **
         ** Default metadata
         **
         *********************************/

        namespace sign_block
        {
            enum : u32
            {
                raw_block = 0xF82E95C8
            };
        }

        // Meta block reserved for common external resourcees
        struct raw_block : public block
        {
            char *data;
            u64 dataSize;

            raw_block(char *data = nullptr, u64 dataSize = 0) : data(data), dataSize(dataSize) {}

            virtual u32 signature() const { return sign_block::raw_block; }

            ~raw_block() { acul::release(data); }
        };

        namespace streams
        {
            APPLIB_API block *readRawBlock(acul::bin_stream &stream);
            APPLIB_API void writeRawBlock(acul::bin_stream &stream, block *block);

            inline stream raw_block = {readRawBlock, writeRawBlock};
        } // namespace streams
    } // namespace meta
} // namespace acul