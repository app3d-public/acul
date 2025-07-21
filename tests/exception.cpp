#include <acul/exception/exception.hpp>
#include <acul/exception/utils.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/string.hpp>
#include <cassert>
#ifndef _WIN32
    #include <sys/wait.h>
#endif

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

void test_write_exception_info(runtime_error &err)
{
#ifdef _WIN32
    stringstream stream;
    EXCEPTION_RECORD fake_record = {};
    fake_record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    fake_record.ExceptionAddress = (void *)0xDEADBEEF;
    fake_record.NumberParameters = 2;
    fake_record.ExceptionInformation[0] = 1;
    fake_record.ExceptionInformation[1] = 0x12345678;

    write_exception_info(fake_record, stream);
    assert(!stream.str().empty());

    vector<char> buffer;
    assert(create_mini_dump(err.except_info.hProcess, err.except_info.hThread, fake_record, err.except_info.context,
                            buffer));
    assert(!buffer.empty());
#else
    pid_t child = fork();
    assert(child >= 0);

    if (child == 0)
    {
        // потомок: сигнализируем родителю, что готовы
        raise(SIGSTOP);
        pause(); // остаёмся в паузе пока нас не убьют
        _exit(0);
    }
    else
    {
        stringstream stream;

        siginfo_t fake_info = {};
        fake_info.si_signo = SIGSEGV;
        fake_info.si_code = SEGV_ACCERR;
        fake_info.si_addr = (void *)0x12345678;

        write_exception_info(&fake_info, stream);
        assert(!stream.str().empty());

        // SIGSTOP awaiting
        int status = 0;
        assert(waitpid(child, &status, WUNTRACED) == child);
        assert(WIFSTOPPED(status));

        vector<char> buffer;
        bool ok = create_mini_dump(child, child, SIGSEGV, err.except_info.context, buffer);

        assert(ok && "create_mini_dump failed");
        assert(!buffer.empty() && "minidump buffer is empty");

        // Free child
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
    }
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
    {
        stringstream stream;
        write_frame_registers(stream, err.except_info.context);
        assert(!stream.str().empty());
    }
    {
        stringstream stream;
        write_stack_trace(stream, err.except_info);
        assert(!stream.str().empty());
    }
#ifdef _WIN32
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