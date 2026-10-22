// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/loader/loader.h"
#include "core/ntdll/teb.h"
#include "core/api/api.h"
#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"

#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <sys/mman.h>
#include <dirent.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace ThetaZ {

// Case-insensitive string comparison
static bool strcasecmp_c(const char* a, const char* b) {
    while (*a && *b) {
        if (std::tolower(*a) != std::tolower(*b)) return false;
        ++a; ++b;
    }
    return *a == *b;
}

// Find file case-insensitively in a directory
static std::string find_file_case_insensitive(const std::string& dir, const std::string& target) {
    DIR* d = opendir(dir.c_str());
    if (!d) return "";
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (strcasecmp_c(entry->d_name, target.c_str())) {
            closedir(d);
            return std::string(dir) + "/" + entry->d_name;
        }
    }
    closedir(d);
    return "";
}

#pragma pack(push, 1)
struct DOSHeader {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
};

struct COFFHeader {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
};

struct OptionalHeader64 {
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t size_of_stack_reserve;
    uint64_t size_of_stack_commit;
    uint64_t size_of_heap_reserve;
    uint64_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    uint64_t data_directory[16];
};

struct OptionalHeader32 {
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t size_of_code;
    uint32_t size_of_initialized_data;
    uint32_t size_of_uninitialized_data;
    uint32_t address_of_entry_point;
    uint32_t base_of_code;
    uint32_t base_of_data;
    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t major_os_version;
    uint16_t minor_os_version;
    uint16_t major_image_version;
    uint16_t minor_image_version;
    uint16_t major_subsystem_version;
    uint16_t minor_subsystem_version;
    uint32_t win32_version_value;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t size_of_stack_reserve;
    uint32_t size_of_stack_commit;
    uint32_t size_of_heap_reserve;
    uint32_t size_of_heap_commit;
    uint32_t loader_flags;
    uint32_t number_of_rva_and_sizes;
    uint32_t data_directory[16];
};

struct DataDirectory {
    uint32_t virtual_address;
    uint32_t size;
};

struct SectionHeader {
    uint8_t name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_line_numbers;
    uint16_t number_of_relocations;
    uint16_t number_of_line_numbers;
    uint32_t characteristics;
};

struct IMAGE_IMPORT_DESCRIPTOR {
    uint32_t original_first_thunk;
    uint32_t time_date_stamp;
    uint32_t forwarder_chain;
    uint32_t name;
    uint32_t first_thunk;
};

struct IMAGE_BASE_RELOCATION {
    uint32_t virtual_address;
    uint32_t size_of_block;
};

struct IMAGE_EXPORT_DIRECTORY {
    uint32_t characteristics;
    uint32_t time_date_stamp;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t name;
    uint32_t base;
    uint32_t number_of_functions;
    uint32_t number_of_names;
    uint32_t address_of_functions;
    uint32_t address_of_names;
    uint32_t address_of_name_ordinals;
};

struct IMAGE_TLS_DIRECTORY32 {
    uint32_t start_address_of_raw_data;
    uint32_t end_address_of_raw_data;
    uint32_t address_of_index;
    uint32_t address_of_callbacks;
    uint32_t size_of_zero_fill;
    uint32_t characteristics;
};

struct IMAGE_THUNK_DATA64 {
    union {
        uint64_t forwarder_string;
        struct {
            uint16_t forwarder_helper;
            uint16_t ordinal;
        };
        uint64_t address_of_data;
    };
};
#pragma pack(pop)

// Format of the loaded image (PE32 vs PE32+). Used to pick the right optional
// header layout, image base width, thunk width, and relocation type.
struct PeFormat {
    bool is_32bit;
    uint64_t image_base;
    uint32_t size_of_image;
    uint32_t size_of_headers;
    uint32_t address_of_entry_point;
    uint32_t number_of_rva_and_sizes;
    uint64_t data_directory[16]; // low 32 bits = RVA, high 32 bits = size
};

static PeFormat read_pe_format(std::vector<uint8_t>& data) {
    PeFormat fmt{};
    if (data.size() < sizeof(DOSHeader)) return fmt;

    DOSHeader* dos = reinterpret_cast<DOSHeader*>(data.data());
    if (dos->e_magic != 0x5A4D) return fmt;
    if (dos->e_lfanew + 4 + sizeof(COFFHeader) + 2 > data.size()) return fmt;

    uint32_t pe_magic = *reinterpret_cast<uint32_t*>(data.data() + dos->e_lfanew);
    if (pe_magic != 0x00004550) return fmt;

    uint16_t opt_magic = *reinterpret_cast<uint16_t*>(
        data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader));

    if (opt_magic == 0x20B) {
        OptionalHeader64* opt = reinterpret_cast<OptionalHeader64*>(
            data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader));
        fmt.is_32bit = false;
        fmt.image_base = opt->image_base;
        fmt.size_of_image = opt->size_of_image;
        fmt.size_of_headers = opt->size_of_headers;
        fmt.address_of_entry_point = opt->address_of_entry_point;
        fmt.number_of_rva_and_sizes = opt->number_of_rva_and_sizes;
        // Data directory entries are 8 bytes ({rva, size}); pack both halves.
        const uint8_t* dd = data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader) + 112;
        for (int i = 0; i < 16; i++) {
            uint32_t rva = *reinterpret_cast<const uint32_t*>(dd + i * 8);
            uint32_t size = *reinterpret_cast<const uint32_t*>(dd + i * 8 + 4);
            fmt.data_directory[i] = rva | (static_cast<uint64_t>(size) << 32);
        }
    } else if (opt_magic == 0x10B) {
        OptionalHeader32* opt = reinterpret_cast<OptionalHeader32*>(
            data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader));
        fmt.is_32bit = true;
        fmt.image_base = opt->image_base;
        fmt.size_of_image = opt->size_of_image;
        fmt.size_of_headers = opt->size_of_headers;
        fmt.address_of_entry_point = opt->address_of_entry_point;
        fmt.number_of_rva_and_sizes = opt->number_of_rva_and_sizes;
        // PE32 data directory starts at offset 96 of the optional header.
        const uint8_t* dd = data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader) + 96;
        for (int i = 0; i < 16; i++) {
            uint32_t rva = *reinterpret_cast<const uint32_t*>(dd + i * 8);
            uint32_t size = *reinterpret_cast<const uint32_t*>(dd + i * 8 + 4);
            fmt.data_directory[i] = rva | (static_cast<uint64_t>(size) << 32);
        }
    }
    return fmt;
}

PELoader::PELoader() {}
PELoader::~PELoader() {
    for (auto& entry : modules_) {
        if (entry.second.mapped_data) {
            munmap(entry.second.mapped_data, entry.second.size);
        }
    }
}

bool PELoader::read_file(const char* path, std::vector<uint8_t>& data) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0) {
        fclose(fp);
        return false;
    }

    data.resize(size);
    fread(data.data(), 1, size, fp);
    fclose(fp);
    return true;
}

bool PELoader::validate_pe(std::vector<uint8_t>& data) {
    PeFormat fmt = read_pe_format(data);
    if (fmt.image_base == 0 && fmt.size_of_image == 0) return false;
    return fmt.size_of_image > 0;
}

uint64_t PELoader::map_sections(std::vector<uint8_t>& data, uint64_t base_addr, bool force_fresh) {
    DOSHeader* dos = reinterpret_cast<DOSHeader*>(data.data());
    COFFHeader* coff = reinterpret_cast<COFFHeader*>(data.data() + dos->e_lfanew + 4);
    PeFormat fmt = read_pe_format(data);

    uint64_t image_base = base_addr ? base_addr : fmt.image_base;

    void* addr = nullptr;
    if (!force_fresh) {
        addr = mmap(reinterpret_cast<void*>(image_base), fmt.size_of_image,
                    PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (addr == MAP_FAILED) addr = nullptr;
    }
    if (addr == nullptr) {
        // 32-bit guests need the image below 4GB so guest pointers (which are
        // identity-mapped host addresses) can reach it. Try a set of low
        // candidate bases before falling back to an arbitrary high mapping.
        if (fmt.is_32bit) {
            static const uint64_t low_candidates[] = {
                0x10000000, 0x08000000, 0x04000000, 0x02000000, 0x01000000,
                0x00C00000, 0x00800000, 0x00600000, 0x00400000, 0x00300000,
                0x00200000, 0x00100000, 0x00080000, 0x00060000, 0x00040000,
            };
            for (uint64_t cand : low_candidates) {
                if (cand + fmt.size_of_image > 0xFFFFFFFFULL) continue;
                void* a = mmap(reinterpret_cast<void*>(cand), fmt.size_of_image,
                               PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
                if (a != MAP_FAILED) {
                    addr = a;
                    image_base = cand;
                    break;
                }
            }
            // All fixed candidates taken (usually by low Wine DLLs and prior
            // child copies). For a 32-bit guest, NEVER give up to a high
            // address: sweep the dedicated re-mapped-image zone for a free
            // run. This zone sits above the Wine DLL range (0x01000000-),
            // below the per-thread stack zones that grow down from 0x20000000
            // and 0x18000000, and below the guest heap (0x60000000). Any
            // number of child copies can chain-spawn here.
            if (addr == nullptr) {
                for (uint64_t cand = 0x20000000u;
                     cand + fmt.size_of_image <= 0x60000000u;
                     cand += 0x100000u) {
                    void* a = mmap(reinterpret_cast<void*>(cand), fmt.size_of_image,
                                   PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
                    if (a != MAP_FAILED) {
                        addr = a;
                        image_base = cand;
                        break;
                    }
                }
                if (addr) {
                    fprintf(stderr, "[load_image] swept low zone, mapped at 0x%llX\n",
                            (unsigned long long)image_base);
                }
            }
        }
        if (addr == nullptr) {
            addr = mmap(nullptr, fmt.size_of_image, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (addr == MAP_FAILED) return 0;
            image_base = reinterpret_cast<uint64_t>(addr);
        }
    }

    SectionHeader* sections = reinterpret_cast<SectionHeader*>(
        data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader) + coff->size_of_optional_header);

    size_t headers_size = fmt.size_of_headers;
    mprotect(addr, headers_size, PROT_READ | PROT_WRITE);
    memcpy(addr, data.data(), headers_size);
    mprotect(addr, headers_size, PROT_READ);

    for (uint16_t i = 0; i < coff->number_of_sections; i++) {
        SectionHeader& sec = sections[i];

        bool is_code = (sec.characteristics & 0x20000000) != 0;
        bool is_read = (sec.characteristics & 0x40000000) != 0;
        bool is_write = (sec.characteristics & 0x80000000) != 0;

        // During loading, map everything read/write (plus exec for code) so the
        // loader can write IAT entries and process relocations. Final section
        // protections are applied later by set_final_protections().
        int prot = (is_code ? PROT_EXEC : 0) | PROT_READ | PROT_WRITE;
        if (!is_read && !is_write && !is_code) prot = PROT_NONE;

        void* sec_addr = static_cast<char*>(addr) + sec.virtual_address;
        size_t sec_size = sec.virtual_size ? sec.virtual_size : sec.size_of_raw_data;

        mprotect(sec_addr, sec_size, prot);

        if (sec.size_of_raw_data > 0) {
            memcpy(sec_addr, data.data() + sec.pointer_to_raw_data, sec.size_of_raw_data);
        } else {
            memset(sec_addr, 0, sec_size);
        }
    }

    return image_base;
}

// Apply the final per-section memory protections after loading is complete
void PELoader::set_final_protections(std::vector<uint8_t>& data, uint64_t base_addr) {
    DOSHeader* dos = reinterpret_cast<DOSHeader*>(data.data());
    COFFHeader* coff = reinterpret_cast<COFFHeader*>(data.data() + dos->e_lfanew + 4);
    PeFormat fmt = read_pe_format(data);

    SectionHeader* sections = reinterpret_cast<SectionHeader*>(
        data.data() + dos->e_lfanew + 4 + sizeof(COFFHeader) + coff->size_of_optional_header);

    void* addr = reinterpret_cast<void*>(base_addr);

    size_t headers_size = fmt.size_of_headers;
    mprotect(addr, headers_size, PROT_READ | PROT_WRITE | PROT_EXEC);

    for (uint16_t i = 0; i < coff->number_of_sections; i++) {
        SectionHeader& sec = sections[i];

        // Keep every section writable: guest code (e.g. the delay-load helper
        // and CRT self-modifying tricks) patches IAT slots and data pages at
        // runtime, and those live in nominally read-only sections.
        int prot = PROT_READ | PROT_WRITE;
        if (sec.characteristics & 0x20000000) prot |= PROT_EXEC;

        void* sec_addr = static_cast<char*>(addr) + sec.virtual_address;
        size_t sec_size = sec.virtual_size ? sec.virtual_size : sec.size_of_raw_data;

        mprotect(sec_addr, sec_size, prot);
    }
}

// Write one delay-load IAT entry. The IAT width follows the image's pointer
// size (4 bytes for PE32, 8 bytes for PE32+), which matches the field width
// the guest's delay-load helper expects when it fills slots itself.
static void set_delay_slot(void* image_base, uint32_t iat_rva, size_t i,
                           bool is_32bit, uint64_t value) {
    uint8_t* iat = static_cast<uint8_t*>(image_base) + iat_rva;
    if (is_32bit) {
        reinterpret_cast<uint32_t*>(iat)[i] = static_cast<uint32_t>(value);
    } else {
        reinterpret_cast<uint64_t*>(iat)[i] = value;
    }
}

void PELoader::process_delay_load_imports(void* image_base, std::vector<uint8_t>& data) {
    write(2, "[delay-load] ENTRY\n", 19);
    PeFormat fmt = read_pe_format(data);
    fprintf(stderr, "[delay-load] number_of_rva_and_sizes=%u\n", fmt.number_of_rva_and_sizes);
    if (fmt.number_of_rva_and_sizes <= 13) {
        fprintf(stderr, "[delay-load] skipping: number_of_rva_and_sizes <= 13\n");
        return;
    }

    uint32_t delay_rva = fmt.data_directory[13] & 0xFFFFFFFF;
    uint32_t delay_size = fmt.data_directory[13] >> 32;
    fprintf(stderr, "[delay-load] delay_rva=0x%x, delay_size=0x%x\n", delay_rva, delay_size);
    if (delay_rva == 0 || delay_size == 0) {
        fprintf(stderr, "[delay-load] skipping: delay_rva=0 or delay_size=0\n");
        return;
    }
    // Validate delay-load directory is within image bounds
    if (delay_rva >= fmt.size_of_image || delay_size > fmt.size_of_image - delay_rva) {
        fprintf(stderr, "[delay-load] skipping: delay-load directory out of bounds (rva=0x%x, size=0x%x, image_size=0x%zx)\n",
                delay_rva, delay_size, fmt.size_of_image);
        return;
    }

    uint8_t* delay_desc = static_cast<uint8_t*>(image_base) + delay_rva;

    for (;;) {
        uint32_t gr_attrs = *reinterpret_cast<uint32_t*>(delay_desc);
        uint32_t name_rva = *reinterpret_cast<uint32_t*>(delay_desc + 4);
        uint32_t phmod_rva = *reinterpret_cast<uint32_t*>(delay_desc + 8);
        uint32_t iat_rva = *reinterpret_cast<uint32_t*>(delay_desc + 12);
        uint32_t int_rva = *reinterpret_cast<uint32_t*>(delay_desc + 16);

        fprintf(stderr, "[delay-load] desc: attrs=0x%x, name_rva=0x%x, phmod=0x%x, iat=0x%x, int=0x%x\n",
               gr_attrs, name_rva, phmod_rva, iat_rva, int_rva);

        // Validate delay-load descriptor fields are within image bounds
        auto in_bounds = [&](uint32_t rva) -> bool {
            return rva < fmt.size_of_image;
        };
        if (!in_bounds(name_rva) || !in_bounds(iat_rva) || !in_bounds(int_rva) ||
            !in_bounds(phmod_rva)) {
            fprintf(stderr, "[delay-load] skipping: descriptor fields out of bounds\n");
            delay_desc += 32;
            continue;
        }

        fprintf(stderr, "[delay-load] desc: attrs=0x%x, name_rva=0x%x, phmod=0x%x, iat=0x%x, int=0x%x\n",
               gr_attrs, name_rva, phmod_rva, iat_rva, int_rva);

        if (gr_attrs == 0 && name_rva == 0 && phmod_rva == 0 && iat_rva == 0 && int_rva == 0) break;

        if (name_rva == 0 || int_rva == 0 || iat_rva == 0) {
            delay_desc += 32;
            continue;
        }

        const char* dll_name = reinterpret_cast<const char*>(static_cast<uint8_t*>(image_base) + name_rva);

        // Validate DLL name is a valid Windows DLL name (ASCII, ends with .dll, reasonable length)
        bool valid_dll_name = false;
        if (name_rva < fmt.size_of_image) {
            const char* name_ptr = reinterpret_cast<const char*>(static_cast<uint8_t*>(image_base) + name_rva);
            size_t len = 0;
            while (len < 256 && name_rva + len < fmt.size_of_image) {
                char c = name_ptr[len];
                if (c == 0) break;
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
                    break;
                }
                len++;
            }
            if (len >= 4 && len < 256) {
                // Check if ends with .dll (case insensitive)
                const char* ext = name_ptr + len - 4;
                if (ext >= name_ptr && ext[0] == '.' &&
                    ((ext[1] == 'd' || ext[1] == 'D') && (ext[2] == 'l' || ext[2] == 'L') && (ext[3] == 'l' || ext[3] == 'L'))) {
                    valid_dll_name = true;
                }
            }
        }
        if (!valid_dll_name) {
            fprintf(stderr, "[delay-load] skipping: invalid DLL name at rva=0x%x\n", name_rva);
            delay_desc += 32;
            continue;
        }

        fprintf(stderr, "  delay-load import from %s:\n", dll_name);

// DLLs that we handle internally (built-in implementations) - skip loading Wine versions
        static const char* kBuiltinDlls[] = {
            "advapi32.dll", "kernel32.dll", "user32.dll", "gdi32.dll",
            "ws2_32.dll", "ole32.dll", "oleaut32.dll", "shell32.dll",
            "comdlg32.dll", "version.dll", "winmm.dll", "winspool.drv",
            "ucrtbase.dll", "msvcrt.dll", "vcruntime140.dll", "vcruntime140_1.dll",
            "api-ms-win-crt-runtime-l1-1-0.dll", "api-ms-win-crt-private-l1-1-0.dll",
            "api-ms-win-core-synch-l1-2-0.dll", "api-ms-win-core-heap-l1-1-0.dll",
            "api-ms-win-core-file-l1-2-0.dll", "api-ms-win-core-libraryloader-l1-1-0.dll",
            "api-ms-win-core-processenvironment-l1-1-0.dll", "api-ms-win-core-rtlsupport-l1-1-0.dll",
            "api-ms-win-core-errorhandling-l1-1-0.dll", "api-ms-win-core-profile-l1-1-0.dll",
            "api-ms-win-core-sysinfo-l1-1-0.dll", "api-ms-win-core-localization-l1-2-0.dll",
            "api-ms-win-core-timezone-l1-1-0.dll", "api-ms-win-core-debug-l1-1-0.dll",
            "api-ms-win-core-handle-l1-1-0.dll", "api-ms-win-core-threadpool-l1-2-0.dll",
            "api-ms-win-core-fibers-l1-1-1.dll", "api-ms-win-core-processthreads-l1-1-0.dll",
            "api-ms-win-core-synch-l1-1-0.dll", "api-ms-win-core-sysinfo-l1-2-0.dll",
            nullptr
        };

        // Check if this is a builtin DLL we handle internally
        bool is_builtin = false;
        for (const char** b = kBuiltinDlls; *b; ++b) {
            if (strcasecmp(dll_name, *b) == 0) {
                is_builtin = true;
                break;
            }
        }

        if (is_builtin) {
            fprintf(stderr, "[delay-load] %s is a builtin DLL, using internal stubs\n", dll_name);
        } else {
            // Try to load the DLL to verify it exists and can be loaded
            LoadedModule* dep_module = find_module_by_dll_name(dll_name);
            if (!dep_module) {
                // Add the main image's directory to search paths (where the EXE lives)
                const std::string& main_img_dir = get_main_image_dir();
                char* wine_prefix = getenv("WINEPREFIX");
                // Search ThetaZ's internal prefix FIRST (system32 for 64-bit, syswow64 for 32-bit),
                // then Wine directories as fallback. Loading a PE32+ DLL into a 32-bit guest
                // (or a PE32 DLL into a 64-bit guest) is never valid: the interpreter cannot
                // run 64-bit code, and a 64-bit guest cannot jump to 32-bit code.
                const char* wine_windows = fmt.is_32bit
                    ? "/usr/lib/wine/i386-windows"
                    : "/usr/lib/wine/x86_64-windows";
                const char* wine_unix = fmt.is_32bit
                    ? "/usr/lib/wine/i386-unix"
                    : "/usr/lib/wine/x86_64-unix";

                // Build ThetaZ prefix path
                std::string thetaz_sys_dir;
                if (wine_prefix) {
                    thetaz_sys_dir = std::string(wine_prefix) + "/drive_c/windows/" + (fmt.is_32bit ? "syswow64" : "system32");
                }

                const char* search_dirs[] = {
                    ".",
                    main_img_dir.empty() ? nullptr : main_img_dir.c_str(),
                    thetaz_sys_dir.empty() ? nullptr : thetaz_sys_dir.c_str(),
                    wine_windows,
                    wine_unix,
                    "/usr/lib",
                    "/usr/lib64",
                    "/lib",
                    "/lib64",
                };
                std::string full_path;
                for (const char* dir : search_dirs) {
                    if (!dir) continue;
                    // Try exact case first
                    std::string candidate = std::string(dir) + "/" + dll_name;
                    fprintf(stderr, "[delay-load] searching exact: %s\n", candidate.c_str());
                    if (access(candidate.c_str(), F_OK) == 0) {
                        fprintf(stderr, "[delay-load] found %s at %s\n", dll_name, candidate.c_str());
                        full_path = candidate;
                        break;
                    }
                    // Try case-insensitive search in the directory
                    std::string ci_path = find_file_case_insensitive(dir, dll_name);
                    if (!ci_path.empty()) {
                        fprintf(stderr, "[delay-load] found %s at %s (case-insensitive)\n", dll_name, ci_path.c_str());
                        full_path = ci_path;
                        break;
                    }
                    // Try lowercase version (Linux filesystems are case-sensitive)
                    std::string candidate_lower = std::string(dir) + "/";
                    for (size_t j = 0; dll_name[j]; ++j) {
                        candidate_lower += std::tolower(static_cast<unsigned char>(dll_name[j]));
                    }
                    if (access(candidate_lower.c_str(), F_OK) == 0) {
                        fprintf(stderr, "[delay-load] found %s at %s (lowercase)\n", dll_name, candidate_lower.c_str());
                        full_path = candidate_lower;
                        break;
                    }
                }
                if (full_path.empty()) {
                    fprintf(stderr, "[delay-load] skipping: DLL %s not found in search paths\n", dll_name);
                    delay_desc += 32;
                    continue;
                }
                fprintf(stderr, "[delay-load] calling load_image for %s\n", full_path.c_str());
                LoadedModule* dep_module = load_image(full_path.c_str());
                fprintf(stderr, "[delay-load] load_image returned for %s\n", full_path.c_str());
                if (!dep_module) {
                    fprintf(stderr, "[delay-load] skipping: failed to load dependency %s\n", dll_name);
                    delay_desc += 32;
                    continue;
                }
                add_module_to_ldr(dep_module->base, dep_module->name);
            }
        }

        fprintf(stderr, "  delay-load import from %s:\n", dll_name);

        // INT entries are pointer-sized (32-bit for PE32, 64-bit for PE32+).
        // IAT entry width also matches the image's pointer size.
        void* int_ptr_void = static_cast<uint8_t*>(image_base) + int_rva;

        for (size_t i = 0; ; i++) {
            uint64_t entry;
            if (fmt.is_32bit) {
                uint32_t* int_ptr = reinterpret_cast<uint32_t*>(int_ptr_void);
                if (int_ptr[i] == 0) break;
                entry = int_ptr[i];
            } else {
                uint64_t* int_ptr = reinterpret_cast<uint64_t*>(int_ptr_void);
                if (int_ptr[i] == 0) break;
                entry = int_ptr[i];
            }

            void* resolved = nullptr;
            const char* display = nullptr;
            const char* func_name = nullptr;
            bool from_builtin = false;

            // High bit indicates ordinal import (bit 31 for PE32, bit 63 for PE32+)
            uint64_t ordinal_mask = fmt.is_32bit ? 0x80000000ULL : 0x8000000000000000ULL;
            uint64_t rva_mask = fmt.is_32bit ? 0x7FFFFFFFULL : 0x7FFFFFFFFFFFFFFFULL;

            if (entry & ordinal_mask) {
                uint16_t ordinal = static_cast<uint16_t>(entry & 0xFFFF);
                char ordinal_buf[16];
                snprintf(ordinal_buf, sizeof(ordinal_buf), "#%u", ordinal);
                display = ordinal_buf;

                LoadedModule* dep = find_module_by_dll_name(dll_name);
                if (dep) {
                    resolved = resolve_export(*dep, ordinal, "", 0);
                }
                if (!resolved) {
                    resolved = reinterpret_cast<void*>(ApiRegistry::instance().find_function_ordinal(dll_name, ordinal));
                    from_builtin = true;
                }
            } else {
                uint64_t name_rva = entry & rva_mask;
                func_name = reinterpret_cast<const char*>(static_cast<uint8_t*>(image_base) + name_rva + 2);
                display = func_name;
                // First try the loaded module's own export table (for Office DLLs, etc.)
                LoadedModule* dep = find_module_by_dll_name(dll_name);
                if (dep) {
                    resolved = resolve_export(*dep, 0, func_name, 0);
                }
                if (!resolved) {
                    resolved = reinterpret_cast<void*>(ApiRegistry::instance().find_function(dll_name, func_name));
                    from_builtin = true;
                }
            }

            if (!resolved) {
                fprintf(stderr, "    !! unresolved: %s\n", display);
                uint64_t generic = alloc_api_thunk((void*)ThetaZ::generic_api_stub, 0, true, dll_name, display);
                set_delay_slot(image_base, iat_rva, i, fmt.is_32bit, generic);
            } else if (fmt.is_32bit) {
                // 32-bit guests cannot call host function pointers directly:
                // route builtin resolves through a thunk. A real dependency
                // export (from an i386 Wine DLL) is a genuine guest address in
                // the low 4GB, so it can be written directly as a 32-bit slot.
                uint64_t slot = 0;
                if (from_builtin) {
                    slot = alloc_api_thunk(resolved, 0, true, dll_name, display);
                    if (!slot) {
                        fprintf(stderr, "    !! thunk alloc failed: %s\n", display);
                        set_delay_slot(image_base, iat_rva, i, fmt.is_32bit, 0);
                        continue;
                    }
                } else {
                    slot = reinterpret_cast<uint64_t>(resolved);
                    if (slot > 0xFFFFFFFFULL) {
                        fprintf(stderr, "    !! resolved export above 4GB (bad for 32-bit guest): %s\n", display);
                        set_delay_slot(image_base, iat_rva, i, fmt.is_32bit, 0);
                        continue;
                    }
                }
                fprintf(stderr, "    %s -> 0x%08llx (delay slot %p)\n", display,
                        static_cast<unsigned long long>(slot),
                        static_cast<uint8_t*>(image_base) + iat_rva + i * 4);
                set_delay_slot(image_base, iat_rva, i, true, slot);
            } else {
                // 64-bit guest: real exports are called directly; builtin SysV
                // host functions need a register-shuffling ms_abi shim.
                uint64_t slot = reinterpret_cast<uint64_t>(resolved);
                if (from_builtin) {
                    slot = ThetaZ::x86_alloc64_shim(resolved, dll_name, func_name);
                    if (!slot) {
                        fprintf(stderr, "    !! shim alloc failed: %s\n", display);
                        set_delay_slot(image_base, iat_rva, i, false, 0);
                        continue;
                    }
                }
                fprintf(stderr, "    %s -> %p (delay slot %p)\n", display, resolved,
                        static_cast<uint8_t*>(image_base) + iat_rva + i * 8);
                set_delay_slot(image_base, iat_rva, i, false, slot);
            }
        }

        delay_desc += 32;
    }
}

void PELoader::apply_relocations(void* image_base, std::vector<uint8_t>& data, uint64_t delta) {
    if (delta == 0) return;

    PeFormat fmt = read_pe_format(data);
    if (fmt.number_of_rva_and_sizes <= 5) return;

    uint32_t reloc_rva = fmt.data_directory[5] & 0xFFFFFFFF;
    uint32_t reloc_size = fmt.data_directory[5] >> 32;

    if (reloc_rva == 0 || reloc_size == 0) return;

    uint8_t* reloc_base = static_cast<uint8_t*>(image_base) + reloc_rva;
    uint8_t* reloc_end = reloc_base + reloc_size;

    while (reloc_base < reloc_end) {
        uint32_t page_rva = *reinterpret_cast<uint32_t*>(reloc_base);
        uint32_t block_size = *reinterpret_cast<uint32_t*>(reloc_base + 4);

        if (block_size == 0) break;

        uint16_t* entries = reinterpret_cast<uint16_t*>(reloc_base + 8);
        uint32_t num_entries = (block_size - 8) / 2;

        for (uint32_t i = 0; i < num_entries; i++) {
            uint16_t entry = entries[i];
            uint16_t type = entry >> 12;
            uint16_t offset = entry & 0xFFF;

            if (type == 0) continue;

            uint8_t* target = static_cast<uint8_t*>(image_base) + page_rva + offset;

            if (fmt.is_32bit) {
                // HIGHLOW (type 3): add delta to a 32-bit pointer.
                *reinterpret_cast<uint32_t*>(target) += static_cast<uint32_t>(delta);
            } else {
                if (type == 10) {
                    *reinterpret_cast<uint64_t*>(target) += delta;
                } else if (type == 3) {
                    *reinterpret_cast<uint32_t*>(target) += static_cast<uint32_t>(delta);
                }
            }
        }

        reloc_base += block_size;
    }
}

void PELoader::process_imports(void* image_base, std::vector<uint8_t>& data) {
    DOSHeader* dos = reinterpret_cast<DOSHeader*>(data.data());
    PeFormat fmt = read_pe_format(data);

    if (fmt.number_of_rva_and_sizes <= 1) return;

    uint32_t import_rva = fmt.data_directory[1] & 0xFFFFFFFF;
    if (import_rva == 0) return;

    uint8_t* import_desc = static_cast<uint8_t*>(image_base) + import_rva;
    ApiRegistry& api = ApiRegistry::instance();

    for (;;) {
        uint32_t original_first_thunk = *reinterpret_cast<uint32_t*>(import_desc);
        uint32_t name_rva = *reinterpret_cast<uint32_t*>(import_desc + 12);
        uint32_t first_thunk = *reinterpret_cast<uint32_t*>(import_desc + 16);

        if (original_first_thunk == 0 && name_rva == 0 && first_thunk == 0) break;

        if (name_rva != 0) {
            const char* dll_name = reinterpret_cast<const char*>(image_base) + name_rva;
            printf("  import from %s:\n", dll_name);

            uint32_t thunk_rva = first_thunk ? first_thunk : original_first_thunk;
            uint32_t lookup_rva = original_first_thunk ? original_first_thunk : first_thunk;

            if (fmt.is_32bit) {
                uint32_t* thunk = reinterpret_cast<uint32_t*>(
                    static_cast<uint8_t*>(image_base) + thunk_rva);
                uint32_t* lookup = reinterpret_cast<uint32_t*>(
                    static_cast<uint8_t*>(image_base) + lookup_rva);

                for (int i = 0; lookup[i] != 0; i++) {
                    uint32_t entry = lookup[i];
                    void* resolved = nullptr;
                    const char* display = nullptr;

                    // Check API registry FIRST (our builtin implementations take precedence over Wine DLLs)
                    static const auto is_builtin_dll = [](const std::string& d) -> bool {
                        std::string lower = d;
                        std::transform(lower.begin(), lower.end(), lower.begin(),
                                       [](unsigned char c) { return std::tolower(c); });
                        // Strip .dll extension for extensionless imports (e.g. "ucrtbase")
                        if (lower.size() >= 4 && lower.substr(lower.size()-4) == ".dll")
                            lower.resize(lower.size()-4);
                        return lower == "kernel32" || lower == "kernelbase" ||
                               lower == "ntdll" || lower == "user32" ||
                               lower == "gdi32" || lower == "advapi32" ||
                               lower == "ole32" || lower == "oleaut32" ||
                               lower == "shell32" || lower == "ws2_32" ||
                               lower == "version" || lower == "shlwapi" ||
                               lower == "ucrtbase" || lower == "msvcrt" ||
                               lower == "vcruntime140" || lower == "vcruntime140_1";
                    }(dll_name);

                    LoadedModule* dep = nullptr;

                    if (entry & 0x80000000ULL) {
                        uint16_t ordinal = static_cast<uint16_t>(entry & 0xFFFF);
                        char ordinal_buf[16];
                        snprintf(ordinal_buf, sizeof(ordinal_buf), "#%u", ordinal);
                        display = ordinal_buf;

                        // For builtin DLLs, ONLY use API registry - never load Wine DLLs
                        if (is_builtin_dll) {
                            resolved = reinterpret_cast<void*>(api.find_function_ordinal(dll_name, ordinal));
                        } else {
                            dep = find_module_by_dll_name(dll_name);
                            if (dep) {
                                resolved = resolve_export(*dep, ordinal, "", 0);
                            }
                            if (!resolved) {
                                resolved = reinterpret_cast<void*>(api.find_function_ordinal(dll_name, ordinal));
                            }
                            if (!resolved) {
                                dep = try_load_from_thetaz_prefix(dll_name, fmt.is_32bit);
                                if (dep) {
                                    resolved = resolve_export(*dep, ordinal, "", 0);
                                }
                            }
                        }
                    } else {
                        const char* func_name = reinterpret_cast<const char*>(image_base) + (entry & 0x7FFFFFFF) + 2;
                        display = func_name;

                        // For builtin DLLs, ONLY use API registry - never load Wine DLLs
                        if (is_builtin_dll) {
                            resolved = reinterpret_cast<void*>(api.find_function(dll_name, func_name));
                        } else {
                            dep = find_module_by_dll_name(dll_name);
                            if (dep) {
                                resolved = resolve_export(*dep, 0, func_name, 0);
                            }
                            if (!resolved) {
                                resolved = reinterpret_cast<void*>(api.find_function(dll_name, func_name));
                            }
                            if (!resolved) {
                                dep = try_load_from_thetaz_prefix(dll_name, fmt.is_32bit);
                                if (dep) {
                                    resolved = resolve_export(*dep, 0, func_name, 0);
                                }
                            }
                        }
                    }

                    if (!resolved) {
                        printf("    !! unresolved: %s (using generic stub)\n", display);
                        uint32_t thunk_addr = alloc_api_thunk((void*)ThetaZ::generic_api_stub, 0, true,
                                                              dll_name, display);
                        if (thunk_addr) {
                            printf("    %s -> generic thunk 0x%08x\n", display, thunk_addr);
                            thunk[i] = thunk_addr;
                        } else {
                            printf("    !! generic thunk alloc failed: %s\n", display);
                            thunk[i] = 0;
                        }
                    } else {
                        // 32-bit guests cannot call host function pointers
                        // directly: write the guest address of an API thunk
                        // into the IAT slot. The interpreter detects the thunk
                        // and marshals the call via x86_dispatch_api.
                        uint32_t thunk_addr = alloc_api_thunk(resolved, 0, true,
                                                              dll_name, display);
                        if (thunk_addr) {
                            printf("    %s -> thunk 0x%08x\n", display, thunk_addr);
                            thunk[i] = thunk_addr;
                        } else {
                            printf("    !! thunk alloc failed: %s\n", display);
                            thunk[i] = 0;
                        }
                    }
                }
            } else {
                uint64_t* thunk = reinterpret_cast<uint64_t*>(
                    static_cast<uint8_t*>(image_base) + thunk_rva);
                uint64_t* lookup = reinterpret_cast<uint64_t*>(
                    static_cast<uint8_t*>(image_base) + lookup_rva);

                for (int i = 0; lookup[i] != 0; i++) {
                    uint64_t entry = lookup[i];
                    void* resolved = nullptr;
                    const char* display = nullptr;
                    const char* func_name = nullptr;
                    bool from_builtin = false; // host SysV fn -> needs an ms_abi shim
                    LoadedModule* dep = nullptr;

                    static const auto is_builtin_dll = [](const std::string& d) -> bool {
                        std::string lower = d;
                        std::transform(lower.begin(), lower.end(), lower.begin(),
                                       [](unsigned char c) { return std::tolower(c); });
                        if (lower.size() >= 4 && lower.substr(lower.size()-4) == ".dll")
                            lower.resize(lower.size()-4);
                        return lower == "kernel32" || lower == "kernelbase" ||
                               lower == "ntdll" || lower == "user32" ||
                               lower == "gdi32" || lower == "advapi32" ||
                               lower == "ole32" || lower == "oleaut32" ||
                               lower == "shell32" || lower == "ws2_32" ||
                               lower == "version" || lower == "shlwapi" ||
                               lower == "ucrtbase" || lower == "msvcrt" ||
                               lower == "vcruntime140" || lower == "vcruntime140_1";
                    }(dll_name);

                    if (entry & 0x8000000000000000ULL) {
                        uint16_t ordinal = static_cast<uint16_t>(entry & 0xFFFF);
                        char ordinal_buf[16];
                        snprintf(ordinal_buf, sizeof(ordinal_buf), "#%u", ordinal);
                        display = ordinal_buf;

                        // For builtin DLLs, ONLY use API registry - never load Wine DLLs
                        if (is_builtin_dll) {
                            resolved = reinterpret_cast<void*>(api.find_function_ordinal(dll_name, ordinal));
                        } else {
                            dep = find_module_by_dll_name(dll_name);
                            if (dep) {
                                resolved = resolve_export(*dep, ordinal, "", 0);
                            }
                            if (!resolved) {
                                resolved = reinterpret_cast<void*>(api.find_function_ordinal(dll_name, ordinal));
                            }
                            if (!resolved) {
                                dep = try_load_from_thetaz_prefix(dll_name, fmt.is_32bit);
                                if (dep) {
                                    resolved = resolve_export(*dep, ordinal, "", 0);
                                }
                            }
                        }
                        from_builtin = is_builtin_dll || (resolved != nullptr && api.find_function_ordinal(dll_name, ordinal));
                    } else {
func_name = reinterpret_cast<const char*>(image_base) + (entry & 0x7FFFFFFFFFFFFFFFULL) + 2;
                        display = func_name;

                        // For builtin DLLs, ONLY use API registry - never load Wine DLLs
                        if (is_builtin_dll) {
                            resolved = reinterpret_cast<void*>(api.find_function(dll_name, func_name));
                            from_builtin = true;
                        } else {
                            bool from_registry = false;
                            dep = find_module_by_dll_name(dll_name);
                            if (dep) {
                                resolved = resolve_export(*dep, 0, func_name, 0);
                            }
                            if (!resolved) {
                                resolved = reinterpret_cast<void*>(api.find_function(dll_name, func_name));
                                from_registry = (resolved != nullptr);
                            }
                            if (!resolved) {
                                dep = try_load_from_thetaz_prefix(dll_name, fmt.is_32bit);
                                if (dep) {
                                    resolved = resolve_export(*dep, 0, func_name, 0);
                                }
                            }
                            from_builtin = is_builtin_dll || from_registry;
                        }
                    }

                    if (!resolved) {
                        printf("    !! unresolved: %s (using generic shim)\n", display);
                        uint64_t slot = ThetaZ::x86_alloc64_shim((void*)ThetaZ::generic_api_stub, dll_name, display);
                        if (slot) {
                            printf("    %s -> generic shim 0x%016lx\n", display, slot);
                            thunk[i] = slot;
                        } else {
                            printf("    !! generic shim alloc failed: %s\n", display);
                            thunk[i] = 0;
                        }
                    } else {
                        // Host (SysV) functions cannot be called with the guest's
                        // MS x64 ABI: route them through a register-shuffling
                        // shim. Real guest DLL exports need no shim.
                        uint64_t slot = reinterpret_cast<uint64_t>(resolved);
                        if (from_builtin) {
                            slot = ThetaZ::x86_alloc64_shim(resolved, dll_name, func_name);
                            if (!slot) {
                                printf("    !! shim alloc failed: %s\n", display);
                                thunk[i] = 0;
                                continue;
                            }
                        }
                        printf("    %s -> %p (slot %p)\n", display, resolved, &thunk[i]);
                        thunk[i] = slot;
                    }
                }
            }
        }

        import_desc += 20;
    }
}

void PELoader::process_exports(void* image_base, std::vector<uint8_t>& data, LoadedModule& module) {
    DOSHeader* dos = reinterpret_cast<DOSHeader*>(data.data());
    PeFormat fmt = read_pe_format(data);

    if (fmt.number_of_rva_and_sizes <= 0) return;

    uint32_t export_rva = fmt.data_directory[0] & 0xFFFFFFFF;
    uint32_t export_size = fmt.data_directory[0] >> 32;
    if (export_rva == 0) return;

    uint8_t* image = static_cast<uint8_t*>(image_base);
    IMAGE_EXPORT_DIRECTORY* exp = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(image + export_rva);

    module.export_dir_rva = export_rva;
    module.export_dir_size = export_size;

    uint32_t* functions = reinterpret_cast<uint32_t*>(image + exp->address_of_functions);
    uint32_t* names = reinterpret_cast<uint32_t*>(image + exp->address_of_names);
    uint16_t* ordinals = reinterpret_cast<uint16_t*>(image + exp->address_of_name_ordinals);

    auto is_forwarder = [&](uint32_t rva) {
        return rva >= export_rva && rva < export_rva + export_size;
    };

    for (uint32_t i = 0; i < exp->number_of_functions; i++) {
        if (functions[i] == 0) continue;
        uint16_t ordinal = static_cast<uint16_t>(exp->base + i);
        if (is_forwarder(functions[i])) {
            module.forwarders_by_ordinal[ordinal] =
                reinterpret_cast<const char*>(image + functions[i]);
        } else {
            module.exports_by_ordinal[ordinal] = functions[i];
        }
    }

    for (uint32_t i = 0; i < exp->number_of_names; i++) {
        const char* name = reinterpret_cast<const char*>(image + names[i]);
        if (ordinals[i] >= exp->number_of_functions) continue;
        uint32_t func_rva = functions[ordinals[i]];
        if (func_rva == 0) continue;
        if (is_forwarder(func_rva)) {
            module.forwarders[name] = reinterpret_cast<const char*>(image + func_rva);
        } else {
            module.exports_by_name[name] = func_rva;
        }
    }
}

// Locate a loaded module by its DLL basename ("kernel32.dll") independent of the
// full path used as the module key.
LoadedModule* PELoader::find_module_by_dll_name(const std::string& dll_name) {
    std::string lower = dll_name;
    for (auto& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    for (auto& entry : modules_) {
        std::string key = entry.second.name;
        for (auto& c : key) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        size_t slash = key.find_last_of("/\\");
        std::string base = (slash == std::string::npos) ? key : key.substr(slash + 1);
        if (base == lower) return &entry.second;
    }
    return nullptr;
}

// Try to load a DLL from ThetaZ's internal prefix (system32 for 64-bit, syswow64 for 32-bit)
// Returns the LoadedModule if successful, nullptr otherwise.
// Does NOT load builtin DLLs (kernel32, user32, etc.) - those use the API registry.
LoadedModule* PELoader::try_load_from_thetaz_prefix(const std::string& dll_name, bool is_32bit) {
    // First check if already loaded (or being loaded) to avoid recursion
    LoadedModule* existing = find_module_by_dll_name(dll_name);
    if (existing) return existing;

    // Skip builtin DLLs entirely - they use the API registry
    static const std::unordered_set<std::string> builtin_dlls = {
        "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll",
        "gdi32.dll", "advapi32.dll", "ole32.dll", "oleaut32.dll",
        "shell32.dll", "ws2_32.dll", "version.dll", "shlwapi.dll",
        "msvcrt.dll", "ucrtbase.dll", "vcruntime140.dll"
    };

    std::string lower = dll_name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (builtin_dlls.count(lower)) {
        fprintf(stderr, "[import-fallback] skipping builtin DLL: %s\n", dll_name.c_str());
        return nullptr;
    }

    char* wine_prefix = getenv("WINEPREFIX");

    // Build a list of directories to search: main image dir first (where the
    // exe lives), then the Wine prefix system dirs.  Windows DLL search order
    // starts with the directory containing the calling image.
    std::vector<std::string> search_dirs;

    const std::string& main_dir = get_main_image_dir();
    if (!main_dir.empty()) {
        search_dirs.push_back(main_dir);
    }

    if (wine_prefix) {
        std::string sys_dir = std::string(wine_prefix) + "/drive_c/windows/" + (is_32bit ? "syswow64" : "system32");
        search_dirs.push_back(sys_dir);
    }

    for (const auto& dir : search_dirs) {
        // Try exact case first
        std::string full_path = dir + "/" + dll_name;
        if (access(full_path.c_str(), F_OK) == 0) {
            fprintf(stderr, "[import-fallback] loading %s from %s\n", dll_name.c_str(), full_path.c_str());
            return load_image(full_path.c_str());
        }
        // Try case-insensitive
        std::string ci_path = find_file_case_insensitive(dir, dll_name);
        if (!ci_path.empty()) {
            fprintf(stderr, "[import-fallback] loading %s from %s (ci)\n", dll_name.c_str(), ci_path.c_str());
            return load_image(ci_path.c_str());
        }
    }
    return nullptr;
}

// Resolve an export identified by ordinal and/or name. Follows forwarders
// recursively with a depth guard against circular forwarding.
void* PELoader::resolve_export(LoadedModule& module, uint16_t ordinal,
                               const std::string& name, size_t depth) {
    if (depth > 32) return nullptr;

    auto try_fwd = [&](const std::string& fwd) -> void* {
        void* p = resolve_forwarder(fwd, depth);
        if (p) return p;
        // A forwarder that names a module already handled by the API layer
        // (e.g. ntdll -> our builtin) is resolved by ApiRegistry.
        return nullptr;
    };

    if (!name.empty()) {
        auto it = module.exports_by_name.find(name);
        if (it != module.exports_by_name.end()) {
            return reinterpret_cast<void*>(module.base + it->second);
        }
        auto fit = module.forwarders.find(name);
        if (fit != module.forwarders.end()) return try_fwd(fit->second);
    }
    if (ordinal != 0) {
        auto it = module.exports_by_ordinal.find(ordinal);
        if (it != module.exports_by_ordinal.end()) {
            return reinterpret_cast<void*>(module.base + it->second);
        }
        auto fit = module.forwarders_by_ordinal.find(ordinal);
        if (fit != module.forwarders_by_ordinal.end()) return try_fwd(fit->second);
    }
    return nullptr;
}

// Resolve a forwarder string ("dllname.dll!Function" or "dllname.dll.Function")
// against the API registry and any loaded modules.
void* PELoader::resolve_forwarder(const std::string& forwarder_str, size_t depth) {
    if (depth > 32) return nullptr;

    const char* forwarder = forwarder_str.c_str();
    const char* sep = strchr(forwarder, '!');
    const char* dot = sep ? nullptr : strrchr(forwarder, '.');
    const char* delim = sep ? sep : dot;
    if (!delim) return nullptr;

    std::string dll(forwarder, delim - forwarder);
    const char* func = delim + 1;

    std::string dll_lower = dll;
    for (auto& c : dll_lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    // Ordinal forwarder form: "dll.dll.#123"
    if (func[0] == '#') {
        unsigned long ord = strtoul(func + 1, nullptr, 10);
        if (ord != 0 && ord < 0x10000) {
            if (void* f = ApiRegistry::instance().find_function(dll_lower, func)) return f;
            LoadedModule* target = find_module_by_dll_name(dll_lower);
            if (target) return resolve_export(*target, static_cast<uint16_t>(ord), "", depth + 1);
        }
        return nullptr;
    }

    if (void* f = ApiRegistry::instance().find_function(dll_lower, func)) {
        return f;
    }

    LoadedModule* target = find_module_by_dll_name(dll_lower);
    if (target) {
        return resolve_export(*target, 0, func, depth + 1);
    }
    return nullptr;
}

LoadedModule* PELoader::load_image(const char* path, uint64_t preferred_base) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::vector<uint8_t> data;
    if (!read_file(path, data)) {
        fprintf(stderr, "PELoader: cannot read file %s\n", path);
        return nullptr;
    }

    if (!validate_pe(data)) {
        fprintf(stderr, "PELoader: not a valid PE file: %s\n", path);
        return nullptr;
    }

    DOSHeader* dos = reinterpret_cast<DOSHeader*>(data.data());
    COFFHeader* coff = reinterpret_cast<COFFHeader*>(data.data() + dos->e_lfanew + 4);
    PeFormat fmt = read_pe_format(data);

    uint64_t want = preferred_base ? preferred_base : fmt.image_base;
    bool conflict = false;
    for (const auto& e : modules_) {
        uint64_t b = e.second.base;
        if (want < b + e.second.size && b < want + fmt.size_of_image) {
            conflict = true;
            break;
        }
    }
    // With an explicit preferred base, map at EXACTLY that address and fail if
    // it is taken. The caller (CreateProcess relaunch loop) iterates candidate
    // bases, so a conflict must return failure rather than silently falling
    // back to a low candidate or high address.
    if (preferred_base != 0 && conflict) {
        return nullptr;
    }
    uint64_t base = map_sections(data, want, conflict);
    if (base == 0) {
        fprintf(stderr, "PELoader: failed to map image: %s\n", path);
        return nullptr;
    }

    uint64_t delta = base - fmt.image_base;
    apply_relocations(reinterpret_cast<void*>(base), data, delta);

    // Build the module struct, parse exports, and insert into modules_ BEFORE
    // processing imports.  This mirrors the Windows loader's "in-load" state:
    // a DLL is visible to other loaders (find_module_by_dll_name) before its
    // own imports are resolved, which breaks circular dependencies (e.g.
    // winmm.dll ↔ msacm32.dll).  Without this, process_imports for a
    // dependency would call try_load_from_thetaz_prefix for the parent DLL,
    // find it absent from modules_, and recurse infinitely.
    LoadedModule module{};
    strncpy(module.name, path, sizeof(module.name) - 1);
    module.base = base;
    module.size = fmt.size_of_image;
    module.mapped_data = reinterpret_cast<void*>(base);
    module.entry_point = base + fmt.address_of_entry_point;
    module.is_dll = (coff->characteristics & 0x2000) != 0;
    module.is_32bit = fmt.is_32bit;

    fprintf(stderr, "[load_image] calling process_exports (before imports for cycle detection)\n");
    process_exports(reinterpret_cast<void*>(base), data, module);
    fprintf(stderr, "[load_image] process_exports done for %s base=0x%llx\n", path, (unsigned long long)base);
    fflush(stderr);

    // Insert into modules_ so find_module_by_dll_name can find us during
    // recursive import resolution.  Exports are already populated above.
    fprintf(stderr, "[load_image] inserting module base=0x%llx name=%s (before imports)\n", (unsigned long long)base, path);
    fflush(stderr);
    modules_[module.base] = module;
    fprintf(stderr, "[load_image] module inserted, calling add_module_to_ldr\n");
    fflush(stderr);

    // Register PE TLS before imports resolve: the EXE should normally receive
    // TLS index 0, and every later thread must see its raw template.  The TLS
    // directory contains absolute VAs; children are relocated, so normalize
    // values that still point into the original image range.
    if (fmt.is_32bit && fmt.number_of_rva_and_sizes > 9) {
        uint32_t tls_rva = static_cast<uint32_t>(fmt.data_directory[9] & 0xFFFFFFFF);
        uint32_t tls_size = static_cast<uint32_t>(fmt.data_directory[9] >> 32);
        if (tls_rva != 0) {
            if (tls_size >= sizeof(IMAGE_TLS_DIRECTORY32) &&
                tls_rva + sizeof(IMAGE_TLS_DIRECTORY32) <= fmt.size_of_image) {
                auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY32*>(base + tls_rva);
                auto normalize_va = [&](uint32_t va) -> uint32_t {
                    if (va == 0) return 0;
                    if (va >= base && va < base + fmt.size_of_image) return va;
                    if (va >= fmt.image_base &&
                        va < fmt.image_base + fmt.size_of_image) {
                        uint64_t adjusted = (va - fmt.image_base) + base;
                        if (adjusted < 0x100000000ULL)
                            return static_cast<uint32_t>(adjusted);
                    }
                    return va;
                };
                uint32_t raw_start = normalize_va(tls->start_address_of_raw_data);
                uint32_t raw_end = normalize_va(tls->end_address_of_raw_data);
                uint32_t index_va = normalize_va(tls->address_of_index);
                uint32_t callbacks = normalize_va(tls->address_of_callbacks);
                if (!x86_tls_register_module(
                        static_cast<uint32_t>(base), fmt.size_of_image,
                        raw_start, raw_end, index_va, callbacks,
                        tls->size_of_zero_fill)) {
                    fprintf(stderr, "[load_image] TLS registration failed for %s\n", path);
                }
            } else {
                fprintf(stderr,
                        "[load_image] invalid TLS directory rva=0x%x size=0x%x for %s\n",
                        tls_rva, tls_size, path);
            }
        }
    }

    // Add to PEB LDR module lists (also before imports, for consistency)
    add_module_to_ldr(module.base, module.name);
    fprintf(stderr, "[load_image] add_module_to_ldr done\n");
    fflush(stderr);

    printf("PELoader: loading %s (%s)\n", path, fmt.is_32bit ? "PE32/32-bit" : "PE32+/64-bit");
    process_imports(reinterpret_cast<void*>(base), data);
    process_delay_load_imports(reinterpret_cast<void*>(base), data);

    fprintf(stderr, "[load_image] calling set_final_protections base=0x%llx\n", (unsigned long long)base);
    set_final_protections(data, base);
    fprintf(stderr, "[load_image] set_final_protections done\n");

    return &modules_[module.base];
}

void PELoader::unload_image(uint64_t base) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = modules_.find(base);
    if (it != modules_.end()) {
        munmap(it->second.mapped_data, it->second.size);
        modules_.erase(it);
    }
}

LoadedModule* PELoader::find_module(const char* name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!name) return nullptr;
    for (auto& entry : modules_) {
        if (entry.second.name == std::string(name)) return &entry.second;
    }
    return nullptr;
}

LoadedModule* PELoader::find_module_by_base(uint64_t base) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = modules_.find(base);
    if (it != modules_.end()) return &it->second;
    return nullptr;
}

LoadedModule* PELoader::find_module_named(const char* name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!name) return nullptr;

    std::string lower = name;
    for (auto& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    for (auto& entry : modules_) {
        std::string key = entry.second.name;
        for (auto& c : key) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        size_t slash = key.find_last_of("/\\");
        std::string base = (slash == std::string::npos) ? key : key.substr(slash + 1);
        if (base == lower) return &entry.second;
    }
    return nullptr;
}

void* PELoader::get_export_address(LoadedModule& module, uint16_t ordinal,
                                   const char* name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return resolve_export(module, ordinal, name ? std::string(name) : std::string(), 0);
}

PELoader& PELoader::instance() {
    static PELoader loader;
    return loader;
}

void* PELoader::get_proc_address(const char* module_name, const char* proc_name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Windows convention: an ordinal is passed when the value fits in 16 bits.
    uintptr_t as_int = reinterpret_cast<uintptr_t>(proc_name);
    if (as_int < 0x10000) {
        LoadedModule* module = find_module_by_dll_name(module_name);
        if (!module) return nullptr;
        return resolve_export(*module, static_cast<uint16_t>(as_int), "", 0);
    }

    LoadedModule* module = find_module_by_dll_name(module_name);
    if (!module) return nullptr;
    return resolve_export(*module, 0, proc_name, 0);
}

uint64_t PELoader::get_entry_point(const char* module_name) {
    LoadedModule* module = find_module(module_name);
    if (!module) return 0;
    return module->entry_point;
}

void PELoader::print_loaded_modules() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    printf("Loaded modules:\n");
    for (const auto& entry : modules_) {
        printf("  %s @ 0x%016lX (%lu bytes, entry 0x%016lX)\n",
               entry.second.name, entry.second.base, entry.second.size,
               entry.second.entry_point);
    }
}

uint32_t PELoader::count_modules_named(const char* name) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    uint32_t count = 0;
    // Extract basename from the input path for comparison
    std::string lower = name;
    auto slash = lower.find_last_of("/\\");
    if (slash != std::string::npos) lower = lower.substr(slash + 1);
    for (auto& c : lower) c = tolower(c);
    for (const auto& entry : modules_) {
        if (!entry.second.name[0]) continue;
        std::string mname = entry.second.name;
        auto s = mname.find_last_of("/\\");
        if (s != std::string::npos) mname = mname.substr(s + 1);
        for (auto& c : mname) c = tolower(c);
        if (mname == lower) count++;
    }
    return count;
}

} // namespace ThetaZ
