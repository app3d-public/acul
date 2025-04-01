#include <acul/log.hpp>
#include <acul/meta.hpp>

namespace acul
{
    namespace meta
    {
        hashmap<u32, const stream *> registered_streams;

        const stream *get_stream(u32 signature)
        {
            auto it = registered_streams.find(signature);
            if (it == registered_streams.end())
            {
                logError("Failed to recognize meta stream signature: 0x%08x", signature);
                return nullptr;
            }
            return it->second;
        }

        /*********************************
         **
         ** Default metadata
         **
         *********************************/

        namespace streams
        {
            block *read_raw_block(bin_stream &stream)
            {
                auto *block = alloc<acul::meta::raw_block>();
                stream.read(block->data_size);
                block->data = acul::alloc_n<char>(block->data_size);
                stream.read(block->data, block->data_size);
                return block;
            }

            void write_raw_block(bin_stream &stream, block *content)
            {
                auto *raw = static_cast<acul::meta::raw_block *>(content);
                stream.write(raw->data_size).write(raw->data, raw->data_size);
            }
        } // namespace streams
    } // namespace meta
} // namespace acul