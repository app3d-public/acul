#include <acul/io/file.hpp>
#include <acul/io/path.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <cassert>
#include <cstdlib>

void test_file()
{
    const char *output_dir = getenv("TEST_OUTPUT_DIR");
    assert(output_dir);

    using namespace acul::io::file;
    using namespace acul;

    io::path data = io::path(output_dir);
    // --- write_binary
    const char *text = "Hello\nWorld\nTest\n";
    auto filename = data / "test_file.txt";
    bool write_ok = write_binary(filename, text, strlen(text));
    assert(write_ok);
    assert(exists(filename.str().c_str()));

    // --- read_binary
    vector<char> buffer;
    op_state read_result = read_binary(filename, buffer);
    assert(read_result == op_state::Success);
    assert(!buffer.empty());
    assert(strncmp(buffer.data(), text, buffer.size()) == 0);

    // --- fill_line_buffer
    string_pool<char> pool(256);
    fill_line_buffer(buffer.data(), buffer.size(), pool);
    assert(pool.size() == 3);

    // --- read_by_block
    string dst;
    auto read_by_block_result = read_by_block(filename, [&dst](char *line, size_t size) { dst = line; });
    assert(read_by_block_result == op_state::Success);
    assert(!dst.empty());

    // --- write_by_block

    string error;
    io::path copy_file = data / "copy_file.txt";
    bool block_written = write_by_block(copy_file, buffer.data(), buffer.size(), error);
    assert(block_written);
    assert(exists(copy_file.str().c_str()));

    // --- copy
    io::path copy_file2 = data / "copy_file2.txt";
    op_state copy_result = copy(copy_file.str().c_str(), copy_file2.str().c_str(), true);
    assert(copy_result == op_state::Success);
    assert(exists(copy_file2.str().c_str()));

    // --- list_files
    vector<string> files;
    op_state list_result = list_files(".", files, false);
    assert(list_result == op_state::Success);
    assert(!files.empty());

    // --- remove_file
    assert(remove_file(copy_file2.str().c_str()) == op_state::Success);
    assert(!exists(copy_file2.str().c_str()));

    // --- compress / decompress
    vector<char> compressed;
    vector<char> decompressed;

    bool compress_ok = compress(buffer.data(), buffer.size(), compressed, 3);
    assert(compress_ok);
    assert(!compressed.empty());

    bool decompress_ok = decompress(compressed.data(), compressed.size(), decompressed);
    assert(decompress_ok);
    assert(decompressed.size() == buffer.size());
    assert(memcmp(buffer.data(), decompressed.data(), buffer.size()) == 0);

    // Create directory
    auto dp = data / "test_dir";
    auto state = create_directory(dp.str().c_str());
    assert(state != op_state::Error);
    assert(exists(dp.str().c_str()));

    // --- clean
    remove_file(filename.str().c_str());
    remove_file(copy_file.str().c_str());
#ifdef _WIN32
    RemoveDirectoryA(dp.str().c_str());
#else
    rmdir(dp.str().c_str());
#endif
}