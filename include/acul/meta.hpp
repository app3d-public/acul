#pragma once

#include "api.hpp"
#include "hash/hashmap.hpp"
#include "scalars.hpp"
#include "stream.hpp"

namespace acul
{
    namespace meta
    {
        struct header
        {
            u32 signature;
            u64 block_size;
        };
        struct block
        {
            virtual ~block() = default;

            virtual u32 signature() const = 0;
        };

        struct stream
        {
            block *(*read)(bin_stream &stream);
            void (*write)(bin_stream &stream, block *block);
        };

        APPLIB_API const stream *get_stream(u32 signature);
        extern APPLIB_API hashmap<u32, const stream *> registered_streams;

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
            u64 data_size;

            raw_block(char *data = nullptr, u64 data_size = 0) : data(data), data_size(data_size) {}

            virtual u32 signature() const { return sign_block::raw_block; }

            ~raw_block() { acul::release(data); }
        };

        namespace streams
        {
            APPLIB_API block *read_raw_block(bin_stream &stream);
            APPLIB_API void write_raw_block(bin_stream &stream, block *block);

            inline stream raw_block = {read_raw_block, write_raw_block};
        } // namespace streams
    } // namespace meta
} // namespace acul