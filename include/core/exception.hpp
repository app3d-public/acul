#pragma once

#include <fstream>
#include "api.hpp"
#include "std/basic_types.hpp"
#include "std/vector.hpp"
#ifdef _WIN32
    #include <windows.h>

struct SymbolInfo
{
    std::string name;
    DWORD64 endAddress;
};

APPLIB_API void writeExceptionInfo(EXCEPTION_POINTERS *, std::ofstream &);

void APPLIB_API analyzeCOFFSymbols(const astl::vector<char> &fileData, DWORD64 loadedBaseAddr, std::ofstream &crash_stream,
                        astl::map<DWORD64, SymbolInfo> &symbolMap);

APPLIB_API void writeStackTrace(std::ofstream &stream, CONTEXT *context, const astl::map<u64, SymbolInfo> &symbolTable);

APPLIB_API void createMiniDump(EXCEPTION_POINTERS *pep, const std::string &filename, std::ofstream &crash_stream);

APPLIB_API astl::vector<char> readHModule(std::ofstream &crash_stream, HMODULE hModule);
#else
    #error "Platform not supported"
#endif