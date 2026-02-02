#include <acul/io/path.hpp>
#include <acul/string/utils.hpp>

namespace acul::fs
{
    path get_module_directory() noexcept
    {
#ifdef _WIN32
        HMODULE hModule = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&get_module_directory), &hModule);

        wchar_t path[MAX_PATH];
        GetModuleFileNameW(hModule, path, MAX_PATH);

        u16string full_path((c16 *)path);
        acul::path p = utf16_to_utf8(full_path);
#else
        Dl_info info;
        dladdr(reinterpret_cast<void *>(&get_module_directory), &info);
        io::path p(info.dli_fname);
#endif
        return p.parent_path();
    }
} // namespace acul::fs