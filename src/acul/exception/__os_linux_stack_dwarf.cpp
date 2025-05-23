#include <acul/string/string.hpp>
#include <acul/vector.hpp>
#include <dwarf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include "elf_read.hpp"

#define REG_MAX_COUNT 17

namespace acul
{
    struct cfa_rule
    {
        enum
        {
            UNDEF,
            OFFSET,
            SAME
        } kind;
        i32 offs;
    };

    struct unwind_state
    {
        gregset_t regs;
        u8 cfa_reg;
        i32 cfa_offs;
        cfa_rule ra_rule;
        i32 saved_offs[REG_MAX_COUNT];

        unwind_state() { std::fill(std::begin(saved_offs), std::end(saved_offs), INT32_MAX); }
    };
    struct fde_info
    {
        uintptr_t pc_begin;
        uintptr_t pc_end;
        const u8 *cfi;
        size_t cfi_size;
        i32 code_align;
        i32 data_align;
        u8 ra_reg;
    };

    struct eh_frame
    {
        vector<u8> buffer;
        size_t size;
        const u8 *hdr;
        size_t hdr_size;
        uintptr_t remote_base;
    };

    bool map_eh_frame_remote(pid_t pid, const string &path, uintptr_t load_bias, eh_frame &out)
    {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return false;

        struct stat st;
        if (fstat(fd, &st) < 0)
        {
            close(fd);
            return false;
        }

        void *base = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (base == MAP_FAILED) return false;

        auto *ehdr = reinterpret_cast<Elf64_Ehdr *>(base);
        auto *shdr = reinterpret_cast<Elf64_Shdr *>((char *)base + ehdr->e_shoff);
        const char *shstr = (char *)base + shdr[ehdr->e_shstrndx].sh_offset;

        for (int i = 0; i < ehdr->e_shnum; ++i)
        {
            if (strcmp(shstr + shdr[i].sh_name, ".eh_frame") == 0)
            {
                uintptr_t eh_remote_addr = load_bias + shdr[i].sh_addr;
                size_t eh_size = shdr[i].sh_size;

                out.buffer.resize(eh_size);
                iovec local{out.buffer.data(), eh_size};
                iovec remote{reinterpret_cast<void *>(eh_remote_addr), eh_size};
                if (process_vm_readv(pid, &local, 1, &remote, 1, 0) != (ssize_t)eh_size)
                {
                    munmap(base, st.st_size);
                    return false;
                }
                out.size = eh_size;
                out.remote_base = eh_remote_addr;
                return true;
            }
        }
        munmap(base, st.st_size);
        return false;
    }

    static inline u64 read_uleb(const u8 *&p)
    {
        u64 result = 0;
        unsigned shift = 0;
        while (true)
        {
            u8 byte = *p++;
            result |= u64(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }
        return result;
    }

    static inline i64 read_sleb(const u8 *&p)
    {
        i64 result = 0;
        unsigned shift = 0;
        u8 byte;
        do {
            byte = *p++;
            result |= i64(byte & 0x7F) << shift;
            shift += 7;
        } while (byte & 0x80);

        if (byte & 0x40) result |= -(i64(1) << shift);
        return result;
    }

    struct cie_fields
    {
        i32 code_align;
        i32 data_align;
        u8 ra_reg;
        u8 ptr_encoding;  // DW_EH_PE_*
        u8 lsda_encoding; // LSDA-pointer в FDE
        u8 personality_encoding;
        uintptr_t personality;
        bool is_signal_frame;
        const u8 *init_cfi;
        size_t init_cfi_size;
    };

    template <typename T>
    inline void load_aligned(const u8 *&p, u64 &out)
    {
        T tmp;
        memcpy(&tmp, p, sizeof(T));
        p += sizeof(T);
        if constexpr (std::is_signed_v<T>)
            out = static_cast<u64>(static_cast<std::make_signed_t<T>>(tmp));
        else
            out = static_cast<u64>(tmp);
    }

    u64 read_encoded_pointer(const u8 *&p, u8 encoding, uintptr_t base)
    {
        if (encoding == DW_EH_PE_omit) return 0;

        if ((encoding & 0x0F) == DW_EH_PE_aligned)
        {
            constexpr uintptr_t align = sizeof(uintptr_t);
            uintptr_t addr = reinterpret_cast<uintptr_t>(p);
            uintptr_t aligned = (addr + align - 1) & ~(align - 1);
            p = reinterpret_cast<const u8 *>(aligned);
            u64 result;
            load_aligned<uintptr_t>(p, result);
            return result;
        }

        u64 result = 0;
        switch (encoding & 0x0F)
        {
            case DW_EH_PE_absptr:
            case DW_EH_PE_udata8:
                load_aligned<uintptr_t>(p, result);
                break;
            case DW_EH_PE_uleb128:
                result = read_uleb(p);
                break;
            case DW_EH_PE_udata2:
                load_aligned<u16>(p, result);
                break;
            case DW_EH_PE_udata4:
                load_aligned<u32>(p, result);
                break;
            case DW_EH_PE_sleb128:
                result = static_cast<u64>(read_sleb(p));
                break;
            case DW_EH_PE_sdata2:
                load_aligned<i16>(p, result);
                break;
            case DW_EH_PE_sdata4:
                load_aligned<i32>(p, result);
                break;
            case DW_EH_PE_sdata8:
                load_aligned<i64>(p, result);
                break;
            default:
                return 0;
        }

        switch (encoding & 0x70)
        {
            case DW_EH_PE_pcrel:
            case DW_EH_PE_datarel:
            case DW_EH_PE_textrel:
            case DW_EH_PE_funcrel:
                result += static_cast<u64>(base);
                break;
            default:
                break;
        }

        // indirect
        if ((encoding & DW_EH_PE_indirect) && ((encoding & 0x0F) == DW_EH_PE_absptr))
        {
            uintptr_t tmp;
            memcpy(&tmp, reinterpret_cast<void *>(result), sizeof(tmp));
            result = static_cast<u64>(tmp);
        }

        return result;
    }

    bool parse_cie(const eh_frame &eh, const u8 *fde_p, cie_fields &cie)
    {
        i32 cie_off = *reinterpret_cast<const i32 *>(fde_p - 4);
        const u8 *cie_p = fde_p - 4 - cie_off;
        const u8 *p = cie_p + 4;
        p += 4; // CIE-id (0)

        u8 version = *p++;
        const char *aug = reinterpret_cast<const char *>(p);
        p += strlen(aug) + 1; //  augmentation

        cie.code_align = static_cast<int32_t>(read_uleb(p));
        cie.data_align = static_cast<int32_t>(read_sleb(p));
        cie.ra_reg = static_cast<uint8_t>(read_uleb(p));
        cie.ptr_encoding = DW_EH_PE_absptr;
        cie.lsda_encoding = DW_EH_PE_absptr;
        cie.personality_encoding = DW_EH_PE_absptr;
        cie.personality = 0;
        cie.is_signal_frame = false;

        if (strchr(aug, 'z'))
        {
            uint64_t aug_len = read_uleb(p);
            const uint8_t *end = p + aug_len;

            for (const char *q = aug; *q; ++q)
            {
                switch (*q)
                {
                    case 'z':
                        break;
                    case 'L': // LSDA encoding
                        cie.lsda_encoding = *p++;
                        break;
                    case 'P': // personality routine
                        cie.personality_encoding = *p++;
                        cie.personality = read_encoded_pointer(p, cie.personality_encoding, 0);
                        break;
                    case 'R': // FDE initial_location/range encoding
                        cie.ptr_encoding = *p++;
                        break;
                    case 'S': // сигнальный фрейм
                        cie.is_signal_frame = true;
                        break;
                    default:
                        break;
                }
            }
            p = end;
        }

        cie.init_cfi = p;
        cie.init_cfi_size = (fde_p - cie_p - 4) - (p - cie_p);
        return true;
    }

    size_t read_remote_encoded_pointer(pid_t pid, uintptr_t remote_addr, u8 encoding, uintptr_t base, u64 &result)
    {
        u8 buf[8];
        size_t read_size = 0;

        switch (encoding & 0x0F)
        {
            case DW_EH_PE_udata4:
            case DW_EH_PE_sdata4:
                read_size = 4;
                break;
            case DW_EH_PE_udata8:
            case DW_EH_PE_sdata8:
                read_size = 8;
                break;
            default:
                read_size = sizeof(void *);
                break;
        }

        iovec loc = {buf, read_size};
        iovec rem = {reinterpret_cast<void *>(remote_addr), read_size};

        if (process_vm_readv(pid, &loc, 1, &rem, 1, 0) != read_size) return 0;

        const u8 *p = buf;
        result = read_encoded_pointer(p, encoding, base);
        return read_size;
    }

    inline u32 peek_u32(const void *p)
    {
        u32 val;
        memcpy(&val, p, sizeof(val));
        return val;
    }

    bool find_fde(const eh_frame &eh, uintptr_t pc, cie_fields &cie, fde_info &fde, pid_t pid)
    {
        const u8 *p = eh.buffer.data();
        const u8 *end = p + eh.size;

        while (p + 8 < end)
        {
            u32 length = peek_u32(p);
            if (length == 0) break;
            const u8 *next = p + 4 + (length == 0xffffffff ? 8 : length);
            u32 cie_id = peek_u32(p + 4);
            if (cie_id != 0)
            {
                const u8 *fde_p = p + 8;
                if (!parse_cie(eh, fde_p, cie))
                {
                    p = next;
                    continue;
                }

                uintptr_t fde_remote_addr = eh.remote_base + (fde_p - eh.buffer.data());
                uintptr_t cursor = fde_remote_addr;

                u64 initial, range;

                {
                    uintptr_t word_addr = cursor;
                    cursor += read_remote_encoded_pointer(pid, word_addr, cie.ptr_encoding, word_addr, initial);
                }
                // Range from remote
                {
                    uintptr_t word_addr = cursor;
                    cursor += read_remote_encoded_pointer(pid, word_addr, cie.ptr_encoding & 0x0F, 0, range);
                }

                if (pc >= initial && pc < initial + range)
                {
                    fde.pc_begin = initial;
                    fde.pc_end = initial + range;
                    fde.code_align = cie.code_align;
                    fde.data_align = cie.data_align;
                    fde.ra_reg = cie.ra_reg;
                    fde.cfi = fde_p + (cursor - fde_remote_addr);
                    fde.cfi_size = next - fde_p - (cursor - fde_remote_addr);
                    return true;
                }
            }
            p = next;
        }
        return false;
    }

    void exec_cfi(const u8 *cfi, size_t size, intptr_t pc_rel, const fde_info &fde, unwind_state &st)
    {
        const u8 *p = cfi;
        intptr_t cur_loc = 0;

        while (p < cfi + size)
        {
            u8 op = *p++;
            u8 prim = op & 0xC0;

            if (prim == DW_CFA_advance_loc) // 0x40-0x7f
                cur_loc += (op & 0x3f) * fde.code_align;
            else if (prim == DW_CFA_offset) // compact DW_CFA_offset (0x80-0xbf)
            {
                u8 reg = op & 0x3f;
                i64 off = read_uleb(p) * fde.data_align;
                if (reg == fde.ra_reg)
                    st.ra_rule = {cfa_rule::OFFSET, (i32)off};
                else
                    st.saved_offs[reg] = (i32)off;
            }
            else
                switch (op)
                {
                    case DW_CFA_advance_loc1:
                        cur_loc += *p++ * fde.code_align;
                        break;
                    case DW_CFA_def_cfa:
                    {
                        u64 reg = read_uleb(p);
                        i64 off = read_uleb(p);
                        st.cfa_reg = static_cast<u8>(reg);
                        st.cfa_offs = static_cast<i32>(off);
                        break;
                    }
                    case DW_CFA_offset_extended:
                    {
                        u64 reg = read_uleb(p);
                        i64 off = read_uleb(p) * fde.data_align;
                        if (reg == fde.ra_reg) { st.ra_rule = {cfa_rule::OFFSET, (i32)off}; }
                        else { st.saved_offs[reg] = (i32)off; }
                    }
                    break;
                    case DW_CFA_offset_extended_sf:
                    {
                        u64 reg = read_uleb(p);
                        i64 off = read_sleb(p) * fde.data_align;
                        if (reg == fde.ra_reg) { st.ra_rule = {cfa_rule::OFFSET, static_cast<i32>(off)}; }
                        break;
                    }
                    case DW_CFA_def_cfa_register:
                    {
                        u64 reg = read_uleb(p);
                        st.cfa_reg = static_cast<u8>(reg);
                        break;
                    }
                    case DW_CFA_def_cfa_offset:
                        st.cfa_offs = static_cast<i32>(read_uleb(p));
                        break;

                    case DW_CFA_same_value:
                    {
                        u64 reg = read_uleb(p);
                        if (reg == fde.ra_reg) st.ra_rule = {cfa_rule::SAME, 0};
                        break;
                    }
                    case DW_CFA_nop:
                        break;

                    default:
                        p = cfi + size;
                }

            if (cur_loc > pc_rel) break;
        }
    }

    static uintptr_t get_reg_by_cfa(unwind_state &cur)
    {
        switch (cur.cfa_reg)
        {
            case 7:
                return cur.regs[REG_RSP];
            case 6:
                return cur.regs[REG_RBP];
            case 3:
                return cur.regs[REG_RBX];
            case 12:
                return cur.regs[REG_R12];
            case 13:
                return cur.regs[REG_R13];
            case 14:
                return cur.regs[REG_R14];
            case 15:
                return cur.regs[REG_R15];
            default:
                return 0;
        }
    }

    bool restore_saved_regs(pid_t pid, const unwind_state &cur, unwind_state &next, uintptr_t cfa)
    {
        for (int reg = 0; reg < REG_MAX_COUNT; ++reg)
        {
            i32 off = cur.saved_offs[reg];
            if (off == INT32_MAX) continue;
            uintptr_t addr = cfa + off;
            switch (reg)
            {
                case 16:
                {
                    iovec loc{&next.regs[REG_RIP], sizeof(uintptr_t)};
                    iovec rem{reinterpret_cast<void *>(addr), sizeof(uintptr_t)};
                    if (process_vm_readv(pid, &loc, 1, &rem, 1, 0) != sizeof(uintptr_t)) return false;
                    break;
                }
                case 6:
                {
                    iovec loc{&next.regs[REG_RBP], sizeof(uintptr_t)};
                    iovec rem{reinterpret_cast<void *>(addr), sizeof(uintptr_t)};
                    if (process_vm_readv(pid, &loc, 1, &rem, 1, 0) != sizeof(uintptr_t)) return false;
                    break;
                }
                case 3:
                {
                    iovec loc{&next.regs[REG_RBX], sizeof(uintptr_t)};
                    iovec rem{reinterpret_cast<void *>(addr), sizeof(uintptr_t)};
                    if (process_vm_readv(pid, &loc, 1, &rem, 1, 0) != sizeof(uintptr_t)) return false;
                    break;
                }
                case 12:
                case 13:
                case 14:
                case 15:
                {
                    uintptr_t *dst = nullptr;
                    if (reg == 12) dst = (uintptr_t *)&next.regs[REG_R12];
                    if (reg == 13) dst = (uintptr_t *)&next.regs[REG_R13];
                    if (reg == 14) dst = (uintptr_t *)&next.regs[REG_R14];
                    if (reg == 15) dst = (uintptr_t *)&next.regs[REG_R15];
                    iovec loc{dst, sizeof(uintptr_t)};
                    iovec rem{reinterpret_cast<void *>(addr), sizeof(uintptr_t)};
                    if (process_vm_readv(pid, &loc, 1, &rem, 1, 0) != sizeof(uintptr_t)) return false;
                    break;
                }
                default:
                    break;
            }
        }
        return true;
    }

    bool step_frame_remote(pid_t pid, const fde_info &fde, unwind_state &cur, unwind_state &next)
    {
        next = cur;

        uintptr_t reg_value = get_reg_by_cfa(cur);
        if (reg_value == 0) return false;

        uintptr_t cfa = reg_value + cur.cfa_offs;
        if (!restore_saved_regs(pid, cur, next, cfa)) return false;

        switch (cur.ra_rule.kind)
        {
            case cfa_rule::OFFSET:
            {
                uintptr_t cfa = reg_value + cur.cfa_offs;
                uintptr_t rip_addr = cfa + cur.ra_rule.offs;

                iovec loc_rip{&next.regs[REG_RIP], sizeof(uintptr_t)};
                iovec rem_rip{reinterpret_cast<void *>(rip_addr), sizeof(uintptr_t)};
                if (process_vm_readv(pid, &loc_rip, 1, &rem_rip, 1, 0) != sizeof(uintptr_t)) return false;

                if (cur.cfa_reg == 6)
                {
                    uintptr_t rbp_addr = cfa - 16;
                    iovec loc_rbp{&next.regs[REG_RBP], sizeof(uintptr_t)};
                    iovec rem_rbp{reinterpret_cast<void *>(rbp_addr), sizeof(uintptr_t)};
                    if (process_vm_readv(pid, &loc_rbp, 1, &rem_rbp, 1, 0) != sizeof(uintptr_t)) return false;
                }
                next.regs[REG_RSP] = cfa;
                break;
            }
            case cfa_rule::SAME:
            {
                iovec loc0{&next.regs[REG_RBP], sizeof(uintptr_t)};
                iovec rem0{reinterpret_cast<void *>(cur.regs[REG_RBP]), sizeof(uintptr_t)};
                if (process_vm_readv(pid, &loc0, 1, &rem0, 1, 0) != sizeof(uintptr_t)) return false;

                iovec loc1{&next.regs[REG_RIP], sizeof(uintptr_t)};
                iovec rem1{reinterpret_cast<void *>(cur.regs[REG_RBP] + sizeof(void *)), sizeof(uintptr_t)};
                if (process_vm_readv(pid, &loc1, 1, &rem1, 1, 0) != sizeof(uintptr_t)) return false;

                next.regs[REG_RSP] = cur.regs[REG_RBP] + 16;
                return true;
            }
            default:
                return false;
        }

        next.regs[REG_RSP] = cfa;
        return true;
    }

    bool capture_stack_trace_remote(pid_t pid, const ucontext_t &uctx, vector<void *> &out, size_t max_depth)
    {
        exec_table mods;
        if (!build_exec_table(pid, mods)) return false;

        unwind_state cur{};
        memcpy(cur.regs, uctx.uc_mcontext.gregs, sizeof(gregset_t));

        cur.cfa_reg = 7; // RSP
        cur.cfa_offs = 0;
        cur.ra_rule = {cfa_rule::OFFSET, -8};

        for (size_t depth = 0; depth < max_depth; ++depth)
        {
            uintptr_t pc = cur.regs[REG_RIP];

            auto mod_it = get_module_by_table(pc, mods);
            if (mod_it == mods.end() || !mod_it->is_exec) break;
            out.push_back(reinterpret_cast<void *>(pc));
            eh_frame eh;
            if (!map_eh_frame_remote(pid, mod_it->path, mod_it->load_bias, eh)) break;

            cie_fields cie;
            fde_info fde;
            if (!find_fde(eh, pc, cie, fde, pid)) break;

            // CIE part
            exec_cfi(cie.init_cfi, cie.init_cfi_size, 0, fde, cur);

            // FDE part
            intptr_t rel = static_cast<intptr_t>(pc) - static_cast<intptr_t>(fde.pc_begin);
            exec_cfi(fde.cfi, fde.cfi_size, rel, fde, cur);

            // Step frame
            uintptr_t reg_value = (cur.cfa_reg == 7    ? cur.regs[REG_RSP]
                                   : cur.cfa_reg == 6  ? cur.regs[REG_RBP]
                                   : cur.cfa_reg == 3  ? cur.regs[REG_RBX]
                                   : cur.cfa_reg == 12 ? cur.regs[REG_R12]
                                   : cur.cfa_reg == 13 ? cur.regs[REG_R13]
                                   : cur.cfa_reg == 14 ? cur.regs[REG_R14]
                                   : cur.cfa_reg == 15 ? cur.regs[REG_R15]
                                                       : 0);

            if (cur.ra_rule.kind != cfa_rule::OFFSET) break;

            uintptr_t cfa = reg_value + cur.cfa_offs;
            uintptr_t addr = cfa + cur.ra_rule.offs;

            unwind_state next = cur;
            if (!step_frame_remote(pid, fde, cur, next)) break;
            if (next.regs[REG_RIP] == cur.regs[REG_RIP]) break;
            cur = next;
        }
        return !out.empty();
    }
} // namespace acul