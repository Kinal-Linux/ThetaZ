// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <sys/mman.h>

namespace ThetaZ {

// PAGE_* protection constants (Windows)
constexpr uint32_t PAGE_NOACCESS = 0x01;
constexpr uint32_t PAGE_READONLY = 0x02;
constexpr uint32_t PAGE_READWRITE = 0x04;
constexpr uint32_t PAGE_WRITECOPY = 0x08;
constexpr uint32_t PAGE_EXECUTE = 0x10;
constexpr uint32_t PAGE_EXECUTE_READ = 0x20;
constexpr uint32_t PAGE_EXECUTE_READWRITE = 0x40;
constexpr uint32_t PAGE_EXECUTE_WRITECOPY = 0x80;
constexpr uint32_t PAGE_GUARD = 0x100;
constexpr uint32_t PAGE_NOCACHE = 0x200;
constexpr uint32_t PAGE_WRITECOMBINE = 0x400;

// MEM_* allocation state constants (Windows)
constexpr uint32_t MEM_COMMIT = 0x1000;
constexpr uint32_t MEM_RESERVE = 0x2000;
constexpr uint32_t MEM_DECOMMIT = 0x4000;
constexpr uint32_t MEM_RELEASE = 0x8000;
constexpr uint32_t MEM_FREE = 0x10000;
constexpr uint32_t MEM_PRIVATE = 0x20000;
constexpr uint32_t MEM_MAPPED = 0x40000;

// A single committed (mapped) span inside a reservation.
struct CommittedBlock {
    size_t offset;         // offset from the reservation base
    size_t size;
    uint32_t page_protect;
};

// A MEM_RESERVE allocation. Committed portions are tracked separately.
struct MemoryRegion {
    void* base;
    size_t size;
    uint32_t allocation_protect;
    std::vector<CommittedBlock> commits;
};

// Mirrors the PE-visible layout of MEMORY_BASIC_INFORMATION. Must stay packed
// like the Windows struct so guest code copying it verbatim works.
#pragma pack(push, 8)
struct MemoryBasicInformation {
    void* base_address;
    void* allocation_base;
    uint32_t allocation_protect;
    uintptr_t region_size;
    uint32_t state;
    uint32_t protect;
    uint32_t type;
};
#pragma pack(pop)

class WindowsAddressSpace {
public:
    static WindowsAddressSpace& instance();

    void* reserve(size_t size, size_t alignment = 0x1000);
    void* reserve_and_commit(size_t size, size_t alignment, uint32_t page_protect);
    bool commit(void* address, size_t size, uint32_t page_protect);
    bool decommit(void* address, size_t size);
    bool release(void* address);
    bool protect(void* address, size_t size, uint32_t new_protect, uint32_t* old_protect);
    bool query(void* address, MemoryBasicInformation* out) const;

    size_t get_used_address_space() const;
    void print_layout() const;

private:
    WindowsAddressSpace();
    ~WindowsAddressSpace();

    void* find_free_region(size_t size, size_t alignment);
    MemoryRegion* region_containing(uintptr_t address);
    const MemoryRegion* region_containing(uintptr_t address) const;
    bool commit_range(uintptr_t s, uintptr_t e, uint32_t page_protect);
    bool protect_range(uintptr_t s, uintptr_t e, uint32_t new_protect, uint32_t* old_protect);
    bool decommit_range(uintptr_t s, uintptr_t e);
    static int page_to_prot(uint32_t page_protect);

    void* low_region_;
    void* high_region_;
    size_t low_region_size_;
    size_t high_region_size_;

    std::vector<MemoryRegion> regions_;
    mutable std::mutex mutex_;
};

} // namespace ThetaZ
