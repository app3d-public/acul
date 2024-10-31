#include <astl/basic_types.hpp>
#include <astl/string.hpp>
#include <astl/vector.hpp>
#include <core/exception.hpp>
#include <dbghelp.h>
#include <psapi.h>

void writeExceptionInfo(EXCEPTION_POINTERS *pExceptionInfo, std::ofstream &stream)
{
    stream << "Exception code: 0x" << std::hex << pExceptionInfo->ExceptionRecord->ExceptionCode << std::endl;
    stream << "Exception address: " << std::showbase
           << reinterpret_cast<uintptr_t>(pExceptionInfo->ExceptionRecord->ExceptionAddress) << std::endl;

    if (pExceptionInfo->ExceptionRecord->NumberParameters > 0)
    {
        stream << "Exception parameters: ";
        for (DWORD i = 0; i < pExceptionInfo->ExceptionRecord->NumberParameters; ++i)
            stream << "0x" << std::noshowbase << pExceptionInfo->ExceptionRecord->ExceptionInformation[i] << " ";
        stream << std::endl;
    }

    CONTEXT *context = pExceptionInfo->ContextRecord;
    stream << "Registers:\n";
    stream << "\tRAX: 0x" << std::hex << context->Rax << std::endl;
    stream << "\tRBX: 0x" << context->Rbx << std::endl;
    stream << "\tRCX: 0x" << context->Rcx << std::endl;
    stream << "\tRDX: 0x" << context->Rdx << std::endl;
    stream << "\tRSI: 0x" << context->Rsi << std::endl;
    stream << "\tRDI: 0x" << context->Rdi << std::endl;
    stream << "\tRBP: 0x" << context->Rbp << std::endl;
    stream << "\tRSP: 0x" << context->Rsp << std::endl;
    stream << "\tRIP: 0x" << context->Rip << std::dec << std::endl;
}

struct SymbolInfo
{
    std::string name;
    DWORD64 endAddress;
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
std::string demangle(const char *mangledName)
{
    char undecorated_name[1024];
    if (UnDecorateSymbolName(mangledName, undecorated_name, sizeof(undecorated_name),
                             UNDNAME_COMPLETE | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_ALLOCATION_MODEL |
                                 UNDNAME_NO_ALLOCATION_LANGUAGE | UNDNAME_NO_MS_THISTYPE | UNDNAME_NO_SPECIAL_SYMS))
        return std::string(undecorated_name);
    return std::string(mangledName);
}
#else
    #include <cstdlib>
    #include <cxxabi.h>

std::string demangle(const char *mangledName)
{
    int status = 0;
    char *demangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, &status);
    if (status == 0 && demangled)
    {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    return std::string(mangledName);
}
#endif

std::string getSymbolName(const COFFSymbol &symbol, const char *stringTable)
{
    if (symbol.Name.Zeroes == 0)
    {
        DWORD offset = symbol.Name.Offset;
        return std::string(stringTable + offset);
    }
    else
        return std::string(symbol.ShortName, strnlen(symbol.ShortName, 8));
}

std::string findSymbolFromTable(DWORD64 address, const astl::map<DWORD64, SymbolInfo> &symbolMap)
{
    auto it = symbolMap.upper_bound(address);
    if (it != symbolMap.begin())
    {
        --it;
        if (address >= it->first && address < it->second.endAddress) return it->second.name;
    }
    return "<unknown>";
}

struct __SymbolTempInfo
{
    DWORD64 startAddress;
    DWORD64 endAddress;
    std::string name;
};

void analyzeCOFFSymbols(const astl::vector<char> &fileData, DWORD64 loadedBaseAddr,
                        astl::map<DWORD64, SymbolInfo> &symbolMap)
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

    astl::vector<__SymbolTempInfo> tempSymbolList;
    astl::vector<IMAGE_SECTION_HEADER> sortedSections(sectionHeaders,
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
            tempSymbolList.push_back({startAddress, 0, getSymbolName(*symbol, stringTable)});
        }
        i += symbol->NumberOfAuxSymbols;
    }

    std::sort(tempSymbolList.begin(), tempSymbolList.end(),
              [](const __SymbolTempInfo &a, const __SymbolTempInfo &b) { return a.startAddress < b.startAddress; });

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
                tempSymbolList[i].endAddress = section->VirtualAddress + section->Misc.VirtualSize + baseAddressOffset;
        }

        symbolMap[tempSymbolList[i].startAddress] = {tempSymbolList[i].name, tempSymbolList[i].endAddress};
    }
}

using hmodule_symbol_map_t = astl::hashmap<DWORD64, std::pair<std::string, astl::map<DWORD64, SymbolInfo>>>;

hmodule_symbol_map_t::iterator findSymbolMap(hmodule_symbol_map_t &hmodule_symbol_map, DWORD64 moduleBase,
                                             std::string &moduleName)
{
    auto it = hmodule_symbol_map.find(moduleBase);
    if (it == hmodule_symbol_map.end())
    {
        char moduleNameTmp[MAX_PATH];
        if (!GetModuleFileNameA((HINSTANCE)moduleBase, moduleNameTmp, MAX_PATH)) return hmodule_symbol_map.end();

        moduleName = std::string(moduleNameTmp);
        std::ifstream fd(moduleName, std::ios::binary);
        if (!fd)
            return hmodule_symbol_map.end();
        else
        {
            auto fileData = astl::vector<char>((std::istreambuf_iterator<char>(fd)), std::istreambuf_iterator<char>());
            fd.close();
            auto [inserted_it, inserted] =
                hmodule_symbol_map.emplace(moduleBase, std::make_pair(moduleName, astl::map<DWORD64, SymbolInfo>()));
            analyzeCOFFSymbols(fileData, moduleBase, inserted_it->second.second); // заполняем символы
            return inserted_it;
        }
    }
    return it;
}

void writeStackTrace(std::ofstream &stream, CONTEXT *context)
{
    hmodule_symbol_map_t hmodule_symbol_map;

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();
    HMODULE mainModule = GetModuleHandleA(NULL);

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(hProcess, NULL, TRUE);

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrStack.Offset = context->Rsp;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    stream << "Stack trace:\n";
    int frameIndex = 0;

    while (StackWalk64(machineType, hProcess, hThread, &stackFrame, context, NULL, SymFunctionTableAccess64,
                       SymGetModuleBase64, NULL))
    {
        stream << astl::format("\t#%d 0x%llx", frameIndex, stackFrame.AddrPC.Offset);
        DWORD64 moduleBase = SymGetModuleBase64(hProcess, stackFrame.AddrPC.Offset);

        if (moduleBase)
        {
            std::string name, moduleName;
            auto it = findSymbolMap(hmodule_symbol_map, moduleBase, moduleName);
            if (it == hmodule_symbol_map.end())
                name = "<unknown>";
            else
            {
                const std::string &moduleName = it->second.first;
                const astl::map<DWORD64, SymbolInfo> &symbolMap = it->second.second;
                if (symbolMap.empty()) // Try get symbol info from .edata
                {
                    DWORD64 displacementSym = 0;
                    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
                    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                    pSymbol->MaxNameLen = MAX_SYM_NAME;
                    if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, &displacementSym, pSymbol))
                        name = pSymbol->Name;
                    else
                        name = "<unknown>";
                }
                else
                    name = findSymbolFromTable(stackFrame.AddrPC.Offset, symbolMap);
            }
            stream << " in " << demangle(name.c_str());
            if (moduleBase != (DWORD64)mainModule && !moduleName.empty()) stream << " at " << moduleName;
        }
        else
            stream << " in <unknown>";

        stream << std::endl;
        frameIndex++;
    }

    SymCleanup(hProcess);
}

void createMiniDump(EXCEPTION_POINTERS *pep, const std::string &filename, std::ofstream &crash_stream)
{
    HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        crash_stream << "Failed to create dump file: " << GetLastError() << std::endl;
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = pep;
    mdei.ClientPointers = FALSE;

    BOOL success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal,
                                     (pep != nullptr) ? &mdei : NULL, NULL, NULL);

    if (!success) crash_stream << "MiniDumpWriteDump failed: " << GetLastError() << std::endl;
    CloseHandle(hFile);
}