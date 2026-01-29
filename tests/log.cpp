#include <acul/io/fs/file.hpp>
#include <acul/log.hpp>
#include <acul/task.hpp>
#include <cassert>

void test_log()
{
    using namespace acul;
    using namespace acul::log;

    task::service_dispatch sd;
    sd.run();
    auto *service = acul::alloc<log_service>();
    sd.register_service(service);
    service->level = level::trace;

    auto *console = service->add_logger<console_logger>("console");
    set_default_logger(console);
    console->set_pattern("%(color_auto)[%(level_name)]%(ascii_time)%(thread)%(message)%(color_off)\n");
    assert(console->name() == "console");

    service->log(console, level::debug, "Test debug log: %d", 123);
    service->log(console, level::trace, "Test trace log: %d", 123);
    service->log(console, level::error, "Test error log: %d", 123);
    service->log(console, level::warn, "Test warn log: %d", 123);
    service->log(console, level::info, "Test info log: %d", 123);
    service->log(console, level::fatal, "Test fatal log: %d", 123);

    logger_base *fetched = service->get_logger("console");
    assert(fetched == console);
    service->await(true);
    service->remove_logger("console");
    assert(service->get_logger("console") == nullptr);

    const char *output_dir = getenv("TEST_OUTPUT_DIR");
    assert(output_dir);

    string filepath = string(output_dir) + "/test_log.txt";
    auto *filelog = service->add_logger<file_logger>("file", filepath, std::ios::out);
    assert(filelog->stream().good());
    filelog->set_pattern("%(message)\n");

    set_default_logger(filelog);
    service->log(filelog, level::info, "File log: %d", 456);
    {
        auto next = service->dispatch();
        while (next != std::chrono::steady_clock::time_point::max()) next = service->dispatch();
    }
    service->remove_logger("file");

    {
        vector<char> buffer;
        assert(fs::read_binary(filepath, buffer));

        string content(buffer.data(), buffer.size());
        assert(content.find("File log: 456") != string::npos);
    }

    fs::remove_file(filepath.c_str());
}
