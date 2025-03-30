#include <acul/log.hpp>
#include <acul/meta.hpp>

namespace acul
{
    namespace meta
    {
        acul::hashmap<u32, const stream *> g_Streams;

        const stream *get_stream(u32 signature)
        {
            auto it = g_Streams.find(signature);
            if (it == g_Streams.end())
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
            block *readRawBlock(acul::bin_stream &stream)
            {
                auto *block = acul::alloc<acul::meta::raw_block>();
                stream.read(block->dataSize);
                block->data = acul::alloc_n<char>(block->dataSize);
                stream.read(block->data, block->dataSize);
                return block;
            }

            void writeRawBlock(acul::bin_stream &stream, block *content)
            {
                auto *raw = static_cast<acul::meta::raw_block *>(content);
                stream.write(raw->dataSize).write(raw->data, raw->dataSize);
            }
        } // namespace streams
    } // namespace meta
} // namespace acul