#include <acul/hash/utils.hpp>
#include <acul/io/fs/file.hpp>
#include <acul/io/fs/jatc.hpp>

// Default compression settings
#ifndef JATC_MIN_COMPRESS
    #define JATC_MIN_COMPRESS 10240
#endif
#ifndef JATC_COMPRESS_LEVEL
    #define JATC_COMPRESS_LEVEL 5
#endif

namespace acul::fs::jatc
{
    op_result inline make_op_error(u16 state, u32 code = 0) { return {state, JATC_OP_DOMAIN, code}; }

    bool write_header(entrypoint *entrypoint)
    {
        header header{JATC_MAGIC_NUMBER, JATC_VERSION};
        entrypoint->fd.write((const char *)(&header), sizeof(header));

        if (!entrypoint->fd.good())
        {
            entrypoint->fd.close();
            return false;
        }
        entrypoint->pos = entrypoint->fd.tellp();
        return true;
    }

    op_result rewrite_file(entrypoint *entrypoint, vector<index_entry *> &index_entries,
                           vector<vector<char>> &data_buffers, const string &path)
    {
        auto open_flags = std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc;
        entrypoint->fd.open(path.c_str(), open_flags);

        if (!entrypoint->fd.is_open()) return make_op_error(ACUL_OP_READ_ERROR, JATC_CODE_ENTRYPOINT);
        if (!write_header(entrypoint)) return make_op_error(ACUL_OP_WRITE_ERROR, JATC_CODE_HEADER);

        for (size_t i = 0; i < index_entries.size(); ++i)
        {
            auto &data = data_buffers[i];
            index_entries[i]->offset = entrypoint->pos;
            entrypoint->fd.write(data.data(), data.size());

            if (!entrypoint->fd.good())
            {
                entrypoint->fd.clear();
                return make_op_error(ACUL_OP_WRITE_ERROR, JATC_CODE_ENTRYPOINT);
            }
            entrypoint->pos += data.size();
        }
        return make_op_success();
    }

    entrypoint *cache::register_entrypoint(entrygroup *group)
    {
        auto entrypoint = alloc<jatc::entrypoint>();
        entrypoint->id = id_gen()();
        entrypoint->op_count = 0;
        group->entrypoints.push_back(entrypoint);
        return entrypoint;
    }

    op_result cache::deregister_entrypoint(entrypoint *entrypoint, entrygroup *group)
    {
        auto it = std::find(group->entrypoints.begin(), group->entrypoints.end(), entrypoint);
        if (it == group->entrypoints.end()) return make_op_error(ACUL_OP_OUT_OF_BOUNDS, JATC_CODE_ENTRYPOINT);
        group->entrypoints.erase(it);
        ++_op_count;
        _dispatch.dispatch([this, entrypoint, path = this->path(entrypoint, group)]() mutable {
            {
                exclusive_lock entrypoint_lock(entrypoint->lock);
                entrypoint->cv.wait(entrypoint_lock, [&]() { return entrypoint->op_count.load() == 0; });
                if (entrypoint->fd.is_open()) entrypoint->fd.close();
                entrypoint->cv.notify_all();
            }
            release(entrypoint);
            if (fs::exists(path.c_str())) fs::remove_file(path.c_str());
            {
                exclusive_lock write_lock(_lock);
                --_op_count;
                _cv.notify_all();
            }
        });
        return make_op_success();
    }

    op_result cache::read(entrypoint *entrypoint, entrygroup *group, const index_entry &entry, bin_stream &dst)
    {
        if (entry.size == 0) return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_OP_CODE_SIZE_ZERO);
        if (!entrypoint || !group) return make_op_error(ACUL_OP_NULLPTR);

        vector<char> buffer(entry.size);
        if (!buffer.data()) return make_op_error(ACUL_OP_INVALID_SIZE, ACUL_OP_CODE_SIZE_ZERO);

        {
            shared_lock lock(entrypoint->lock);
            entrypoint->cv.wait(lock, [&]() { return entrypoint->op_count.load() == 0; });

            auto fd = get_file_stream(entrypoint, group);
            if (!fd) return make_op_error(ACUL_OP_READ_ERROR, JATC_CODE_ENTRYPOINT);

            fd->seekg(entry.offset);
            if (!fd->good()) return make_op_error(ACUL_OP_SEEK_ERROR, JATC_CODE_ENTRYPOINT);

            fd->read(buffer.data(), entry.size);
            if (!fd->good())
            {
                fd->clear();
                return make_op_error(ACUL_OP_READ_ERROR, JATC_CODE_ENTRYPOINT);
            }
        }

        dst = bin_stream(std::move(buffer));

        if (entry.compressed > 0)
        {
            vector<char> decompressed;
            auto dr = fs::decompress(dst.data() + dst.pos(), dst.size() - dst.pos(), decompressed);
            if (!dr.success()) return dr;
            dst = bin_stream(std::move(decompressed));
        }

        if (crc32(0, dst.data(), dst.size()) != entry.checksum)
            return make_op_error(ACUL_OP_COMPRESS_ERROR, JATC_CODE_INDEX);
        return make_op_success();
    }

    op_result cache::filter_index_entries(entrypoint *entrypoint, entrygroup *group,
                                          vector<index_entry *> &index_entries)
    {
        {
            shared_lock read_lock(entrypoint->lock);
            entrypoint->cv.wait(read_lock, [&]() { return entrypoint->op_count.load() == 0; });
        }
        auto fd = get_file_stream(entrypoint, group);
        if (!fd) return make_op_error(ACUL_OP_READ_ERROR, JATC_CODE_ENTRYPOINT);
        vector<vector<char>> data_buffers;
        data_buffers.reserve(index_entries.size());
        for (index_entry *entry : index_entries)
        {
            fd->seekg(entry->offset, std::ios::beg);
            vector<char> buffer(entry->size);
            fd->read(buffer.data(), entry->size);
            if (!fd->good())
            {
                fd->clear();
                return make_op_error(ACUL_OP_SEEK_ERROR, JATC_CODE_ENTRYPOINT);
            }
            data_buffers.push_back(std::move(buffer));
        }
        fd->close();
        ++entrypoint->op_count;

        acul::exclusive_lock write_lock(entrypoint->lock);
        rewrite_file(entrypoint, index_entries, data_buffers, path(entrypoint, group));
        --entrypoint->op_count;
        entrypoint->cv.notify_all();
        return make_op_success();
    }

    op_result cache::write_to_entrypoint(const request &request, response &response, index_entry &index_entry,
                                         const char *buffer, size_t size)
    {
        auto fd = get_file_stream(request.entrypoint, request.group);
        if (!fd) return make_op_error(ACUL_OP_READ_ERROR, JATC_CODE_ENTRYPOINT);
        else
        {
            fd->seekp(0, std::ios::end);
            u64 data_offset = request.entrypoint->fd.tellp();
            fd->write(buffer, size);

            if (!request.entrypoint->fd.good())
            {
                fd->clear();
                return make_op_error(ACUL_OP_WRITE_ERROR, JATC_CODE_ENTRYPOINT);
            }
            else
            {
                request.entrypoint->pos += data_offset + index_entry.size;
                index_entry.offset = data_offset;
                response.state = ACUL_OP_SUCCESS;
                response.entry(index_entry);
            }
        }
        return make_op_success();
    }

    void cache::write_to_entrypoint(const request &request, response &response)
    {
        vector<char> compressed;
        const char *dst_buffer = nullptr;
        size_t dst_size = 0;
        bin_stream stream;
        index_entry index_entry;
        op_result result = make_op_success();
        if (!request.write_callback)
        {
            result.state = ACUL_OP_NULLPTR;
            goto on_error;
        }
        request.write_callback(stream);

        if (stream.size() > JATC_MIN_COMPRESS)
        {
            auto data = stream.data() + stream.pos();
            auto size = stream.size() - stream.pos();
            result = fs::compress(data, size, compressed, JATC_COMPRESS_LEVEL);
            if (result.success())
            {
                dst_buffer = compressed.data();
                dst_size = compressed.size();
                index_entry.compressed = JATC_COMPRESS_LEVEL;
            }
            else goto on_error;
        }
        else
        {
            dst_buffer = stream.data();
            dst_size = stream.size();
            index_entry.compressed = 0;
        }
        index_entry.size = stream.size();
        index_entry.checksum = acul::crc32(0, stream.data(), stream.size());
        request.entrypoint->lock.lock();
        result = write_to_entrypoint(request, response, index_entry, dst_buffer, dst_size);
    on_error:
        if (!result.success()) response.state = result.state;
        --request.entrypoint->op_count;
        response.ready_promise.set_value();
        request.entrypoint->cv.notify_all();
        request.entrypoint->lock.unlock();
    }

    std::fstream *cache::get_file_stream(entrypoint *entrypoint, entrygroup *group)
    {
        if (!entrypoint->fd.is_open())
        {
            auto path = this->path(entrypoint, group);
            auto open_flags = std::ios::binary | std::ios::in | std::ios::out | std::ios::app;
            entrypoint->fd = std::fstream(path.c_str(), open_flags);
            if (!entrypoint->fd.is_open()) return nullptr;
            if (entrypoint->pos == 0 && !write_header(entrypoint)) return nullptr;
        }
        return &entrypoint->fd;
    }
} // namespace acul::fs::jatc