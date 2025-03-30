#pragma once

#include <condition_variable>
#include <fstream>
#include <oneapi/tbb/flow_graph.h>
#include "../shared_mutex.hpp"
#include "../stream.hpp"
#include "../string/string.hpp"
#include "../task.hpp"
#include "file.hpp"

// Journalable Asynchronous Temporary Cache

#define JATC_VERSION_MAJOR 1
#define JATC_VERSION_MINOR 0
#define JATC_VERSION_PATCH 0
#define JATC_VERSION       (JATC_VERSION_MAJOR << 16 | JATC_VERSION_MINOR << 8 | JATC_VERSION_PATCH)
#define JATC_MAGIC_NUMBER  0x4A1950B9
// Default compression settings
#ifndef JATC_MIN_COMPRESS
    #define JATC_MIN_COMPRESS 10240
#endif
#ifndef JATC_COMPRESS_LEVEL
    #define JATC_COMPRESS_LEVEL 5
#endif

namespace acul
{
    namespace io
    {
        namespace file
        {
            namespace jatc
            {
                struct alignas(8) IndexEntry
                {
                    u64 offset;
                    u64 size;
                    u32 checksum;
                    u8 compressed;
                    u8 padding[3];
                };

                struct Header
                {
                    u32 magicNumber;
                    u32 version;
                };

                struct EntryPoint
                {
                    u64 id;
                    u64 pos = 0;
                    std::fstream fd;

                    // Sync
                    acul::shared_mutex lock;
                    std::condition_variable_any cv;
                    std::atomic<int> op_count;

                    void await()
                    {
                        acul::shared_lock read_lock(lock);
                        cv.wait(read_lock, [&]() { return op_count.load() == 0; });
                    }
                };

                struct EntryGroup
                {
                    std::string name;
                    acul::vector<EntryPoint *> entrypoints;
                };

                struct Request
                {
                    std::function<void(acul::bin_stream &)> write_callback;
                    EntryGroup *group = nullptr;
                    EntryPoint *entrypoint = nullptr;
                };

                struct Response
                {
                    io::file::op_state state;
                    std::promise<void> ready_promise;
                    EntryGroup *group = nullptr;
                    EntryPoint *entrypoint = nullptr;

                    IndexEntry &entry()
                    {
                        if (state == op_state::undefined) ready_promise.get_future().wait();
                        return _entry;
                    }

                    void entry(const IndexEntry &entry) { _entry = entry; }

                private:
                    IndexEntry _entry;
                };

                struct FlowOutput
                {
                    Request req;
                    Response *res;
                };

                class APPLIB_API Cache
                {
                public:
                    Cache(const std::filesystem::path &path, task::ThreadDispatch &dispatch)
                        : _path(path),
                          _dispatch(dispatch),
                          _writeNode(acul::alloc<oneapi::tbb::flow::function_node<FlowOutput>>(
                              _graph, tbb::flow::unlimited,
                              [this](const FlowOutput &output) { this->writeToEntrypoint(output.req, *output.res); }))
                    {
                    }

                    ~Cache() { release(_writeNode); }

                    std::filesystem::path path(EntryPoint *entrypoint, EntryGroup *group)
                    {
                        string name = format("entrypoint-%s-%llx.jatc", group->name.c_str(), entrypoint->id);
                        return _path / name.c_str();
                    }

                    EntryPoint *registerEntrypoint(EntryGroup *group);

                    void deregisterEntrypoint(EntryPoint *entrypoint, EntryGroup *group);

                    void addRequest(const Request &request, Response *response)
                    {
                        response->state = io::file::op_state::undefined;
                        response->group = request.group;
                        response->entrypoint = request.entrypoint;
                        ++request.entrypoint->op_count;
                        _writeNode->try_put({request, response});
                    }

                    void await()
                    {
                        acul::shared_lock read_lock(_lock);
                        _cv.wait(read_lock, [&]() { return _op_count.load() == 0; });
                    }

                    op_state read(EntryPoint *entrypoint, EntryGroup *group, const IndexEntry &entry,
                                   acul::bin_stream &dst);

                    // Filter index blocks and replace them in the file
                    void filterIndexEntries(EntryPoint *entrypoint, EntryGroup *group,
                                            acul::vector<IndexEntry *> &indexEntries);

                private:
                    std::filesystem::path _path;
                    task::ThreadDispatch &_dispatch;
                    oneapi::tbb::flow::graph _graph;
                    tbb::flow::function_node<FlowOutput> *_writeNode;
                    acul::shared_mutex _lock;
                    std::condition_variable_any _cv;
                    std::atomic<int> _op_count = 0;

                    // Returns nullptr if success, otherwise returns error message.
                    const char *writeToEntryPoint(const Request &request, Response &response, IndexEntry &indexEntry,
                                                  const char *buffer, size_t size);

                    void writeToEntrypoint(const Request &request, Response &response);

                    std::fstream *getFileStream(EntryPoint *entrypoint, EntryGroup *group);
                };
            } // namespace jatc
        } // namespace file
    } // namespace io
} // namespace acul

namespace acul
{
    template <>
    inline bin_stream &bin_stream::write(const io::file::jatc::IndexEntry &entry)
    {
        return write(entry.offset).write(entry.size).write(entry.checksum).write(entry.compressed);
    }

    template <>
    inline bin_stream &bin_stream::read(io::file::jatc::IndexEntry &entry)
    {
        return read(entry.offset).read(entry.size).read(entry.checksum).read(entry.compressed);
    }
} // namespace acul