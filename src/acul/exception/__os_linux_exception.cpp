#include <acul/exception/exception.hpp>
#include <acul/exception/utils.hpp>
#include <acul/hash/hashmap.hpp>
#include <acul/io/path.hpp>
#include <acul/string/sstream.hpp>
#include "elf_read.hpp"

namespace acul
{

    bool capture_stack_trace_remote(pid_t pid, const ucontext_t &uctx, vector<void *> &out, size_t max_depth = 64);

    void capture_stack_trace(except_info &info)
    {
        vector<except_addr> addresses;
        if (info.pid == 0)
        {
            info.pid = getpid();
            constexpr size_t MaxFrames = 64;
            void *buffer[MaxFrames];
            int nptrs = backtrace(buffer, MaxFrames);
            for (int i = 0; i < nptrs; ++i) addresses.push_back(buffer[i]);
        }
        else if (!capture_stack_trace_remote(info.pid, info.context, addresses))
            return;
        info.addresses_count = addresses.size();
        info.addresses = addresses.release();
    }

    void write_exception_info(siginfo_t *info, stringstream &stream)
    {
        int sig = info->si_signo;
        stream << format("Signal: %d (%s)\n", sig, strsignal(sig));

        if (info)
        {
            stream << format("Signal code: %d\n", info->si_code);
            stream << format("Fault address: %p\n", info->si_addr);

            if (sig == SIGSEGV || sig == SIGBUS || sig == SIGFPE || sig == SIGILL)
            {
                stream << "Signal details: ";
                switch (info->si_code)
                {
                    case SEGV_MAPERR:
                        stream << "Address not mapped to object\n";
                        break;
                    case SEGV_ACCERR:
                        stream << "Invalid permissions for mapped object\n";
                        break;
                    default:
                        stream << "Unknown reason\n";
                        break;
                }
            }
        }
    }

    void write_frame_registers(stringstream &stream, const ucontext_t &context)
    {
        const auto &regs = context.uc_mcontext.gregs;

        stream << "Frame registers:\n";
        stream << format("\tRAX: 0x%llx\n", regs[REG_RAX]);
        stream << format("\tRBX: 0x%llx\n", regs[REG_RBX]);
        stream << format("\tRCX: 0x%llx\n", regs[REG_RCX]);
        stream << format("\tRDX: 0x%llx\n", regs[REG_RDX]);
        stream << format("\tRSI: 0x%llx\n", regs[REG_RSI]);
        stream << format("\tRDI: 0x%llx\n", regs[REG_RDI]);
        stream << format("\tRBP: 0x%llx\n", regs[REG_RBP]);
        stream << format("\tRSP: 0x%llx\n", regs[REG_RSP]);
        stream << format("\tRIP: 0x%llx\n", regs[REG_RIP]);
    }

    string get_symbol_name_from_elf(const elf_module &elf, uintptr_t ip)
    {
        auto it = elf.symbols.upper_bound(ip);
        if (it == elf.symbols.begin()) return "<unknown>";
        --it;
        if (ip < it->first || ip >= it->second.end_address) return "<unknown>";
        string demangled = demangle(it->second.name.c_str());
        return demangled.empty() ? "<unknown>" : demangled;
    }

    void write_stack_trace(stringstream &out, const except_info &info)
    {
        exec_table module_table;
        build_exec_table(info.pid, module_table);
        vector<exec_module>::iterator main_module_it = module_table.end();
        char main_module_path[PATH_MAX];
        if (io::get_executable_path(main_module_path, PATH_MAX))
        {
            string str = (const char *)main_module_path;
            main_module_it = std::find_if(module_table.begin(), module_table.end(),
                                          [&str](const exec_module &module) { return module.path == str; });
        }
        out << "Stack trace:\n";
        hashmap<exec_module *, elf_module> module_load_cache;
        module_load_cache.reserve(module_table.size());
        for (size_t i = 0; i < info.addresses_count; ++i)
        {
            auto ip = reinterpret_cast<uintptr_t>(info.addresses[i]);
            string msg = format("\t#%d 0x%llx", i, ip);
            out << msg;
            auto module_it = get_module_by_table(ip, module_table);
            bool is_error = false;
            if (module_it != module_table.end())
            {
                if (module_it->is_exec)
                {
                    auto load_it = module_load_cache.find(&(*module_it));
                    if (load_it == module_load_cache.end())
                    {
                        elf_module elf;
                        if (!load_module(module_it->path, elf))
                            is_error = true;
                        else
                        {
                            auto load_pair = module_load_cache.emplace(&(*module_it), elf);
                            load_it = load_pair.first;
                        }
                    }
                    if (!is_error)
                    {
                        auto &elf = load_it->second;
                        auto ip_local_va = elf.e_type == ET_EXEC ? ip : ip - module_it->load_bias;
                        out << " in " << get_symbol_name_from_elf(elf, ip_local_va);
                    }
                }
                if (module_it != main_module_it) out << " at " << io::get_filename(module_it->path).c_str();
            }
            else
                out << " at " << "<unknown>";
            out << '\n';
        }
    }
} // namespace acul