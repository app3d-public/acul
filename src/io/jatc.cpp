#include <astl/hash.hpp>
#include <core/log.hpp>
#include <io/jatc.hpp>

namespace io
{
    namespace file
    {
        namespace jatc
        {
            bool writeHeader(EntryPoint *entrypoint)
            {
                Header header{JATC_MAGIC_NUMBER, JATC_VERSION};
                entrypoint->fd.write((const char *)(&header), sizeof(Header));

                if (!entrypoint->fd.good())
                {
                    entrypoint->fd.close();
                    return false;
                }
                entrypoint->pos = entrypoint->fd.tellp();
                return true;
            }

            void rewriteFile(EntryPoint *entrypoint, astl::vector<IndexEntry *> &indexEntries,
                             astl::vector<astl::vector<char>> &dataBuffers, const std::filesystem::path &path)
            {
                auto openFlags = std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc;
                entrypoint->fd.open(path, openFlags);

                if (!entrypoint->fd.is_open())
                {
                    logError("Failed to init entrypoint file");
                    return;
                }

                if (!writeHeader(entrypoint))
                {
                    logError("Failed to write header to entrypoint file");
                    return;
                }

                for (size_t i = 0; i < indexEntries.size(); ++i)
                {
                    auto &data = dataBuffers[i];
                    indexEntries[i]->offset = entrypoint->pos;
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

            EntryPoint *Cache::registerEntrypoint(EntryGroup *group)
            {
                auto entrypoint = astl::alloc<EntryPoint>();
                entrypoint->id = astl::IDGen()();
                entrypoint->op_count = 0;
                logInfo("Registering entrypoint: %llx", entrypoint->id);
                group->entrypoints.push_back(entrypoint);
                return entrypoint;
            }

            void Cache::deregisterEntrypoint(EntryPoint *entrypoint, EntryGroup *group)
            {
                logInfo("Deregistering entrypoint: %llx", entrypoint->id);
                auto it = std::find(group->entrypoints.begin(), group->entrypoints.end(), entrypoint);
                if (it == group->entrypoints.end())
                {
                    logWarn("Entrypoint %llx not found", entrypoint->id);
                    return;
                }
                group->entrypoints.erase(it);
                ++_op_count;
                _dispatch.dispatch([=, this, path = this->path(entrypoint, group)]() mutable {
                    {
                        astl::exclusive_lock entrypoint_lock(entrypoint->lock);
                        entrypoint->cv.wait(entrypoint_lock, [&]() { return entrypoint->op_count.load() == 0; });
                        if (entrypoint->fd.is_open()) entrypoint->fd.close();
                        entrypoint->cv.notify_all();
                    }
                    astl::release(entrypoint);
                    if (std::filesystem::exists(path))
                    {
                        logInfo("Deleting cache file: %s", path.string().c_str());
                        std::filesystem::remove(path);
                    }
                    {
                        astl::exclusive_lock write_lock(_lock);
                        --_op_count;
                        _cv.notify_all();
                    }
                });
            }

            ReadState Cache::read(EntryPoint *entrypoint, EntryGroup *group, const IndexEntry &entry,
                                  astl::bin_stream &dst)
            {
                logInfo("Reading index entry");
                if (entry.size == 0)
                {
                    logError("Entry size is zero.");
                    return ReadState::Error;
                }

                if (!entrypoint || !group)
                {
                    logError("Invalid entrypoint or group pointer.");
                    return ReadState::Error;
                }

                astl::vector<char> buffer(entry.size);
                if (!buffer.data())
                {
                    logError("Failed to allocate buffer of size: %llu", entry.size);
                    return ReadState::Error;
                }

                {
                    astl::shared_lock lock(entrypoint->lock);
                    entrypoint->cv.wait(lock, [&]() { return entrypoint->op_count.load() == 0; });

                    auto fd = getFileStream(entrypoint, group);
                    if (!fd)
                    {
                        logError("Failed to open file stream.");
                        return ReadState::Error;
                    }

                    fd->seekg(entry.offset);
                    if (!fd->good())
                    {
                        logError("Failed to seek file to offset: %llu", entry.offset);
                        return ReadState::Error;
                    }

                    fd->read(buffer.data(), entry.size);
                    if (!fd->good())
                    {
                        logError("Failed to read data at offset: %llu", entry.offset);
                        fd->clear();
                        return ReadState::Error;
                    }
                }

                dst = astl::bin_stream(std::move(buffer));

                if (entry.compressed > 0)
                {
                    astl::vector<char> decompressed;
                    logInfo("Decompressing data.");
                    if (!io::file::decompress(dst.data() + dst.pos(), dst.size() - dst.pos(), decompressed))
                    {
                        logError("Failed to decompress data at offset: %llu", entry.offset);
                        return ReadState::Error;
                    }
                    dst = astl::bin_stream(std::move(decompressed));
                }

                logInfo("Verifying checksum.");
                if (astl::crc32(0, dst.data(), dst.size()) != entry.checksum)
                {
                    logError("Invalid entrypoint index checksum.");
                    return ReadState::ChecksumMismatch;
                }

                return ReadState::Success;
            }

            void Cache::filterIndexEntries(EntryPoint *entrypoint, EntryGroup *group,
                                           astl::vector<IndexEntry *> &indexEntries)
            {
                logInfo("Reading index entries for entrypoint: %llx", entrypoint->id);
                {
                    astl::shared_lock read_lock(entrypoint->lock);
                    entrypoint->cv.wait(read_lock, [&]() { return entrypoint->op_count.load() == 0; });
                }
                auto fd = getFileStream(entrypoint, group);
                if (!fd)
                {
                    logError("Failed to open file stream for entrypoint: %llx", entrypoint->id);
                    return;
                }
                astl::vector<astl::vector<char>> dataBuffers;
                dataBuffers.reserve(indexEntries.size());
                for (IndexEntry *entry : indexEntries)
                {
                    fd->seekg(entry->offset, std::ios::beg);
                    astl::vector<char> buffer(entry->size);
                    fd->read(buffer.data(), entry->size);
                    if (!fd->good())
                    {
                        fd->clear();
                        logError("Failed to read data from entrypoint file.");
                        return;
                    }
                    dataBuffers.push_back(std::move(buffer));
                }
                fd->close();
                ++entrypoint->op_count;

                logInfo("Overwriting index entries for entrypoint: %llx", entrypoint->id);
                astl::exclusive_lock write_lock(entrypoint->lock);
                rewriteFile(entrypoint, indexEntries, dataBuffers, path(entrypoint, group));
                --entrypoint->op_count;
                entrypoint->cv.notify_all();
            }

            const char *Cache::writeToEntryPoint(const Request &request, Response &response, IndexEntry &indexEntry,
                                                 const char *buffer, size_t size)
            {
                auto fd = getFileStream(request.entrypoint, request.group);
                if (!fd)
                    return "Failed to get file stream.";
                else
                {
                    fd->seekp(0, std::ios::end);
                    u64 dataOffset = request.entrypoint->fd.tellp();
                    fd->write(buffer, size);

                    if (!request.entrypoint->fd.good())
                    {
                        fd->clear();
                        return "Failed to write to file.";
                    }
                    else
                    {
                        request.entrypoint->pos += dataOffset + indexEntry.size;
                        indexEntry.offset = dataOffset;
                        response.state = io::file::ReadState::Success;
                        response.entry(indexEntry);
                    }
                }
                return nullptr;
            }

            void Cache::writeToEntrypoint(const Request &request, Response &response)
            {
                astl::vector<char> compressed;
                const char *dstBuffer = nullptr, *err = nullptr;
                size_t dstSize = 0;
                astl::bin_stream stream;
                IndexEntry indexEntry;
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
                        dstBuffer = compressed.data();
                        dstSize = compressed.size();
                        indexEntry.compressed = JATC_COMPRESS_LEVEL;
                    }
                    else
                    {
                        err = "Failed to compress entrypoint index data.";
                        goto on_error;
                    }
                }
                else
                {
                    dstBuffer = stream.data();
                    dstSize = stream.size();
                    indexEntry.compressed = 0;
                }
                indexEntry.size = stream.size();
                indexEntry.checksum = astl::crc32(0, stream.data(), stream.size());
                request.entrypoint->lock.lock();
                err = writeToEntryPoint(request, response, indexEntry, dstBuffer, dstSize);
            on_error:
                if (err)
                {
                    logError("%s", err);
                    response.state = ReadState::Error;
                }
                --request.entrypoint->op_count;
                response.ready_promise.set_value();
                request.entrypoint->cv.notify_all();
                request.entrypoint->lock.unlock();
            }

            std::fstream *Cache::getFileStream(EntryPoint *entrypoint, EntryGroup *group)
            {
                if (!entrypoint->fd.is_open())
                {
                    auto path = this->path(entrypoint, group);
                    logInfo("Loading cache file: %s", path.string().c_str());
                    auto openFlags = std::ios::binary | std::ios::in | std::ios::out | std::ios::app;
                    entrypoint->fd = std::fstream(path, openFlags);
                    if (!entrypoint->fd.is_open())
                    {
                        logError("%s: %s", strerror(errno), path.string().c_str());
                        return nullptr;
                    }
                    if (entrypoint->pos == 0 && !writeHeader(entrypoint))
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