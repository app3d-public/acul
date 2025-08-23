#pragma once

#include <condition_variable>
#include <fstream>
#include <oneapi/tbb/flow_graph.h>
#include "../../shared_mutex.hpp"
#include "../../stream.hpp"
#include "../../string/utils.hpp"
#include "../../task.hpp"
#include "../file.hpp"
#include "../path.hpp"

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
                struct alignas(8) index_entry
                {
                    u64 offset;
                    u64 size;
                    u32 checksum;
                    u8 compressed;
                    u8 padding[3];
                };

                struct header
                {
                    u32 magic_number;
                    u32 version;
                };

                struct entrypoint
                {
                    u64 id;
                    u64 pos = 0;
                    std::fstream fd;

                    // Sync
                    shared_mutex lock;
                    std::condition_variable_any cv;
                    std::atomic<int> op_count;

                    void await()
                    {
                        shared_lock read_lock(lock);
                        cv.wait(read_lock, [&]() { return op_count.load() == 0; });
                    }
                };

                struct entrygroup
                {
                    string name;
                    vector<entrypoint *> entrypoints;
                };

                struct request
                {
                    std::function<void(bin_stream &)> write_callback;
                    entrygroup *group = nullptr;
                    struct entrypoint *entrypoint = nullptr;
                };

                struct response
                {
                    io::file::op_state state;
                    std::promise<void> ready_promise;
                    entrygroup *group = nullptr;
                    struct entrypoint *entrypoint = nullptr;

                    index_entry &entry()
                    {
                        if (state == op_state::unknown) ready_promise.get_future().wait();
                        return _entry;
                    }

                    void entry(const index_entry &entry) { _entry = entry; }

                private:
                    index_entry _entry;
                };

                struct flow_output
                {
                    request req;
                    response *res;
                };

                class APPLIB_API cache
                {
                public:
                    cache(const string &path, task::thread_dispatch &dispatch)
                        : _path(path),
                          _dispatch(dispatch),
                          _write_node(alloc<oneapi::tbb::flow::function_node<flow_output>>(
                              _graph, tbb::flow::unlimited, [this](const flow_output &output) {
                                  this->write_to_entrypoint(output.req, *output.res);
                              }))
                    {
                    }

                    ~cache() { release(_write_node); }

                    string path(entrypoint *entrypoint, entrygroup *group)
                    {
                        string name = format("entrypoint-%s-%llx.jatc", group->name.c_str(), entrypoint->id);
                        return _path / name;
                    }

                    entrypoint *register_entrypoint(entrygroup *group);

                    void deregister_entrypoint(entrypoint *entrypoint, entrygroup *group);

                    void add_request(const request &request, response *response)
                    {
                        response->state = io::file::op_state::unknown;
                        response->group = request.group;
                        response->entrypoint = request.entrypoint;
                        ++request.entrypoint->op_count;
                        _write_node->try_put({request, response});
                    }

                    void await()
                    {
                        shared_lock read_lock(_lock);
                        _cv.wait(read_lock, [&]() { return _op_count.load() == 0; });
                    }

                    op_state read(entrypoint *entrypoint, entrygroup *group, const index_entry &entry, bin_stream &dst);

                    // Filter index blocks and replace them in the file
                    void filter_index_entries(entrypoint *entrypoint, entrygroup *group,
                                              vector<index_entry *> &index_entries);

                private:
                    io::path _path;
                    task::thread_dispatch &_dispatch;
                    oneapi::tbb::flow::graph _graph;
                    tbb::flow::function_node<flow_output> *_write_node;
                    shared_mutex _lock;
                    std::condition_variable_any _cv;
                    std::atomic<int> _op_count = 0;

                    // Returns nullptr if success, otherwise returns error message.
                    const char *write_to_entrypoint(const request &request, response &response, index_entry &index,
                                                    const char *buffer, size_t size);

                    void write_to_entrypoint(const request &request, response &response);

                    std::fstream *get_file_stream(entrypoint *entrypoint, entrygroup *group);
                };
            } // namespace jatc
        } // namespace file
    } // namespace io
} // namespace acul

namespace acul
{
    template <>
    inline bin_stream &bin_stream::write(const io::file::jatc::index_entry &entry)
    {
        return write(entry.offset).write(entry.size).write(entry.checksum).write(entry.compressed);
    }

    template <>
    inline bin_stream &bin_stream::read(io::file::jatc::index_entry &entry)
    {
        return read(entry.offset).read(entry.size).read(entry.checksum).read(entry.compressed);
    }
} // namespace acul