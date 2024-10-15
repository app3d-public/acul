#pragma once

#include <fstream>
#include "api.hpp"
#ifdef _WIN32
    #include <windows.h>

APPLIB_API void writeExceptionInfo(EXCEPTION_POINTERS *, std::ofstream &);

APPLIB_API void writeStackTrace(std::ofstream &stream, CONTEXT *context);

APPLIB_API void createMiniDump(EXCEPTION_POINTERS *pep, const std::string &filename, std::ofstream &crash_stream);

#else
    #error "Platform not supported"
#endif