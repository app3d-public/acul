#include <acul/exception.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/map.hpp>
#include <acul/string/utils.hpp>
#include <acul/vector.hpp>
#include <dbghelp.h>
#include <psapi.h>
#ifndef _MSC_VER
    #include <cstdlib>
    #include <cxxabi.h>
#endif

namespace acul
{
    void write_exception_info(_EXCEPTION_RECORD *pRecord, std::ofstream &stream)
    {
        stream << "Exception code: 0x" << std::hex << pRecord->ExceptionCode << std::endl;
        stream << "Exception address: " << std::showbase << reinterpret_cast<uintptr_t>(pRecord->ExceptionAddress)
               << std::endl;

        if (pRecord->NumberParameters > 0)
        {
            stream << "Exception parameters: ";
            for (DWORD i = 0; i < pRecord->NumberParameters; ++i)
                stream << "0x" << std::noshowbase << pRecord->ExceptionInformation[i] << " ";
            stream << std::endl;
        }
    }

    void write_frame_registers(std::ofstream &stream, const CONTEXT &context)
    {
        stream << "Frame registers:\n";
        stream << "\tRAX: 0x" << std::hex << context.Rax << std::endl;
        stream << "\tRBX: 0x" << context.Rbx << std::endl;
        stream << "\tRCX: 0x" << context.Rcx << std::endl;
        stream << "\tRDX: 0x" << context.Rdx << std::endl;
        stream << "\tRSI: 0x" << context.Rsi << std::endl;
        stream << "\tRDI: 0x" << context.Rdi << std::endl;
        stream << "\tRBP: 0x" << context.Rbp << std::endl;
        stream << "\tRSP: 0x" << context.Rsp << std::endl;
        stream << "\tRIP: 0x" << context.Rip << std::dec << std::endl;
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

    hmodule_symbol_map::iterator find_symbol_map(hmodule_symbol_map &hmodule_symbol_map, DWORD64 module_base,
                                                 string &module_name)
    {
        auto it = hmodule_symbol_map.find(module_base);
        if (it == hmodule_symbol_map.end())
        {
            char module_name_tmp[MAX_PATH];
            if (!GetModuleFileNameA((HINSTANCE)module_base, module_name_tmp, MAX_PATH)) return hmodule_symbol_map.end();

            module_name = module_name_tmp;
            std::ifstream fd(module_name.c_str(), std::ios::binary);
            if (!fd)
                return hmodule_symbol_map.end();
            else
            {
                auto fileData =
                    acul::vector<char>((std::istreambuf_iterator<char>(fd)), std::istreambuf_iterator<char>());
                fd.close();
                auto [inserted_it, inserted] =
                    hmodule_symbol_map.emplace(module_base, std::make_pair(module_name, map<DWORD64, symbol_info>()));
                analyze_COFF_symbols(fileData, module_base, inserted_it->second.second);
                return inserted_it;
            }
        }
        return it;
    }

    void write_stack_trace(std::ofstream &stream, const except_info_t &except_info)
    {
        hmodule_symbol_map hmodule_symbol_map;

        HMODULE main_module = GetModuleHandleA(NULL);
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
                auto it = find_symbol_map(hmodule_symbol_map, addr, module_name);
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

            stream << std::endl;
            frameIndex++;
        }
        SymCleanup(except_info.hProcess);
    }

    void create_mini_dump(EXCEPTION_POINTERS *pep, const string &path)
    {
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return;

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = pep;
        mdei.ClientPointers = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal,
                          (pep != nullptr) ? &mdei : NULL, NULL, NULL);

        CloseHandle(hFile);
    }

    void exception::captureStack()
    {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
        SymInitialize(except_info.hProcess, NULL, TRUE);

        STACKFRAME64 stackFrame;
        memset(&stackFrame, 0, sizeof(STACKFRAME64));
        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
        auto &context = except_info.context;
        stackFrame.AddrPC.Offset = context.Rip;
        stackFrame.AddrFrame.Offset = context.Rbp;
        stackFrame.AddrStack.Offset = context.Rsp;
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        vector<except_addr_t> addresses;
        while (StackWalk64(machineType, except_info.hProcess, except_info.hThread, &stackFrame, &context, NULL,
                           SymFunctionTableAccess64, SymGetModuleBase64, NULL))
        {
            DWORD64 base = SymGetModuleBase64(except_info.hProcess, stackFrame.AddrPC.Offset);
            addresses.emplace_back(base, stackFrame.AddrPC.Offset);
        }
        except_info.addresses = addresses.release();
    }
} // namespace acul