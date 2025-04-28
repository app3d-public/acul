#include <acul/exception/exception.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/map.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/utils.hpp>
#include <acul/vector.hpp>
#include <dbghelp.h>
#include <fstream>
#include <psapi.h>
#include <winternl.h>

#ifndef _MSC_VER
    #include <cstdlib>
    #include <cxxabi.h>
#endif

#define SYM_INIT_ATTEMP_COUNT 3
#define SYM_INIT_NONE         0x0
#define SYM_INIT_SUCCESS      0x1
#define SYM_INIT_FAIL         0x2

namespace acul
{
    HANDLE exception_hprocess = nullptr;

    bool init_stack_capture(HANDLE hProcess)
    {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
        DWORD err = 0;
        // A few attempts if DLL loader is busy by Windows app initialization
        for (int i = 0; i < SYM_INIT_ATTEMP_COUNT; ++i)
        {
            if (SymInitialize(hProcess, nullptr, TRUE))
            {
                err = 0;
                break;
            }
            err = GetLastError();
            if (err != 0xC0000004) break;
        }
        return err == 0;
    }

    HANDLE get_exception_process()
    {
        static int sym_state = SYM_INIT_NONE;
        if (sym_state != SYM_INIT_NONE) return exception_hprocess;
        HANDLE hProcess = GetCurrentProcess();
        if (init_stack_capture(hProcess))
        {
            sym_state = SYM_INIT_SUCCESS;
            exception_hprocess = hProcess;
            return hProcess;
        }
        sym_state = SYM_INIT_FAIL;
        return nullptr;
    }

    void destroy_exception_context(HANDLE hProcess)
    {
        HANDLE current_process = hProcess ? hProcess : exception_hprocess;
        if (!current_process) return;
        SymCleanup(current_process);
    }

    APPLIB_API void write_exception_info(EXCEPTION_RECORD record, acul::stringstream &stream)
    {
        stream << format("Excetion code: 0x%llx\n", record.ExceptionCode);
        stream << format("Exception address: 0x%llx\n", record.ExceptionAddress);

        if (record.NumberParameters > 0)
        {
            stream << "Exception parameters: ";
            for (DWORD i = 0; i < record.NumberParameters; ++i)
                stream << format("0x%llx ", record.ExceptionInformation[i]);
            stream << '\n';
        }
    }

    APPLIB_API void write_frame_registers(acul::stringstream &stream, const CONTEXT &context)
    {
        stream << "Frame registers:\n";
        stream << format("\tRAX: 0x%llx\n", context.Rax);
        stream << format("\tRBX: 0x%llx\n", context.Rbx);
        stream << format("\tRCX: 0x%llx\n", context.Rcx);
        stream << format("\tRDX: 0x%llx\n", context.Rdx);
        stream << format("\tRSI: 0x%llx\n", context.Rsi);
        stream << format("\tRDI: 0x%llx\n", context.Rdi);
        stream << format("\tRBP: 0x%llx\n", context.Rbp);
        stream << format("\tRSP: 0x%llx\n", context.Rsp);
        stream << format("\tRIP: 0x%llx\n", context.Rip);
    }

    struct symbol_info
    {
        string name;
        DWORD64 end_address;
    };

    // Required dummy line in according with Clangd bug.
    // See https://github.com/llvm/llvm-project/issues/51164
    static_assert(true);
// COFF Symbol structure
#pragma pack(push, 1)
    struct COFFSymbol
    {
        union
        {
            char ShortName[8];
            struct
            {
                DWORD Zeroes;
                DWORD Offset;
            } Name;
        };
        DWORD Value;
        SHORT SectionNumber;
        WORD Type;
        BYTE StorageClass;
        BYTE NumberOfAuxSymbols;
    };
#pragma pack(pop)

#ifdef _MSC_VER
    string demangle(const char *mangledName)
    {
        char undecorated_name[1024];
        if (UnDecorateSymbolName(mangledName, undecorated_name, sizeof(undecorated_name),
                                 UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_ALLOCATION_MODEL |
                                     UNDNAME_NO_ALLOCATION_LANGUAGE | UNDNAME_NO_MS_THISTYPE | UNDNAME_NO_SPECIAL_SYMS))
            return string(undecorated_name);
        return string(mangledName);
    }
#else

    string demangle(const char *mangledName)
    {
        int status = 0;
        char *demangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
        if (status == 0 && demangled)
        {
            string result(demangled);
            free(demangled);
            return result;
        }
        return mangledName;
    }
#endif

    string get_symbol_name(const COFFSymbol &symbol, const char *string_table)
    {
        if (symbol.Name.Zeroes == 0)
        {
            DWORD offset = symbol.Name.Offset;
            return string_table + offset;
        }
        else
            return string(symbol.ShortName, strnlen(symbol.ShortName, 8));
    }

    string find_symbol_from_table(DWORD64 address, const map<DWORD64, symbol_info> &symbolMap)
    {
        auto it = symbolMap.upper_bound(address);
        if (it != symbolMap.begin())
        {
            --it;
            if (address >= it->first && address < it->second.end_address) return it->second.name;
        }
        return "<unknown>";
    }

    struct __symbol_temp_info
    {
        DWORD64 startAddress;
        DWORD64 endAddress;
        string name;
    };

    void analyze_COFF_symbols(const vector<char> &fileData, DWORD64 loadedBaseAddr,
                              map<DWORD64, symbol_info> &symbolMap)
    {
        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)fileData.data();
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;

        PIMAGE_NT_HEADERS64 ntHeaders = (PIMAGE_NT_HEADERS64)(fileData.data() + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return;

        DWORD symbolTableOffset = ntHeaders->FileHeader.PointerToSymbolTable;
        DWORD numberOfSymbols = ntHeaders->FileHeader.NumberOfSymbols;

        if (symbolTableOffset == 0 || numberOfSymbols == 0) return;

        DWORD64 imageBase = ntHeaders->OptionalHeader.ImageBase;
        if (imageBase == 0) return;

        DWORD64 baseAddressOffset = loadedBaseAddr - imageBase;

        const char *symbolTable = fileData.data() + symbolTableOffset;
        const char *stringTable = symbolTable + numberOfSymbols * sizeof(COFFSymbol);

        PIMAGE_SECTION_HEADER sectionHeaders =
            (PIMAGE_SECTION_HEADER)(fileData.data() + dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS64));

        vector<__symbol_temp_info> tempSymbolList;
        vector<IMAGE_SECTION_HEADER> sortedSections(sectionHeaders,
                                                    sectionHeaders + ntHeaders->FileHeader.NumberOfSections);

        std::sort(sortedSections.begin(), sortedSections.end(),
                  [](const IMAGE_SECTION_HEADER &a, const IMAGE_SECTION_HEADER &b) {
                      return a.VirtualAddress < b.VirtualAddress;
                  });

        for (DWORD i = 0; i < numberOfSymbols; ++i)
        {
            COFFSymbol *symbol = (COFFSymbol *)(symbolTable + i * sizeof(COFFSymbol));

            DWORD sectionNumber = symbol->SectionNumber;
            if (sectionNumber > 0 && sectionNumber <= ntHeaders->FileHeader.NumberOfSections)
            {
                PIMAGE_SECTION_HEADER symbolSection = &sectionHeaders[sectionNumber - 1];
                DWORD64 sectionBase = symbolSection->VirtualAddress + imageBase;
                DWORD64 startAddress = symbol->Value + sectionBase + baseAddressOffset;
                tempSymbolList.push_back({startAddress, 0, get_symbol_name(*symbol, stringTable)});
            }
            i += symbol->NumberOfAuxSymbols;
        }

        std::sort(
            tempSymbolList.begin(), tempSymbolList.end(),
            [](const __symbol_temp_info &a, const __symbol_temp_info &b) { return a.startAddress < b.startAddress; });

        for (size_t i = 0; i < tempSymbolList.size(); ++i)
        {
            if (i + 1 < tempSymbolList.size())
                tempSymbolList[i].endAddress = tempSymbolList[i + 1].startAddress;
            else
            {
                auto section =
                    std::lower_bound(sortedSections.begin(), sortedSections.end(), tempSymbolList[i].startAddress,
                                     [](const IMAGE_SECTION_HEADER &section, DWORD64 addr) {
                                         return section.VirtualAddress + section.Misc.VirtualSize < addr;
                                     });

                if (section != sortedSections.end())
                    tempSymbolList[i].endAddress =
                        section->VirtualAddress + section->Misc.VirtualSize + baseAddressOffset;
            }

            symbolMap[tempSymbolList[i].startAddress] = {tempSymbolList[i].name, tempSymbolList[i].endAddress};
        }
    }

    using hmodule_symbol_map = hashmap<DWORD64, std::pair<string, map<DWORD64, symbol_info>>>;

    hmodule_symbol_map::iterator find_symbol_map(hmodule_symbol_map &hmodule_symbol_map, HANDLE hProcess,
                                                 DWORD64 module_base, string &module_name)
    {
        auto it = hmodule_symbol_map.find(module_base);
        if (it == hmodule_symbol_map.end())
        {
            {
                char module_name_tmp[MAX_PATH];
                if (!GetModuleFileNameExA(hProcess, (HMODULE)module_base, module_name_tmp, MAX_PATH))
                    return hmodule_symbol_map.end();
                module_name = (const char *)module_name_tmp;
            }
            std::ifstream fd(module_name.c_str(), std::ios::binary);
            if (!fd)
                return hmodule_symbol_map.end();
            else
            {
                auto fileData =
                    vector<char>((std::istreambuf_iterator<char>(fd)), std::istreambuf_iterator<char>());
                fd.close();
                auto [inserted_it, inserted] =
                    hmodule_symbol_map.emplace(module_base, std::make_pair(module_name, map<DWORD64, symbol_info>()));
                analyze_COFF_symbols(fileData, module_base, inserted_it->second.second);
                return inserted_it;
            }
        }
        else
            module_name = it->second.first;
        return it;
    }

    typedef NTSTATUS(NTAPI *NtQueryInformationProcess_t)(HANDLE, PROCESS_INFORMATION_CLASS, PVOID, ULONG, PULONG);

    HMODULE GetMainModuleHandle(HANDLE hProcess)
    {
        if (hProcess == GetCurrentProcess()) return (HMODULE)GetModuleHandle(NULL);
        HMODULE hModule = NULL;
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");

        if (!ntdll) return NULL;

        NtQueryInformationProcess_t NtQueryInformationProcess =
            (NtQueryInformationProcess_t)GetProcAddress(ntdll, "NtQueryInformationProcess");

        if (!NtQueryInformationProcess) return NULL;

        PROCESS_BASIC_INFORMATION pbi;
        ULONG returnLength;

        if (NT_SUCCESS(NtQueryInformationProcess(hProcess, static_cast<PROCESS_INFORMATION_CLASS>(0), &pbi, sizeof(pbi),
                                                 &returnLength)))
        {
            PVOID imageBaseAddress = nullptr;
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, &(pbi.PebBaseAddress->Reserved3[1]), &imageBaseAddress, sizeof(PVOID),
                                  &bytesRead))
                hModule = (HMODULE)imageBaseAddress;
        }
        return hModule;
    }

    APPLIB_API void write_stack_trace(acul::stringstream &stream, const except_info &except_info)
    {
        hmodule_symbol_map hmodule_symbol_map;

        HMODULE main_module = GetMainModuleHandle(except_info.hProcess);
        stream << "Stack trace:\n";
        int frameIndex = 0;

        for (int i = 0; i < except_info.addresses_count; ++i)
        {
            auto &[addr, offset] = except_info.addresses[i];
            string line = format("\t#%d 0x%llx", frameIndex, offset);
            stream << line.c_str();
            if (addr)
            {
                string name, module_name;
                auto it = find_symbol_map(hmodule_symbol_map, except_info.hProcess, addr, module_name);
                if (it == hmodule_symbol_map.end())
                    name = "<unknown>";
                else
                {
                    const map<DWORD64, symbol_info> &symbolMap = it->second.second;
                    if (symbolMap.empty()) // Try get symbol info from .edata
                    {
                        DWORD64 displacementSym = 0;
                        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
                        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                        pSymbol->MaxNameLen = MAX_SYM_NAME;
                        if (SymFromAddr(except_info.hProcess, offset, &displacementSym, pSymbol))
                            name = pSymbol->Name;
                        else
                            name = "<unknown>";
                    }
                    else
                        name = find_symbol_from_table(offset, symbolMap);
                }
                stream << " in " << demangle(name.c_str()).c_str();
                if (addr != (DWORD64)main_module && !module_name.empty()) stream << " at " << module_name.c_str();
            }
            else
                stream << " in <unknown>";

            stream << '\n';
            frameIndex++;
        }
    }

    APPLIB_API bool create_mini_dump(HANDLE hProcess, HANDLE hThread, EXCEPTION_RECORD &exceptionRecord,
                                     CONTEXT &context, vector<char> &buffer)
    {
        HANDLE hFile = CreateFileA(".memory.dmp", GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

        if (hFile == INVALID_HANDLE_VALUE) return false;

        EXCEPTION_POINTERS exceptionPointers;
        exceptionPointers.ExceptionRecord = &exceptionRecord;
        exceptionPointers.ContextRecord = &context;

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetThreadId(hThread);
        mdei.ExceptionPointers = &exceptionPointers;
        mdei.ClientPointers = FALSE;

        BOOL success =
            MiniDumpWriteDump(hProcess, GetProcessId(hProcess), hFile, MiniDumpWithFullMemory, &mdei, nullptr, nullptr);

        if (!success)
        {
            CloseHandle(hFile);
            return false;
        }

        SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);

        char tempBuffer[4096];
        DWORD bytesRead = 0;
        buffer.clear();

        while (ReadFile(hFile, tempBuffer, sizeof(tempBuffer), &bytesRead, nullptr) && bytesRead > 0)
            buffer.insert(buffer.end(), tempBuffer, tempBuffer + bytesRead);

        CloseHandle(hFile);
        return true;
    }

    void capture_stack_trace(except_info &except_info)
    {
        if (!except_info.hProcess) return;
        STACKFRAME64 stackFrame = {};
        CONTEXT context = except_info.context;

        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

        stackFrame.AddrPC.Offset = context.Rip;
        stackFrame.AddrFrame.Offset = context.Rbp;
        stackFrame.AddrStack.Offset = context.Rsp;
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;

        vector<except_addr> addresses;
        while (StackWalk64(machineType, except_info.hProcess, except_info.hThread, &stackFrame, &context, nullptr,
                           SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
        {
            DWORD64 base = SymGetModuleBase64(except_info.hProcess, stackFrame.AddrPC.Offset);
            addresses.emplace_back(base, stackFrame.AddrPC.Offset);
        }
        except_info.addresses_count = addresses.size();
        except_info.addresses = addresses.release();
    }
} // namespace acul