#include <acul/hash/hashmap.hpp>
#include <acul/io/file.hpp>
#include <acul/string/utils.hpp>
#include "elf_read.hpp"

namespace acul
{
    bool build_exec_table(pid_t pid, exec_table &out)
    {
        string maps_file = format("/proc/%d/maps", pid);
        vector<char> content;
        if (io::file::read_virtual(maps_file, content) != io::file::op_state::Success) return false;
        string_pool<char> lines(content.size());
        io::file::fill_line_buffer(content.data(), content.size(), lines);
        hashmap<string, int> path_map;
        for (const auto &line : lines)
        {
            uintptr_t lo = 0, hi = 0, file_offset = 0;
            char perms[5] = {};
            char path_buf[PATH_MAX] = {};
            if (sscanf(line, "%lx-%lx %4s %lx %*s %*s %s", &lo, &hi, perms, &file_offset, path_buf) <= 3) continue;
            string path = (const char *)path_buf;
            auto [it, inserted] = path_map.try_emplace(path, out.size());
            exec_module *m = NULL;
            if (inserted)
            {
                out.emplace_back(path);
                m = &out.back();
            }
            else
                m = &out[it->second];
            m->ranges.emplace_back(lo, hi);
            m->is_exec = m->is_exec || perms[2] == 'x';
            m->load_bias = std::min(m->load_bias, lo - file_offset);
        }
        return !out.empty();
    }

    struct Sec
    {
        const Elf64_Sym *sym = nullptr;
        size_t cnt = 0;
        const char *str = nullptr;
    };

    static void collect_sec(Sec &src, elf_module &m)
    {
        if (!src.sym) return;

        for (size_t i = 0; i < src.cnt; ++i)
        {
            auto &e = src.sym[i];
            if (ELF64_ST_TYPE(e.st_info) != STT_FUNC || e.st_value == 0) continue;

            uintptr_t start = e.st_value; // RVA
            uintptr_t end = e.st_size ? start + e.st_size : 0;
            string name = src.str + e.st_name;
            m.symbols[start] = {std::move(name), end}; // key=RVA
        }
    }

    bool load_module(const string &path, elf_module &module)
    {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;
        struct stat st;
        if (fstat(fd, &st))
        {
            close(fd);
            return false;
        }
        void *data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (data == MAP_FAILED) return false;

        auto *eh = reinterpret_cast<Elf64_Ehdr *>(data);
        if (memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_ident[EI_CLASS] != ELFCLASS64)
        {
            munmap(data, st.st_size);
            return false;
        }

        module.e_type = eh->e_type;
        auto *sh = reinterpret_cast<Elf64_Shdr *>((char *)data + eh->e_shoff);

        Sec symtab, dynsym;
        const char *shstr = (char *)data + sh[eh->e_shstrndx].sh_offset;
        for (int i = 0; i < eh->e_shnum; ++i)
        {
            if (sh[i].sh_type == SHT_SYMTAB || sh[i].sh_type == SHT_DYNSYM)
            {
                auto &t = (sh[i].sh_type == SHT_SYMTAB ? symtab : dynsym);
                t.sym = reinterpret_cast<Elf64_Sym *>((char *)data + sh[i].sh_offset);
                t.cnt = sh[i].sh_size / sizeof(Elf64_Sym);
                t.str = (char *)data + sh[sh[i].sh_link].sh_offset;
            }
        }

        collect_sec(symtab, module);
        collect_sec(dynsym, module);

        vector<uintptr_t> keys;
        keys.reserve(module.symbols.size());
        for (const auto &p : module.symbols) keys.push_back(p.first);
        std::sort(keys.begin(), keys.end());

        munmap(data, st.st_size);
        return true;
    }

} // namespace acul