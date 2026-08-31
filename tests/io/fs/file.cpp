#include <acul/functional/unique_function.hpp>
#include <acul/io/fs/file.hpp>
#include <acul/io/path.hpp>
#include <acul/string/string_view_pool.hpp>
#include <acul/string/utils.hpp>
#include <acul/vector.hpp>
#include <cassert>
#include <cstdlib>

void test_file()
{
    const char *output_dir = getenv("TEST_OUTPUT_DIR");
    assert(output_dir);

    using namespace acul::fs;
    using namespace acul;

    path data = path(output_dir);

    // --- file lock
    auto lock_filename = data / "application.lock";
    assert(!is_file_locked(lock_filename));
    fd_lock lock_id = lock_file(lock_filename);
    assert(lock_id != invalid_fd_lock);
    assert(is_file_locked(lock_filename));
    assert(lock_file(lock_filename) == invalid_fd_lock);
    assert(unlock_file(lock_id));
    assert(!is_file_locked(lock_filename));

    // --- write_binary
    const char *text = "Hello\nWorld\nTest\n";
    auto filename = data / "test_file.txt";
    bool write_ok = write_binary(filename, text, strlen(text));
    assert(write_ok);
    assert(exists(filename.str().c_str()));

    // --- read_binary
    vector<char> buffer;
    assert(read_binary(filename, buffer));
    assert(!buffer.empty());
    assert(strncmp(buffer.data(), text, buffer.size()) == 0);

    // --- persistent read-only mapping and move ownership
    {
        mapped_file source;
        assert(source.open(filename).success());
        assert(source.size() == buffer.size());
        assert(std::memcmp(source.data(), buffer.data(), source.size()) == 0);
        const char *mapped_data = source.data();
        mapped_file moved(std::move(source));
        assert(source.data() == nullptr && source.size() == 0u);
        assert(moved.data() == mapped_data);
        source = std::move(moved);
        assert(moved.data() == nullptr && moved.size() == 0u);
        assert(std::memcmp(source.data(), text, source.size()) == 0);
        source.close();
        source.close();
        assert(source.data() == nullptr && source.size() == 0u);
        assert(!source.open(data / "missing-map-source.bin").success());
        assert(source.data() == nullptr && source.size() == 0u);

        const auto empty_file = data / "empty-map-source.bin";
        assert(write_binary(empty_file, "", 0u));
        assert(source.open(empty_file).success());
        assert(source.data() == nullptr && source.size() == 0u);
        source.close();
        assert(remove_file(empty_file.str().c_str()).success());
    }

    // --- fill_line_buffer
    string_view_pool<char> pool;
    pool.reserve(256);
    fill_line_buffer(buffer.data(), buffer.size(), pool);
    assert(pool.size() == 3);

    // --- read_by_block
    string dst;
    auto read_by_block_result = read_by_block(filename, [&dst](char *line, size_t size) { dst = string(line, size); });
    assert(read_by_block_result.success());
    assert(!dst.empty());

    // --- write_by_block

    path copy_file = data / "copy_file.txt";
    auto block_written = write_by_block(copy_file, buffer.data(), buffer.size());
    assert(block_written.success());
    assert(exists(copy_file.str().c_str()));

    // --- copy
    path copy_file2 = data / "copy_file2.txt";
    op_result copy_result = fs::copy_file(copy_file.str().c_str(), copy_file2.str().c_str(), true);
    assert(copy_result.success());
    assert(exists(copy_file2.str().c_str()));

    // --- list_files
    vector<string> files;
    op_result list_result = list_files(".", files, false);
    assert(list_result.success());
    assert(!files.empty());

    // --- remove_file
    assert(remove_file(copy_file2.str().c_str()).success());
    assert(!exists(copy_file2.str().c_str()));

#ifdef ACUL_ZSTD_ENABLE

    // --- compress / decompress
    vector<char> compressed;
    vector<char> decompressed;

    auto compress_ok = compress(buffer.data(), buffer.size(), compressed, 3);
    assert(compress_ok.success());
    assert(!compressed.empty());

    auto decompress_ok = decompress(compressed.data(), compressed.size(), decompressed);
    assert(decompress_ok.success());
    assert(decompressed.size() == buffer.size());
    assert(memcmp(buffer.data(), decompressed.data(), buffer.size()) == 0);
#endif

    // Create directory
    auto dp = data / "test_dir";
    auto state = create_directory(dp.str().c_str());
    assert(state.success());
    assert(exists(dp.str().c_str()));

    // --- clean
    remove_file(filename.str().c_str());
    remove_file(copy_file.str().c_str());
    remove_file(lock_filename.str().c_str());
    assert(remove_directory(dp.str().c_str()).success());
    assert(!exists(dp.str().c_str()));
}
