#include <acul/exception/exception.hpp>
#include <acul/exception/utils.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/string.hpp>
#include <cassert>
#include "acul/io/file.hpp" // todo

using namespace acul;

void test_runtime_error()
{
    runtime_error err("Runtime error occurred");
    assert(string(err.what()) == "Runtime error occurred");
    assert(err.except_info.addresses_count == 0); // Check PROCESS_UNITTEST
}

void test_bad_alloc()
{
    bad_alloc alloc_ex(512);
    assert(strstr(alloc_ex.what(), "512") != nullptr);
}

void test_bad_cast()
{
    bad_cast cast_ex("Bad cast error");
    assert(string(cast_ex.what()) == "Bad cast error");
}

void test_out_of_range()
{
    out_of_range range_ex(10, 15);
    assert(strstr(range_ex.what(), "10") != nullptr);
    assert(strstr(range_ex.what(), "15") != nullptr);
}

void test_write_exception_info(const runtime_error &err)
{
#ifdef _WIN32
    stringstream stream;
    EXCEPTION_RECORD fake_record = {};
    fake_record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    fake_record.ExceptionAddress = (void *)0xDEADBEEF;
    fake_record.NumberParameters = 2;
    fake_record.ExceptionInformation[0] = 1; // write access
    fake_record.ExceptionInformation[1] = 0x12345678;
    write_exception_info(fake_record, stream);
    assert(!stream.str().empty());

    // Minidump
    vector<char> buffer;
    assert(create_mini_dump(err.except_info.hProcess, err.except_info.hThread, fake_record, err.except_info.context,
                            buffer));
    assert(!buffer.empty());
#else
    stringstream stream;

    siginfo_t fake_info = {};
    fake_info.si_signo = SIGSEGV;
    fake_info.si_code = SEGV_ACCERR;
    fake_info.si_addr = (void *)0x12345678;

    write_exception_info(SIGSEGV, &fake_info, err.except_info.context, stream);
    assert(!stream.str().empty());

    // Minidump
    vector<char> buffer;
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid);
    assert(create_mini_dump(pid, tid, SIGSEGV, err.except_info.context, buffer));

    // tmp
    {
        string filename = "test_minidump.core";
        if (io::file::write_binary(filename, buffer.data(), buffer.size()) != io::file::op_state::Success)
            fprintf(stderr, "Failed to write minidump\n");
        else
            fprintf(stdout, "Minidump written to %s (%zu bytes)\n", filename.c_str(), buffer.size());
    }
    assert(!buffer.empty());
#endif
}

void test_stacktrace()
{
    runtime_error err("Runtime error occurred");

    // Capture
#ifdef _WIN32
    RtlCaptureContext(&err.except_info.context);
#else
    assert(getcontext(&err.except_info.context) == 0);
#endif
    capture_stack_trace(err.except_info);
    assert(err.except_info.addresses_count > 0);

    // Write except info
    test_write_exception_info(err);
#ifdef _WIN32
    {
        stringstream stream;
        write_frame_registers(stream, err.except_info.context);
        assert(!stream.str().empty());
    }
    {
        stringstream stream;
        write_stack_trace(stream, err.except_info);
    }
    destroy_exception_context();
#endif
}

void test_exception()
{
    test_runtime_error();
    test_bad_alloc();
    test_bad_cast();
    test_out_of_range();
    test_stacktrace();
}