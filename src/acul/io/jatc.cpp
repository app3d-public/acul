#include <acul/hash/utils.hpp>
#include <acul/io/jatc.hpp>
#include <acul/log.hpp>
#include <inttypes.h>

#ifndef ACUL_BUILD_MIN

namespace acul
{
    namespace io
    {
        namespace file
        {
            namespace jatc
            {
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

                void rewrite_file(entrypoint *entrypoint, vector<index_entry *> &index_entries,
                                  vector<vector<char>> &data_buffers, const string &path)
                {
                    auto open_flags = std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc;
                    entrypoint->fd.open(path.c_str(), open_flags);

                    if (!entrypoint->fd.is_open())
                    {
                        logError("Failed to init entrypoint file");
                        return;
                    }

                    if (!write_header(entrypoint))
                    {
                        logError("Failed to write header to entrypoint file");
                        return;
                    }

                    for (size_t i = 0; i < index_entries.size(); ++i)
                    {
                        auto &data = data_buffers[i];
                        index_entries[i]->offset = entrypoint->pos;
                        entrypoint->fd.write(data.data(), data.size());

                        if (!entrypoint->fd.good())
                        {
                            entrypoint->fd.clear();
                            logError("Failed to write data to entrypoint file");
                            return;
                        }
                        entrypoint->pos += data.size();
                    }
                }

                entrypoint *cache::register_entrypoint(entrygroup *group)
                {
                    auto entrypoint = alloc<jatc::entrypoint>();
                    entrypoint->id = id_gen()();
                    entrypoint->op_count = 0;
                    logInfo("Registering entrypoint: %" PRIx64, entrypoint->id);
                    group->entrypoints.push_back(entrypoint);
                    return entrypoint;
                }

                void cache::deregister_entrypoint(entrypoint *entrypoint, entrygroup *group)
                {
                    logInfo("Deregistering entrypoint: %" PRIx64, entrypoint->id);
                    auto it = std::find(group->entrypoints.begin(), group->entrypoints.end(), entrypoint);
                    if (it == group->entrypoints.end())
                    {
                        logWarn("Entrypoint %" PRIx64 " not found", entrypoint->id);
                        return;
                    }
                    group->entrypoints.erase(it);
                    ++_op_count;
                    _dispatch.dispatch([=, this, path = this->path(entrypoint, group)]() mutable {
                        {
                            acul::exclusive_lock entrypoint_lock(entrypoint->lock);
                            entrypoint->cv.wait(entrypoint_lock, [&]() { return entrypoint->op_count.load() == 0; });
                            if (entrypoint->fd.is_open()) entrypoint->fd.close();
                            entrypoint->cv.notify_all();
                        }
                        acul::release(entrypoint);
                        if (io::file::exists(path.c_str()))
                        {
                            logInfo("Deleting cache file: %s", path.c_str());
                            io::file::remove_file(path.c_str());
                        }
                        {
                            acul::exclusive_lock write_lock(_lock);
                            --_op_count;
                            _cv.notify_all();
                        }
                    });
                }

                op_state cache::read(entrypoint *entrypoint, entrygroup *group, const index_entry &entry,
                                     bin_stream &dst)
                {
                    logInfo("Reading index entry");
                    if (entry.size == 0)
                    {
                        logError("Entry size is zero.");
                        return op_state::error;
                    }

                    if (!entrypoint || !group)
                    {
                        logError("Invalid entrypoint or group pointer.");
                        return op_state::error;
                    }

                    vector<char> buffer(entry.size);
                    if (!buffer.data())
                    {
                        logError("Failed to allocate buffer of size: %" PRIu64, entry.size);
                        return op_state::error;
                    }

                    {
                        shared_lock lock(entrypoint->lock);
                        entrypoint->cv.wait(lock, [&]() { return entrypoint->op_count.load() == 0; });

                        auto fd = get_file_stream(entrypoint, group);
                        if (!fd)
                        {
                            logError("Failed to open file stream.");
                            return op_state::error;
                        }

                        fd->seekg(entry.offset);
                        if (!fd->good())
                        {
                            logError("Failed to seek file to offset: %" PRIu64, entry.offset);
                            return op_state::error;
                        }

                        fd->read(buffer.data(), entry.size);
                        if (!fd->good())
                        {
                            logError("Failed to read data at offset: %" PRIu64, entry.offset);
                            fd->clear();
                            return op_state::error;
                        }
                    }

                    dst = bin_stream(std::move(buffer));

                    if (entry.compressed > 0)
                    {
                        vector<char> decompressed;
                        logInfo("Decompressing data.");
                        if (!io::file::decompress(dst.data() + dst.pos(), dst.size() - dst.pos(), decompressed))
                        {
                            logError("Failed to decompress data at offset: %" PRIu64, entry.offset);
                            return op_state::error;
                        }
                        dst = bin_stream(std::move(decompressed));
                    }

                    logInfo("Verifying checksum.");
                    if (acul::crc32(0, dst.data(), dst.size()) != entry.checksum)
                    {
                        logError("Invalid entrypoint index checksum.");
                        return op_state::checksum_mismatch;
                    }

                    return op_state::success;
                }

                void cache::filter_index_entries(entrypoint *entrypoint, entrygroup *group,
                                                 vector<index_entry *> &index_entries)
                {
                    logInfo("Reading index entries for entrypoint: %" PRIx64, entrypoint->id);
                    {
                        shared_lock read_lock(entrypoint->lock);
                        entrypoint->cv.wait(read_lock, [&]() { return entrypoint->op_count.load() == 0; });
                    }
                    auto fd = get_file_stream(entrypoint, group);
                    if (!fd)
                    {
                        logError("Failed to open file stream for entrypoint: %" PRIx64, entrypoint->id);
                        return;
                    }
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
                            logError("Failed to read data from entrypoint file.");
                            return;
                        }
                        data_buffers.push_back(std::move(buffer));
                    }
                    fd->close();
                    ++entrypoint->op_count;

                    logInfo("Overwriting index entries for entrypoint: %" PRIx64, entrypoint->id);
                    acul::exclusive_lock write_lock(entrypoint->lock);
                    rewrite_file(entrypoint, index_entries, data_buffers, path(entrypoint, group));
                    --entrypoint->op_count;
                    entrypoint->cv.notify_all();
                }

                const char *cache::write_to_entrypoint(const request &request, response &response,
                                                       index_entry &index_entry, const char *buffer, size_t size)
                {
                    auto fd = get_file_stream(request.entrypoint, request.group);
                    if (!fd)
                        return "Failed to get file stream.";
                    else
                    {
                        fd->seekp(0, std::ios::end);
                        u64 data_offset = request.entrypoint->fd.tellp();
                        fd->write(buffer, size);

                        if (!request.entrypoint->fd.good())
                        {
                            fd->clear();
                            return "Failed to write to file.";
                        }
                        else
                        {
                            request.entrypoint->pos += data_offset + index_entry.size;
                            index_entry.offset = data_offset;
                            response.state = io::file::op_state::success;
                            response.entry(index_entry);
                        }
                    }
                    return nullptr;
                }

                void cache::write_to_entrypoint(const request &request, response &response)
                {
                    vector<char> compressed;
                    const char *dst_buffer = nullptr, *err = nullptr;
                    size_t dst_size = 0;
                    bin_stream stream;
                    index_entry index_entry;
                    if (!request.write_callback)
                    {
                        err = "Write callback cannot be null.";
                        goto on_error;
                    }
                    request.write_callback(stream);

                    if (stream.size() > JATC_MIN_COMPRESS)
                    {
                        auto data = stream.data() + stream.pos();
                        auto size = stream.size() - stream.pos();
                        if (io::file::compress(data, size, compressed, JATC_COMPRESS_LEVEL))
                        {
                            dst_buffer = compressed.data();
                            dst_size = compressed.size();
                            index_entry.compressed = JATC_COMPRESS_LEVEL;
                        }
                        else
                        {
                            err = "Failed to compress entrypoint index data.";
                            goto on_error;
                        }
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
                    err = write_to_entrypoint(request, response, index_entry, dst_buffer, dst_size);
                on_error:
                    if (err)
                    {
                        logError("%s", err);
                        response.state = op_state::error;
                    }
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
                        logInfo("Loading cache file: %s", path.c_str());
                        auto openFlags = std::ios::binary | std::ios::in | std::ios::out | std::ios::app;
                        entrypoint->fd = std::fstream(path.c_str(), openFlags);
                        if (!entrypoint->fd.is_open())
                        {
                            logError("%s: %s", strerror(errno), path.c_str());
                            return nullptr;
                        }
                        if (entrypoint->pos == 0 && !write_header(entrypoint))
                        {
                            logError("Failed to write header.");
                            return nullptr;
                        }
                    }
                    return &entrypoint->fd;
                }
            } // namespace jatc
        } // namespace file
    } // namespace io
} // namespace acul
#endif