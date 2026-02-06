#include <acul/bin_stream.hpp>
#include <acul/io/fs/file.hpp>
#include <acul/memory/smart_ptr.hpp>
#include <acul/string/string_view_pool.hpp>
#include <acul/string/utils.hpp>
#include <dirent.h>
#include <elf.h>
#include <signal.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>

#define ELF_BUFFER_CHUNK 0x10000
#define ELF_HDR_ALIGN    8
#define ELF_NOTE_ALIGN   4
#define ELF_XSAVE_MAX    4096

namespace acul
{
    class proc_mem_reader
    {
        pid_t _pid;
        bool _fast{false};
        struct Cl
        {
            void operator()(int *p) const
            {
                if (*p != -1) ::close(*p);
            }
        };
        unique_ptr<int, Cl> _fd{new int(-1)};
        bool probe()
        {
            u64 in = 0x1122334455667788ULL, out = 0;
            iovec l{&out, sizeof out}, r{&in, sizeof in};
            return ::process_vm_readv(_pid, &l, 1, &r, 1, 0) == sizeof(out) && out == in;
        }

    public:
        explicit proc_mem_reader(pid_t pid) : _pid(pid)
        {
            _fast = probe();
            if (!_fast)
            {
                auto p = format("/proc/%d/mem", pid);
                int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
                _fd.reset(alloc<int>(fd));
            }
        }

        size_t read(uintptr_t a, void *out, size_t len)
        {
            if (_fast)
            {
                iovec l{out, len}, r{(void *)a, len};
                ssize_t rc = ::process_vm_readv(_pid, &l, 1, &r, 1, 0);
                return rc > 0 ? (size_t)rc : 0;
            }
            if (*_fd != -1)
            {
                if (::lseek(*_fd, a, SEEK_SET) == -1) return 0;
                ssize_t rc = ::read(*_fd, out, len);
                return rc > 0 ? (size_t)rc : 0;
            }
            return 0;
        }
    };

    struct segment
    {
        uintptr_t vaddr, end;
        u32 flags;
        size_t file_off;
        size_t file_size; //  .p_filesz
    };

    struct file_mapping_t
    {
        uintptr_t vaddr, end;
        uintptr_t file_offset;
        string path;
    };
    struct process_view
    {
        pid_t pid;
        vector<pid_t> tids;
        vector<segment> segs;
        vector<file_mapping_t> file_mappings;
        vector<Elf64_auxv_t> auxv;
        size_t page;
    };

    static vector<pid_t> list_threads(pid_t pid)
    {
        vector<pid_t> v;
        string d = format("/proc/%d/task", pid);
        DIR *dir = ::opendir(d.c_str());
        if (!dir) return v;
        while (auto *e = ::readdir(dir))
        {
            if (e->d_type == DT_DIR)
            {
                char *end = nullptr;
                long val = strtol(e->d_name, &end, 10);
                if (end && *end == '\0' && val > 0) v.push_back((pid_t)val);
            }
        }
        ::closedir(dir);
        std::sort(v.begin(), v.end());
        return v;
    }

    static bool read_auxv(pid_t pid, vector<Elf64_auxv_t> &v)
    {
        string p = format("/proc/%d/auxv", pid);
        int fd = ::open(p.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd == -1) return false;
        Elf64_auxv_t t;
        while (::read(fd, &t, sizeof t) == sizeof t) v.push_back(t);
        ::close(fd);
        return true;
    }

    static bool parse_maps(pid_t pid, vector<segment> &segments, vector<file_mapping_t> &mappings, size_t &page)
    {
        string maps_file = format("/proc/%d/maps", pid);
        vector<char> content;
        if (!fs::read_virtual(maps_file, content)) return false;
        string_view_pool<char> lines;
        lines.reserve(content.size() / 96);
        fill_line_buffer(content.data(), content.size(), lines);

        constexpr size_t ELF_SMALL_ANON = 128 * 1024;
        constexpr size_t ELF_HEAD_ANON = 64 * 1024;

        for (const auto &line : lines)
        {
            uintptr_t lo = 0, hi = 0, off = 0;
            char perm[5] = {};
            int devm = 0, devn = 0;
            unsigned long inode = 0;
            char path_buf[PATH_MAX] = {};

            int n = sscanf(line.data(), "%lx-%lx %4s %lx %x:%x %lu %s", &lo, &hi, perm, &off, &devm, &devn, &inode,
                           path_buf);
            if (n < 7) continue; // must have at least perms
            const size_t size = hi - lo;

            u32 fl = 0;
            if (strchr(perm, 'r')) fl |= PF_R;
            if (strchr(perm, 'w')) fl |= PF_W;
            if (strchr(perm, 'x')) fl |= PF_X;

            bool is_file_backed = (inode != 0 && n == 8 && path_buf[0]);

            // PT_LOAD filter
            bool include_full = (n == 8 && path_buf[0] != '\0');
            bool include_head = false;
            if (fl & PF_X) include_full = true;
            else if ((fl & PF_R) && !(fl & PF_W)) include_full = true; // .rodata, .eh_frame
            else if ((fl & PF_R) && (fl & PF_W))
            {
                if (is_file_backed) include_full = true; // .data/stacks
                else if (size <= ELF_SMALL_ANON) include_full = true;
                else include_head = true;
            }

            if (include_full) segments.push_back({lo, hi, fl, 0, hi - lo});
            else if (include_head)
            {
                size_t head = std::min(ELF_HEAD_ANON, size);
                segments.emplace_back(lo, lo + head, fl, 0, head);
            }
            if (is_file_backed) mappings.emplace_back(lo, hi, off, (const char *)path_buf);
        }

        page = static_cast<size_t>(::sysconf(_SC_PAGESIZE));
        return !segments.empty();
    }

    static bool build_view(pid_t pid, process_view &pv)
    {
        pv.pid = pid;
        pv.tids = list_threads(pid);
        if (pv.tids.empty()) return false;
        if (!parse_maps(pid, pv.segs, pv.file_mappings, pv.page)) return false;
        read_auxv(pid, pv.auxv);
        return true;
    }

    static void write_ehdr(bin_stream &w, size_t phnum)
    {
        Elf64_Ehdr eh{};
        eh.e_ident[EI_MAG0] = ELFMAG0;
        eh.e_ident[EI_MAG1] = ELFMAG1;
        eh.e_ident[EI_MAG2] = ELFMAG2;
        eh.e_ident[EI_MAG3] = ELFMAG3;
        eh.e_ident[EI_CLASS] = ELFCLASS64;
        eh.e_ident[EI_DATA] = ELFDATA2LSB;
        eh.e_ident[EI_VERSION] = EV_CURRENT;
        eh.e_type = ET_CORE;
        eh.e_machine = EM_X86_64;
        eh.e_phoff = sizeof eh;
        eh.e_ehsize = sizeof eh;
        eh.e_phentsize = sizeof(Elf64_Phdr);
        eh.e_phnum = (u16)phnum;
        w.write(eh);
    }

    static size_t write_phdrs(bin_stream &w, process_view &pv, size_t note_sz)
    {
        size_t phnum = pv.segs.size() + 1;
        size_t eh_ph =
            align_up(sizeof(Elf64_Ehdr), ELF_HDR_ALIGN) + align_up(sizeof(Elf64_Phdr) * phnum, ELF_HDR_ALIGN);
        Elf64_Phdr note{};
        note.p_type = PT_NOTE;
        note.p_offset = align_up(eh_ph, ELF_HDR_ALIGN);
        note.p_filesz = note_sz;
        note.p_memsz = note_sz;
        note.p_align = 1;
        w.write(note);
        size_t cur = align_up(note.p_offset + note_sz, pv.page);
        for (auto &s : pv.segs)
        {
            s.file_off = cur;
            Elf64_Phdr ph{};
            ph.p_type = PT_LOAD;
            ph.p_flags = s.flags;
            ph.p_offset = cur;
            ph.p_vaddr = s.vaddr;
            ph.p_filesz = s.file_size;
            ph.p_memsz = ph.p_filesz;
            ph.p_align = pv.page;
            w.write(ph);
            cur += ph.p_filesz;
        }
        return cur;
    }

    void write_note_entry(bin_stream &w, u32 t, string_view name, const void *d, size_t sz)
    {
        Elf64_Nhdr h{};
        h.n_type = t;
        h.n_namesz = static_cast<u32>(name.size() + 1);
        h.n_descsz = static_cast<u32>(sz);
        w.write(h).write(name.data(), name.size()).write("\0", 1);

        size_t name_pad = align_up(h.n_namesz, ELF_NOTE_ALIGN) - h.n_namesz;
        w.write_pad(name_pad).write((char *)d, sz).write_align(ELF_NOTE_ALIGN);
    }

    static inline string get_comm(pid_t pid)
    {
        string path = format("/proc/%d/comm", pid);
        vector<char> buf;
        if (!fs::read_virtual(path, buf)) return "unknown";
        return trim_end(buf.data(), buf.size());
    }

    static inline string get_cmdline(pid_t pid)
    {
        string path = format("/proc/%d/cmdline", pid);
        vector<char> buf;
        if (!fs::read_virtual(path, buf)) return {};
        std::replace(buf.begin(), buf.end(), '\0', ' ');
        return buf.data();
    }

    static void write_crash_tid_status(prstatus_t &pr, const mcontext_t &mc, int crash_sig)
    {
        struct user_regs_struct regs{};
        regs.r8 = mc.gregs[REG_R8];
        regs.r9 = mc.gregs[REG_R9];
        regs.r10 = mc.gregs[REG_R10];
        regs.r11 = mc.gregs[REG_R11];
        regs.r12 = mc.gregs[REG_R12];
        regs.r13 = mc.gregs[REG_R13];
        regs.r14 = mc.gregs[REG_R14];
        regs.r15 = mc.gregs[REG_R15];
        regs.rdi = mc.gregs[REG_RDI];
        regs.rsi = mc.gregs[REG_RSI];
        regs.rbp = mc.gregs[REG_RBP];
        regs.rbx = mc.gregs[REG_RBX];
        regs.rdx = mc.gregs[REG_RDX];
        regs.rax = mc.gregs[REG_RAX];
        regs.rcx = mc.gregs[REG_RCX];
        regs.rsp = mc.gregs[REG_RSP];
        regs.rip = mc.gregs[REG_RIP];
        regs.eflags = mc.gregs[REG_EFL];
        regs.cs = (unsigned long)(mc.gregs[REG_CSGSFS] & 0xffff);
        regs.ss = (unsigned long)(mc.gregs[REG_CSGSFS] >> 16);

        static_assert(sizeof(regs) == sizeof(pr.pr_reg));
        memcpy(pr.pr_reg, &regs, sizeof(regs));

        pr.pr_cursig = crash_sig;
        pr.pr_info.si_signo = crash_sig;
        pr.pr_sigpend = 0;
        pr.pr_sighold = 0;
    }

    void write_threads(bin_stream &w, const process_view &pv, pid_t crash_tid, int crash_sig, const ucontext_t &uc)
    {
        for (pid_t tid : pv.tids)
        {
            prstatus_t pr{};
            pr.pr_pid = tid;
            pr.pr_ppid = pv.pid;
            if (tid == crash_tid) write_crash_tid_status(pr, uc.uc_mcontext, crash_sig);
            else
            {
                elf_gregset_t regs{};
                iovec iov{regs, sizeof(regs)};
                if (ptrace(PTRACE_GETREGSET, tid, (void *)NT_PRSTATUS, &iov) != -1)
                {
                    static_assert(sizeof(regs) == sizeof(pr.pr_reg));
                    memcpy(pr.pr_reg, regs, sizeof(pr.pr_reg));
                    pr.pr_cursig = 0;
                    pr.pr_sigpend = 0;
                    pr.pr_sighold = 0;
                    pr.pr_info.si_signo = 0;
                }
            }
            write_note_entry(w, NT_PRSTATUS, "CORE", &pr, sizeof(pr));
        }
    }

    static inline void write_file64(vector<u8> &file_note, u64 v)
    {
        for (int i = 0; i < 8; ++i) file_note.push_back(static_cast<u8>(v >> (i * 8)));
    }

    void write_file_mappings(bin_stream &w, const process_view &pv)
    {
        const auto &fm = pv.file_mappings;
        if (fm.empty()) return;

        vector<u8> file_note;

        // header: count, page size
        write_file64(file_note, fm.size());
        write_file64(file_note, pv.page);

        // triplets: (start, end, file_offset)
        for (const auto &f : fm)
        {
            write_file64(file_note, f.vaddr);
            write_file64(file_note, f.end);
            write_file64(file_note, f.file_offset);
        }

        // null-terminated strings
        for (const auto &f : fm)
        {
            file_note.insert(file_note.end(), f.path.begin(), f.path.end());
            file_note.push_back('\0');
        }

        write_note_entry(w, NT_FILE, "CORE", file_note.data(), file_note.size());
    }

    static size_t write_notes(bin_stream &w, const process_view &pv, pid_t crash_tid, int crash_sig,
                              const ucontext_t &uc)
    {
        size_t start = w.size();

        // NT_PRPSINFO
        prpsinfo_t ps{};
        string comm = get_comm(pv.pid);
        string cmdline = get_cmdline(pv.pid);
        strncpy(ps.pr_fname, comm.c_str(), sizeof(ps.pr_fname) - 1);
        strncpy(ps.pr_psargs, cmdline.c_str(), sizeof(ps.pr_psargs) - 1);
        ps.pr_pid = pv.pid;
        ps.pr_ppid = getppid();
        ps.pr_uid = getuid();
        ps.pr_gid = getgid();
        write_note_entry(w, NT_PRPSINFO, "CORE", &ps, sizeof(ps));

        // NT_AUXV
        if (!pv.auxv.empty())
            write_note_entry(w, NT_AUXV, "CORE", pv.auxv.data(), pv.auxv.size() * sizeof(Elf64_auxv_t));

        // NT_PRSTATUS
        write_threads(w, pv, crash_tid, crash_sig, uc);

        // NT_FILE
        write_file_mappings(w, pv);

        // NT_SIGINFO
        if (crash_tid && crash_sig)
        {
            siginfo_t si{};
            si.si_signo = crash_sig;
            si.si_pid = pv.pid;
            si.si_code = SI_USER;
            write_note_entry(w, NT_SIGINFO, "CORE", &si, sizeof(si));
        }

        // NT_FPREGSET
        vector<u8> fpbuf(sizeof(struct _fpstate));
        iovec fp_iov{fpbuf.data(), fpbuf.size()};
        if (ptrace(PTRACE_GETREGSET, crash_tid, (void *)NT_FPREGSET, &fp_iov) != -1)
            write_note_entry(w, NT_FPREGSET, "LINUX", fpbuf.data(), fp_iov.iov_len);

        // NT_X86_XSTATE
        vector<u8> xstate(ELF_XSAVE_MAX);
        iovec xs_iov{xstate.data(), xstate.size()};
        if (ptrace(PTRACE_GETREGSET, crash_tid, (void *)NT_X86_XSTATE, &xs_iov) != -1)
            write_note_entry(w, NT_X86_XSTATE, "LINUX", xstate.data(), xs_iov.iov_len);

        return w.size() - start;
    }

    static void dump_memory(bin_stream &w, const process_view &pv)
    {
        proc_mem_reader rdr(pv.pid);
        std::array<char, ELF_BUFFER_CHUNK> buf{};
        for (const auto &s : pv.segs)
        {
            uintptr_t cur = s.vaddr;
            while (cur < s.end)
            {
                size_t todo = std::min<size_t>(ELF_BUFFER_CHUNK, s.end - cur);
                size_t got = rdr.read(cur, buf.data(), todo);
                if (!got)
                {
                    got = todo;
                    memset(buf.data(), 0xF1, got);
                }
                w.write(buf.data(), got);
                cur += got;
            }
        }
    }

    static bool seize_and_stop_threads(const vector<pid_t> &tids)
    {
        for (pid_t tid : tids)
            if (ptrace(PTRACE_SEIZE, tid, nullptr, 0) == -1) return false;

        for (pid_t tid : tids)
        {
            if (ptrace(PTRACE_INTERRUPT, tid, nullptr, nullptr) == -1) return false;
            int status;
            if (waitpid(tid, &status, 0) == -1 || !WIFSTOPPED(status)) return false;
        }
        return true;
    }

    APPLIB_API bool create_mini_dump(pid_t pid, pid_t crash_tid, int crash_sig, const ucontext_t &uc, vector<char> &out)
    {
        // Build process view
        process_view pv{};
        if (!build_view(pid, pv)) return false;
        if (!seize_and_stop_threads(pv.tids)) return false;

        // Buffer for writing
        bin_stream bw;

        // Reserve header and phdr slots
        size_t hdr_pos = bw.size();
        bw.write_pad(sizeof(Elf64_Ehdr));
        size_t phnum = pv.segs.size() + 1;
        bw.write_pad(sizeof(Elf64_Phdr) * phnum);

        // Write notes
        bw.write_align(ELF_HDR_ALIGN);
        size_t note_sz = write_notes(bw, pv, crash_tid, crash_sig, uc);

        // Align to page boundary before dumping memory
        bw.write_align(pv.page);

        // Dump memory regions
        dump_memory(bw, pv);

        // Build final header + phdrs
        bin_stream hw;
        hw.reserve(sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * phnum);
        write_ehdr(hw, phnum);
        write_phdrs(hw, pv, note_sz);

        // Patch into buffer
        memcpy(bw.data() + hdr_pos, hw.data(), hw.size());

        // Detach threads
        for (pid_t tid : pv.tids) ptrace(PTRACE_DETACH, tid, nullptr, nullptr);

        // Move to output
        out.assign(bw.begin(), bw.end());
        return true;
    }
} // namespace acul