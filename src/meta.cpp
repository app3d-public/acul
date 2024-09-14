#include <core/log.hpp>
#include <core/meta.hpp>

namespace meta
{
    astl::hashmap<u32, Stream *> g_Streams;

    void addStream(u32 signature, Stream *stream)
    {
        auto [it, inserted] = g_Streams.try_emplace(signature, stream);
        if (!inserted)
        {
            logWarn("Stream 0x%08x already registered", signature);
            delete stream;
        }
    }

    void initStreams(const astl::vector<std::pair<u32, Stream *>> &streams)
    {
        g_Streams.insert(streams.begin(), streams.end());
    }

    void clearStreams()
    {
        for (auto &[id, stream] : g_Streams) delete stream;
        g_Streams.clear();
    }

    Stream *getStream(u32 signature)
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

    Block *ExternalStream::readFromStream(astl::bin_stream &stream)
    {
        ExternalBlock *block = new ExternalBlock;
        stream.read(block->dataSize);
        block->data = new char[block->dataSize];
        stream.read(block->data, block->dataSize);
        return block;
    }

    void ExternalStream::writeToStream(astl::bin_stream &stream, Block *content)
    {
        ExternalBlock *ext = static_cast<ExternalBlock *>(content);
        stream.write(ext->dataSize).write(ext->data, ext->dataSize);
    }
} // namespace meta