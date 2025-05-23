#pragma once
#include <acul/map.hpp>
#include <acul/pair.hpp>
#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <elf.h>

namespace acul
{
    struct exec_module
    {
        string path;
        vector<range<uintptr_t>> ranges;
        uintptr_t load_bias = UINTPTR_MAX;
        bool is_exec = false;
    };

    using exec_table = vector<exec_module>;

    inline exec_table::const_iterator get_module_by_table(uintptr_t ip, const exec_table &table)
    {
        return std::find_if(table.begin(), table.end(), [ip](const exec_module &module) {
            return std::any_of(module.ranges.begin(), module.ranges.end(),
                               [ip](const range<uintptr_t> &r) { return ip >= r.begin && ip < r.end; });
        });
    }

    bool build_exec_table(pid_t pid, exec_table &out);

    struct symbol_info
    {
        string name;
        uintptr_t end_address;
    };

    struct map_entry
    {
        uintptr_t lo, hi;
        string path;
    };

    using symbol_map_t = map<uintptr_t, symbol_info>;

    struct elf_module
    {
        Elf64_Half e_type;
        symbol_map_t symbols;
    };

    bool load_module(const string &path, elf_module &module);
} // namespace acul