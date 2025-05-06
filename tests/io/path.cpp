#include <acul/io/path.hpp>
#include <cassert>

void test_path_iterators()
{
    using namespace acul::io;

    path p("/usr/local/bin");

    auto it = p.begin();
    assert(it != p.end());
    assert(*it == "usr");

    ++it;
    assert(it != p.end());
    assert(*it == "local");

    ++it;
    assert(it != p.end());
    assert(*it == "bin");

    ++it;
    assert(it == p.end());
}

void test_construct()
{
    using namespace acul::io;

    path empty_path;
    assert(empty_path.empty());

    // UNIX absolute
    path unix_path("/usr/local/bin");
    assert(unix_path.is_absolute());
    assert(unix_path.is_unix_like());
    assert(unix_path.front() == "usr");
    assert(unix_path.back() == "bin");
    assert(unix_path.size() == 3);

    // Windows absolute
    path win_path("C:\\Program Files\\Test");
    assert(win_path.is_absolute());
    assert(!win_path.is_unix_like());
    assert(win_path.front() == "C");
    assert(win_path.back() == "Test");

    // Relative
    path rel_path("folder/subfolder/file.txt");
    assert(!rel_path.is_absolute());
    assert(rel_path.back() == "file.txt");

    // HTTP URL
    path http_path("http://example.com/resources/file.txt");
    assert(http_path.scheme() == "http");
    assert(http_path.front() == "example.com");
    assert(http_path.is_scheme_external());
    assert(http_path.back() == "file.txt");

    // SMB URL
    path smb_path("smb://server/share/folder");
    assert(smb_path.scheme() == "smb");
    assert(smb_path.front() == "server");
    assert(smb_path.back() == "folder");

    // Parent path
    path parent = http_path.parent_path();
    assert(parent.back() == "resources");

    // Op
    path combined = rel_path / path("subfile.txt");
    assert(combined.back() == "subfile.txt");

    // Check
    acul::string rebuilt = unix_path.str();
    assert(rebuilt.find("usr") != acul::string::npos && rebuilt.find("bin") != acul::string::npos);
}

void test_path_comparison()
{
    using namespace acul::io;

    path p1("/usr/local/bin");
    path p2("/usr/local/bin");
    path p3("/usr/local/lib");

    assert(p1 == p2);
    assert(p1 != p3);
}

void test_path_helpers()
{
    using namespace acul::io;
    using namespace acul;

    // get_extension
    string ext = get_extension("file.txt");
    assert(ext == ".txt");

    string no_ext = get_extension("/folder/file");
    assert(no_ext.empty());

    // get_filename
    string fname = get_filename("/folder/file.txt");
    assert(fname == "file.txt");

    string no_folder_fname = get_filename("file.txt");
    assert(no_folder_fname == "file.txt");

    // replace_filename
    string replaced = replace_filename("/folder/file.txt", "newfile.md");
    assert(replaced.find("newfile.md") != string::npos);
    assert(get_filename(replaced) == "newfile.md");

    // replace_filename with Windows separator
    string replaced_win = replace_filename("C:\\folder\\file.txt", "newfile.md");
    assert(replaced_win.find("newfile.md") != string::npos);
    assert(get_filename(replaced_win) == "newfile.md");

    // replace_extension
    string changed_ext = replace_extension("/folder/file.txt", ".md");
    assert(get_extension(changed_ext) == ".md");

    string append_ext = replace_extension("/folder/file", ".md");
    assert(get_extension(append_ext) == ".md");

    // get_current_path
    path cur = get_current_path();
    assert(!cur.empty());
}

void test_path()
{
    test_construct();
    test_path_comparison();
    test_path_iterators();
    test_path_helpers();
}