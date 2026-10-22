// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace ThetaZ {

struct LoadedModule {
    char name[256];
    uint64_t base;
    uint64_t size;
    void* mapped_data;
    uint64_t entry_point;
    bool is_dll;
    bool is_32bit;
    bool disable_thread_calls = false;

    uint64_t export_dir_rva;
    uint64_t export_dir_size;
    std::unordered_map<std::string, uint64_t> exports_by_name;
    std::unordered_map<uint16_t, uint64_t> exports_by_ordinal;
    std::unordered_map<std::string, std::string> forwarders;
    std::unordered_map<uint16_t, std::string> forwarders_by_ordinal;
};

class PELoader {
public:
    static PELoader& instance();

    PELoader();
    ~PELoader();

    LoadedModule* load_image(const char* path, uint64_t preferred_base = 0);
    // Unloads a module by its load base. Name-based unloading is unsafe when
    // multiple copies of the same image are loaded (parent + fresh child
    // copies share a name) — a name match could unmap the wrong copy.
    void unload_image(uint64_t base);
    LoadedModule* find_module(const char* name);
    LoadedModule* find_module_by_base(uint64_t base);

    // Case-insensitive basename lookup (e.g. "kernel32.dll" matches a module
    // loaded from any path). Used by GetModuleHandleA.
    LoadedModule* find_module_named(const char* name);

    void* get_proc_address(const char* module_name, const char* proc_name);
    uint64_t get_entry_point(const char* module_name);

    // Public export lookup (name and/or ordinal), forwarders followed.
    void* get_export_address(LoadedModule& module, uint16_t ordinal,
                             const char* name);

    void print_loaded_modules() const;

    // Count modules whose base name matches `name` (case-insensitive).
    uint32_t count_modules_named(const char* name) const;

    // Access the loaded module map (read-only iteration).
    const std::unordered_map<uint64_t, LoadedModule>& get_modules() const { return modules_; }

    // Try to load a DLL from the main image dir or ThetaZ prefix system dirs.
    LoadedModule* try_load_from_thetaz_prefix(const std::string& dll_name, bool is_32bit);

private:
    bool read_file(const char* path, std::vector<uint8_t>& data);
    bool validate_pe(std::vector<uint8_t>& data);
    uint64_t map_sections(std::vector<uint8_t>& data, uint64_t base_addr, bool force_fresh);    void set_final_protections(std::vector<uint8_t>& data, uint64_t base_addr);
    void apply_relocations(void* image_base, std::vector<uint8_t>& data, uint64_t delta);
    void process_imports(void* image_base, std::vector<uint8_t>& data);
    void process_delay_load_imports(void* image_base, std::vector<uint8_t>& data);
    void process_exports(void* image_base, std::vector<uint8_t>& data, LoadedModule& module);

    LoadedModule* find_module_by_dll_name(const std::string& dll_name);
    void* resolve_forwarder(const std::string& forwarder_str, size_t depth);
    void* resolve_export(LoadedModule& module, uint16_t ordinal,
                         const std::string& name, size_t depth);

    // Keyed by load base so two copies of the same image (parent + a child
    // self-relaunch) can coexist without one clobbering the other.
    std::unordered_map<uint64_t, LoadedModule> modules_;
    mutable std::recursive_mutex mutex_;
};

} // namespace ThetaZ
