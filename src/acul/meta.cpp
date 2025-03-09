#include <acul/log.hpp>
#include <acul/meta.hpp>

namespace meta
{
    astl::hashmap<u32, const Stream *> g_Streams;

    void addStream(u32 signature, const Stream *stream)
    {
        auto [it, inserted] = g_Streams.try_emplace(signature, stream);
        if (!inserted) logWarn("Stream 0x%08x already registered", signature);
    }

    void initStreams(const astl::vector<std::pair<u32, const Stream *>> &streams)
    {
        g_Streams.insert(streams.begin(), streams.end());
    }

    void clearStreams() { g_Streams.clear(); }

    const Stream *getStream(u32 signature)
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
        Block *readExternalBlock(astl::bin_stream &stream)
        {
            ExternalBlock *block = astl::alloc<ExternalBlock>();
            stream.read(block->dataSize);
            block->data = astl::alloc_n<char>(block->dataSize);
            stream.read(block->data, block->dataSize);
            return block;
        }

        void writeExternalBlock(astl::bin_stream &stream, Block *content)
        {
            ExternalBlock *ext = static_cast<ExternalBlock *>(content);
            stream.write(ext->dataSize).write(ext->data, ext->dataSize);
        }
    } // namespace streams
} // namespace meta