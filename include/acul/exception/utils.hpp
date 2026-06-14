#pragma once

#include "../fwd/sstream.hpp"
#include "../vector.hpp"
#include "exception.hpp"
#ifndef _WIN32
    #include <signal.h>
#endif

namespace acul
{
    ACUL_EXPORT void write_stack_trace(stringstream &stream, const except_info &except_info);
#ifdef _WIN32
    ACUL_EXPORT bool init_stack_capture(HANDLE hProcess);
    ACUL_EXPORT void write_frame_registers(stringstream &stream, const CONTEXT &context);
    ACUL_EXPORT void write_exception_info(EXCEPTION_RECORD record, stringstream &stream);
    ACUL_EXPORT bool create_mini_dump(HANDLE hProcess, HANDLE hThread, EXCEPTION_RECORD &exceptionRecord,
                                     CONTEXT &context, vector<char> &buffer);
#else
    ACUL_EXPORT void write_frame_registers(stringstream &stream, const ucontext_t &context);
    ACUL_EXPORT void write_exception_info(siginfo_t *info, stringstream &stream);
    ACUL_EXPORT bool create_mini_dump(pid_t pid, pid_t tid, int signal, const ucontext_t &context, vector<char> &buffer);
#endif

#ifndef _MSC_VER
    string demangle(const char *mangled_name);
#endif
} // namespace acul