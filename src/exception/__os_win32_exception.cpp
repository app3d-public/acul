#include <acul/exception/exception.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/map.hpp>
#include <acul/string/sstream.hpp>
#include <acul/string/utils.hpp>
#include <acul/vector.hpp>
#include <algorithm>
#include <dbghelp.h>
#include <fstream>
#include <psapi.h>
#include <winternl.h>
#ifndef _MSC_VER
    #include <acul/exception/utils.hpp>
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
        if (!hProcess || hProcess == exception_hprocess) exception_hprocess = nullptr;
    }

    APPLIB_API void write_exception_info(EXCEPTION_RECORD record, acul::stringstream &stream)
    {
        stream << format("Exception code: 0x%llx\n", record.ExceptionCode);
        stream << format("Exception address: 0x%llx\n", record.ExceptionAddress);

        if (record.NumberParameters > 0)
        {
            stream << "Exception parameters: ";
            for (DWORD i = 0; i < record.NumberParameters; ++i)
                stream << format("0x%llx ", record.ExceptionInformation[i]);
            stream << '\n';
        }
    }

    APPLIB_API void write_frame_registers(stringstream &stream, const CONTEXT &context)
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
            char short_name[8];
            struct
            {
                DWORD zeroes;
                DWORD offset;
            } Name;
        };
        DWORD value;
        SHORT section_number;
        WORD type;
        BYTE storage_class;
        BYTE number_of_aux_symbols;
    };
#pragma pack(pop)

#ifdef _MSC_VER
    string demangle(const char *mangled_name)
    {
        char undecorated_name[1024];
        if (UnDecorateSymbolName(mangled_name, undecorated_name, sizeof(undecorated_name),
                                 UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_ALLOCATION_MODEL |
                                     UNDNAME_NO_ALLOCATION_LANGUAGE | UNDNAME_NO_MS_THISTYPE | UNDNAME_NO_SPECIAL_SYMS))
            return string(undecorated_name);
        return string(mangled_name);
    }
#endif

    string get_symbol_name(const COFFSymbol &symbol, const char *string_table)
    {
        if (symbol.Name.zeroes == 0)
        {
            DWORD offset = symbol.Name.offset;
            return string_table + offset;
        }
        else return string(symbol.short_name, strnlen(symbol.short_name, 8));
    }

    string find_symbol_from_table(DWORD64 address, const map<DWORD64, symbol_info> &symbol_map)
    {
        auto it = symbol_map.upper_bound(address);
        if (it != symbol_map.begin())
        {
            --it;
            if (address >= it->first && address < it->second.end_address) return it->second.name;
        }
        return "<unknown>";
    }

    struct __symbol_temp_info
    {
        DWORD64 start_address;
        DWORD64 end_address;
        string name;
    };

    void analyze_coff_symbols(const vector<char> &file_data, DWORD64 loaded_base_addr,
                              map<DWORD64, symbol_info> &symbol_map)
    {
        PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)file_data.data();
        if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return;

        PIMAGE_NT_HEADERS64 nt_headers = (PIMAGE_NT_HEADERS64)(file_data.data() + dos_header->e_lfanew);
        if (nt_headers->Signature != IMAGE_NT_SIGNATURE) return;

        DWORD symbol_table_offset = nt_headers->FileHeader.PointerToSymbolTable;
        DWORD symbols_num = nt_headers->FileHeader.NumberOfSymbols;

        if (symbol_table_offset == 0 || symbols_num == 0) return;

        DWORD64 image_base = nt_headers->OptionalHeader.ImageBase;
        if (image_base == 0) return;

        DWORD64 base_address_offset = loaded_base_addr - image_base;

        const char *symbol_table = file_data.data() + symbol_table_offset;
        const char *string_table = symbol_table + symbols_num * sizeof(COFFSymbol);

        PIMAGE_SECTION_HEADER section_headers =
            (PIMAGE_SECTION_HEADER)(file_data.data() + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS64));

        vector<__symbol_temp_info> symbol_list_tmp;
        vector<IMAGE_SECTION_HEADER> sorted_sections(section_headers,
                                                     section_headers + nt_headers->FileHeader.NumberOfSections);

        std::sort(sorted_sections.begin(), sorted_sections.end(),
                  [](const IMAGE_SECTION_HEADER &a, const IMAGE_SECTION_HEADER &b) {
                      return a.VirtualAddress < b.VirtualAddress;
                  });

        for (DWORD i = 0; i < symbols_num; ++i)
        {
            COFFSymbol *symbol = (COFFSymbol *)(symbol_table + i * sizeof(COFFSymbol));

            DWORD section_number = symbol->section_number;
            if (section_number > 0 && section_number <= nt_headers->FileHeader.NumberOfSections)
            {
                PIMAGE_SECTION_HEADER symbol_section = &section_headers[section_number - 1];
                DWORD64 section_base = symbol_section->VirtualAddress + image_base;
                DWORD64 start_address = symbol->value + section_base + base_address_offset;
                symbol_list_tmp.push_back({start_address, 0, get_symbol_name(*symbol, string_table)});
            }
            i += symbol->number_of_aux_symbols;
        }

        std::sort(
            symbol_list_tmp.begin(), symbol_list_tmp.end(),
            [](const __symbol_temp_info &a, const __symbol_temp_info &b) { return a.start_address < b.start_address; });

        for (size_t i = 0; i < symbol_list_tmp.size(); ++i)
        {
            if (i + 1 < symbol_list_tmp.size()) symbol_list_tmp[i].end_address = symbol_list_tmp[i + 1].start_address;
            else
            {
                auto section =
                    std::lower_bound(sorted_sections.begin(), sorted_sections.end(), symbol_list_tmp[i].start_address,
                                     [](const IMAGE_SECTION_HEADER &section, DWORD64 addr) {
                                         return section.VirtualAddress + section.Misc.VirtualSize < addr;
                                     });

                if (section != sorted_sections.end())
                    symbol_list_tmp[i].end_address =
                        section->VirtualAddress + section->Misc.VirtualSize + base_address_offset;
            }

            symbol_map[symbol_list_tmp[i].start_address] = {symbol_list_tmp[i].name, symbol_list_tmp[i].end_address};
        }
    }

    using hmodule_symbol_map = hashmap<DWORD64, pair<string, map<DWORD64, symbol_info>>>;

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
            if (!fd) return hmodule_symbol_map.end();
            else
            {
                auto file_data = vector<char>((std::istreambuf_iterator<char>(fd)), std::istreambuf_iterator<char>());
                fd.close();
                auto [inserted_it, inserted] =
                    hmodule_symbol_map.emplace(module_base, pair{module_name, map<DWORD64, symbol_info>()});
                analyze_coff_symbols(file_data, module_base, inserted_it->second.second);
                return inserted_it;
            }
        }
        else module_name = it->second.first;
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
            PVOID image_baseAddress = nullptr;
            SIZE_T bytes_read;
            if (ReadProcessMemory(hProcess, &(pbi.PebBaseAddress->Reserved3[1]), &image_baseAddress, sizeof(PVOID),
                                  &bytes_read))
                hModule = (HMODULE)image_baseAddress;
        }
        return hModule;
    }

    APPLIB_API void write_stack_trace(stringstream &stream, const except_info &except_info)
    {
        hmodule_symbol_map hmodule_symbol_map;

        HMODULE main_module = GetMainModuleHandle(except_info.hProcess);
        stream << "Stack trace:\n";
        int frame_index = 0;

        for (int i = 0; i < except_info.addresses_count; ++i)
        {
            auto &[addr, offset] = except_info.addresses[i];
            string line = format("\t#%d 0x%llx", frame_index, offset);
            stream << line.c_str();
            if (addr)
            {
                string name, module_name;
                auto it = find_symbol_map(hmodule_symbol_map, except_info.hProcess, addr, module_name);
                if (it == hmodule_symbol_map.end()) name = "<unknown>";
                else
                {
                    const map<DWORD64, symbol_info> &symbol_map = it->second.second;
                    if (symbol_map.empty()) // Try get symbol info from .edata
                    {
                        DWORD64 displacement_sym = 0;
                        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
                        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                        pSymbol->MaxNameLen = MAX_SYM_NAME;
                        if (SymFromAddr(except_info.hProcess, offset, &displacement_sym, pSymbol))
                            name = (const char *)pSymbol->Name;
                        else name = "<unknown>";
                    }
                    else name = find_symbol_from_table(offset, symbol_map);
                }
                stream << " in " << demangle(name.c_str()).c_str();
                if (addr != (DWORD64)main_module && !module_name.empty()) stream << " at " << module_name.c_str();
            }
            else stream << " in <unknown>";

            stream << '\n';
            frame_index++;
        }
    }

    APPLIB_API bool create_mini_dump(HANDLE hProcess, HANDLE hThread, EXCEPTION_RECORD &exception_record,
                                     CONTEXT &context, vector<char> &buffer)
    {
        HANDLE hFile = CreateFileA(".memory.dmp", GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

        if (hFile == INVALID_HANDLE_VALUE) return false;

        EXCEPTION_POINTERS exception_pointers;
        exception_pointers.ExceptionRecord = &exception_record;
        exception_pointers.ContextRecord = &context;

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetThreadId(hThread);
        mdei.ExceptionPointers = &exception_pointers;
        mdei.ClientPointers = FALSE;

        BOOL success =
            MiniDumpWriteDump(hProcess, GetProcessId(hProcess), hFile, MiniDumpWithFullMemory, &mdei, nullptr, nullptr);

        if (!success)
        {
            CloseHandle(hFile);
            return false;
        }

        SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);

        char temp_buffer[4096];
        DWORD bytes_read = 0;
        buffer.clear();

        while (ReadFile(hFile, temp_buffer, sizeof(temp_buffer), &bytes_read, nullptr) && bytes_read > 0)
            buffer.insert(buffer.end(), temp_buffer, temp_buffer + bytes_read);

        CloseHandle(hFile);
        return true;
    }

    void capture_stack_trace(except_info &except_info)
    {
        if (!except_info.hProcess) return;
        STACKFRAME64 stackframe = {};
        CONTEXT context = except_info.context;

        DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;

        stackframe.AddrPC.Offset = context.Rip;
        stackframe.AddrFrame.Offset = context.Rbp;
        stackframe.AddrStack.Offset = context.Rsp;
        stackframe.AddrPC.Mode = AddrModeFlat;
        stackframe.AddrFrame.Mode = AddrModeFlat;
        stackframe.AddrStack.Mode = AddrModeFlat;

        vector<except_addr> addresses;
        while (StackWalk64(machine_type, except_info.hProcess, except_info.hThread, &stackframe, &context, nullptr,
                           SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
        {
            DWORD64 base = SymGetModuleBase64(except_info.hProcess, stackframe.AddrPC.Offset);
            addresses.emplace_back(base, stackframe.AddrPC.Offset);
        }
        except_info.addresses_count = addresses.size();
        except_info.addresses = addresses.release();
    }
} // namespace acul
