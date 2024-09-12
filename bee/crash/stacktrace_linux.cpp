#include <bee/crash/stacktrace.h>
#include <bee/crash/strbuilder.h>

#define PACKAGE "bee.lua"
#define PACKAGE_VERSION 1
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
        asymbol **syms = NULL;
        bfd_vma pc;
        const char *filename;
        const char *functionname;
        unsigned int line;
        int found;
        static void iter(bfd *abfd, asection *section, void *data) noexcept {
            auto f = (address_finder *)(data);
            if (f->found)
                return;
#ifdef bfd_get_section_flags
            if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
                return;
#else
            if ((bfd_section_flags(section) & SEC_ALLOC) == 0)
                return;
#endif
#ifdef bfd_get_section_vma
            bfd_vma vma = bfd_get_section_vma(abfd, section);
#else
            bfd_vma vma = bfd_section_vma(section);
#endif
            if (f->pc < vma)
                return;
#ifdef bfd_get_section_size
            bfd_size_type size = bfd_get_section_size(section);
#else
            bfd_size_type size = bfd_section_size(section);
#endif
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

    static bool address_to_string(const char *modulename, bfd_vma addr, strbuilder &sb) noexcept {
        bfd *abfd = bfd_openr(modulename, NULL);
        if (abfd == NULL) {
            return false;
        }
        if (bfd_check_format(abfd, bfd_archive)) {
            return false;
        }
        char **matching;
        if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
            return false;
        }
        address_finder f;
        if ((bfd_get_file_flags(abfd) & HAS_SYMS) != 0) {
            unsigned int size;
            long symcount = bfd_read_minisymbols(abfd, false, (void **)&f.syms, &size);
            if (symcount == 0) {
                symcount = bfd_read_minisymbols(abfd, true, (void **)&f.syms, &size);
            }
            if (symcount < 0) {
                return false;
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
        }
        if (f.syms != NULL) {
            free(f.syms);
        }
        bfd_close(abfd);
        return f.found;
    }

    struct stacktrace_impl {
        strbuilder sb;
        unsigned int i = 0;

        bool initialize() noexcept {
            return true;
        }
        void add_frame(void *pc) noexcept {
            constexpr size_t max_entry_num = sizeof("65536> ") - 1;
            sb.expansion(max_entry_num);
            const int ret = std::snprintf(sb.remaining_data(), max_entry_num + 1, "%u> ", i++);
            if (ret <= 0) {
                std::abort();
            }
            sb.append(ret);
            auto f = module_finder::find((bfd_vma)pc);
            if (!address_to_string(f.file, f.address, sb)) {
                constexpr size_t max_num = 32;
                sb.expansion(max_num);
                const int ret = std::snprintf(sb.remaining_data(), max_num + 1, "[0x%llx]", (long long unsigned int)(f.base + f.address));
                if (ret <= 0) {
                    std::abort();
                }
                sb.append(ret);
            }
            sb += "\n";
        }
        std::string to_string() noexcept {
            return sb.to_string();
        }
    };

    stacktrace::stacktrace() noexcept
        : impl(new stacktrace_impl) {}

    stacktrace::~stacktrace() noexcept {
        delete impl;
    }

    bool stacktrace::initialize() noexcept {
        return impl->initialize();
    }
    void stacktrace::add_frame(void *pc) noexcept {
        impl->add_frame(pc);
    }
    std::string stacktrace::to_string() noexcept {
        return impl->to_string();
    }
}
