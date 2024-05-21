#include <bee/crash/stacktrace_linux.h>
#include <bee/crash/strbuilder.h>
#include <bee/crash/unwind_linux.h>
#include <bfd.h>
#include <link.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if __has_include(<cxxabi.h>)
#    include <cxxabi.h>
#endif

namespace bee::crash {
    static void strbuilder_append(strbuilder &sb, const char *str) noexcept {
        if (str && *str) {
            sb.append(str, strlen(str));
        }
    }

    struct address_finder {
        asymbol **syms;
        bfd_vma pc;
        const char *filename;
        const char *functionname;
        unsigned int line;
        int found;
        static void iter(bfd *abfd, asection *section, void *data) noexcept {
            auto f = (address_finder *)(data);
            if (f->found)
                return;
            if ((bfd_section_flags(section) & SEC_ALLOC) == 0)
                return;
            bfd_vma vma = bfd_section_vma(section);
            if (f->pc < vma)
                return;
            bfd_size_type size = bfd_section_size(section);
            if (f->pc >= vma + size)
                return;
            f->found = bfd_find_nearest_line(abfd, section, f->syms, f->pc - vma, &f->filename, &f->functionname, &f->line);
        }
        bool find(bfd *abfd, bfd_vma pc) noexcept {
            this->pc    = pc;
            this->found = false;
            bfd_map_over_sections(abfd, address_finder::iter, (void *)this);
            return found;
        }
    };

    struct module_finder {
        const char *file;
        bfd_vma address;
        bfd_vma base;

        static int iter(struct dl_phdr_info *info, size_t size, void *data) noexcept {
            struct module_finder *f = (struct module_finder *)data;
            ElfW(Addr) load_base    = info->dlpi_addr;
            const ElfW(Phdr) *phdr  = info->dlpi_phdr;
            for (long n = info->dlpi_phnum; --n >= 0; phdr++) {
                if (phdr->p_type == PT_LOAD) {
                    ElfW(Addr) vaddr = phdr->p_vaddr + load_base;
                    if ((Elf64_Addr)f->address >= vaddr && (Elf64_Addr)f->address < vaddr + phdr->p_memsz) {
                        f->file = info->dlpi_name;
                        f->base = (bfd_vma)info->dlpi_addr;
                    }
                }
            }
            return 0;
        }

        static module_finder find(bfd_vma pc) noexcept {
            struct module_finder f = { .address = pc };
            dl_iterate_phdr(module_finder::iter, &f);
            f.address = pc - (bfd_vma)f.base;
            if (!f.file || !f.file[0]) {
                f.file = "/proc/self/exe";
            }
            return f;
        }
    };

    static void address_to_string(const char *modulename, bfd_vma addr, strbuilder &sb) noexcept {
        bfd *abfd = bfd_openr(modulename, NULL);
        if (abfd == NULL) {
            exit(1);
        }
        if (bfd_check_format(abfd, bfd_archive)) {
            exit(1);
        }
        char **matching;
        if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
            exit(1);
        }
        address_finder f;
        if ((bfd_get_file_flags(abfd) & HAS_SYMS) != 0) {
            unsigned int size;
            long symcount = bfd_read_minisymbols(abfd, false, (void **)&f.syms, &size);
            if (symcount == 0)
                symcount = bfd_read_minisymbols(abfd, true, (void **)&f.syms, &size);
            if (symcount < 0) {
                exit(1);
            }
        }

        if (f.find(abfd, addr)) {
            strbuilder_append(sb, f.filename);
            if (f.line != 0) {
                constexpr size_t max_line_num = sizeof("(4294967295): ") - 1;
                sb.expansion(max_line_num);
                const int ret = std::snprintf(sb.remaining_data(), max_line_num + 1, "(%u): ", f.line);
                if (ret <= 0) {
                    std::abort();
                }
                sb.append(ret);
            }
            if (strcmp(modulename, "/proc/self/exe") != 0) {
                const char *r = strrchr(modulename, '/');
                if (r != NULL) {
                    strbuilder_append(sb, r + 1);
                } else {
                    strbuilder_append(sb, modulename);
                }
                sb += "!";
            }
#if __has_include(<cxxabi.h>)
            int status     = 0;
            char *demangle = abi::__cxa_demangle(f.functionname, 0, 0, &status);
            if (demangle) {
                strbuilder_append(sb, demangle);
                free(demangle);
            } else {
                strbuilder_append(sb, f.functionname);
            }
#else
            strbuilder_append(sb, f.functionname);
#endif
        } else {
            constexpr size_t max_num = 32;
            sb.expansion(max_num);
            const int ret = std::snprintf(sb.remaining_data(), max_num + 1, "[0x%llx]", (long long unsigned int)addr);
            if (ret <= 0) {
                std::abort();
            }
            sb.append(ret);
        }
        if (f.syms != NULL) {
            free(f.syms);
        }
        bfd_close(abfd);
    }

    struct unwind_ud {
        strbuilder sb;
        unsigned int i = 0;
    };

    static bool unwind_func(void *pc, void *ud_) noexcept {
        struct unwind_ud *ud           = (struct unwind_ud *)ud_;
        constexpr size_t max_entry_num = sizeof("65536> ") - 1;
        ud->sb.expansion(max_entry_num);
        const int ret = std::snprintf(ud->sb.remaining_data(), max_entry_num + 1, "%u> ", ud->i++);
        if (ret <= 0) {
            std::abort();
        }
        ud->sb.append(ret);
        auto f = module_finder::find((bfd_vma)pc);
        address_to_string(f.file, f.address, ud->sb);
        ud->sb += "\n";
        return true;
    }

    std::string stacktrace(ucontext_t *ctx) noexcept {
        struct unwind_ud ud;
        bfd_init();
        unwind(ctx, 0, unwind_func, &ud);
        return ud.sb.to_string();
    }
}