// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/ntdll/virtual.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace ThetaZ {

namespace {
inline uintptr_t page_floor(uintptr_t a) { return a & ~0xFFFULL; }
inline uintptr_t page_ceil(uintptr_t a) { return (a + 0xFFF) & ~0xFFFULL; }

// Coalesce adjacent blocks that share a protection so region queries return
// contiguous extents (as Windows does for VirtualQuery).
void merge_commits(std::vector<CommittedBlock>& blocks) {
    if (blocks.size() < 2) return;
    std::sort(blocks.begin(), blocks.end(),
              [](const CommittedBlock& a, const CommittedBlock& b) {
                  return a.offset < b.offset;
              });
    std::vector<CommittedBlock> merged;
    for (const auto& b : blocks) {
        if (!merged.empty()) {
            auto& last = merged.back();
            if (last.offset + last.size == b.offset && last.page_protect == b.page_protect) {
                last.size += b.size;
                continue;
            }
        }
        merged.push_back(b);
    }
    blocks = std::move(merged);
}
} // namespace

WindowsAddressSpace::WindowsAddressSpace() {
    low_region_ = mmap(nullptr, 8ULL * 1024 * 1024 * 1024, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (low_region_ != MAP_FAILED) {
        low_region_size_ = 8ULL * 1024 * 1024 * 1024;
    } else {
        low_region_ = nullptr;
        low_region_size_ = 0;
    }

    void* high_start = mmap(nullptr, 8ULL * 1024 * 1024 * 1024, PROT_NONE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (high_start != MAP_FAILED) {
        high_region_ = static_cast<char*>(high_start) + 0x7FFFFFFE00000000ULL;
        high_region_size_ = 8ULL * 1024 * 1024 * 1024;
    } else {
        high_region_ = nullptr;
        high_region_size_ = 0;
    }
}

WindowsAddressSpace::~WindowsAddressSpace() {
    for (const auto& region : regions_) {
        munmap(region.base, region.size);
    }

    if (low_region_ != MAP_FAILED && low_region_ != nullptr) munmap(low_region_, low_region_size_);
    if (high_region_ != nullptr) munmap((char*)high_region_ - 0x7FFFFFFE00000000ULL, high_region_size_);
}

WindowsAddressSpace& WindowsAddressSpace::instance() {
    static WindowsAddressSpace space;
    return space;
}

// Find a gap inside the reserved zones large enough for `size`, aligned to
// `alignment`. The zones are PROT_NONE reserved up-front, so every hole we
// find is genuinely unmapped.
void* WindowsAddressSpace::find_free_region(size_t size, size_t alignment) {
    if (alignment < 0x1000) alignment = 0x1000;

    auto scan_zone = [&](uint8_t* zone_base, size_t zone_size) -> void* {
        uint8_t* zone_end = zone_base + zone_size;
        uintptr_t start = reinterpret_cast<uintptr_t>(zone_base);
        uintptr_t end = reinterpret_cast<uintptr_t>(zone_end);

        std::vector<const MemoryRegion*> sorted;
        sorted.reserve(regions_.size());
        for (const auto& r : regions_) sorted.push_back(&r);
        std::sort(sorted.begin(), sorted.end(),
                  [](const MemoryRegion* a, const MemoryRegion* b) {
                      return a->base < b->base;
                  });

        uintptr_t cursor = start;
        for (const auto* r : sorted) {
            uintptr_t r_base = reinterpret_cast<uintptr_t>(r->base);
            uintptr_t r_end = r_base + r->size;
            if (r_end <= cursor) continue;
            if (r_base >= end) break;

            uintptr_t candidate = (cursor + alignment - 1) & ~(alignment - 1);
            if (candidate + size <= r_base) {
                return reinterpret_cast<void*>(candidate);
            }
            cursor = std::max(cursor, r_end);
        }

        uintptr_t candidate = (cursor + alignment - 1) & ~(alignment - 1);
        if (candidate + size <= end) {
            return reinterpret_cast<void*>(candidate);
        }
        return nullptr;
    };

    if (void* addr = scan_zone(reinterpret_cast<uint8_t*>(low_region_), low_region_size_)) {
        return addr;
    }
    if (void* addr = scan_zone(reinterpret_cast<uint8_t*>(high_region_), high_region_size_)) {
        return addr;
    }
    return nullptr;
}

MemoryRegion* WindowsAddressSpace::region_containing(uintptr_t address) {
    for (auto& r : regions_) {
        uintptr_t base = reinterpret_cast<uintptr_t>(r.base);
        if (address >= base && address < base + r.size) return &r;
    }
    return nullptr;
}

const MemoryRegion* WindowsAddressSpace::region_containing(uintptr_t address) const {
    for (const auto& r : regions_) {
        uintptr_t base = reinterpret_cast<uintptr_t>(r.base);
        if (address >= base && address < base + r.size) return &r;
    }
    return nullptr;
}

void* WindowsAddressSpace::reserve(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);

    void* addr = find_free_region(size, alignment);
    if (!addr) return nullptr;

    MemoryRegion r;
    r.base = addr;
    r.size = size;
    r.allocation_protect = PAGE_NOACCESS;
    regions_.push_back(r);
    return addr;
}

void* WindowsAddressSpace::reserve_and_commit(size_t size, size_t alignment, uint32_t page_protect) {
    if (size == 0) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);

    void* addr = find_free_region(size, alignment);
    if (!addr) return nullptr;

    int prot = page_to_prot(page_protect);
    if (mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) {
        return nullptr;
    }

    MemoryRegion r;
    r.base = addr;
    r.size = size;
    r.allocation_protect = page_protect;
    r.commits.push_back({0, size, page_protect});
    regions_.push_back(r);
    return addr;
}

bool WindowsAddressSpace::commit(void* address, size_t size, uint32_t page_protect) {
    if (!address || size == 0) return false;
    uintptr_t s = page_floor(reinterpret_cast<uintptr_t>(address));
    uintptr_t e = page_ceil(reinterpret_cast<uintptr_t>(address) + size);
    std::lock_guard<std::mutex> lock(mutex_);
    return commit_range(s, e, page_protect);
}

bool WindowsAddressSpace::commit_range(uintptr_t s, uintptr_t e, uint32_t page_protect) {
    MemoryRegion* region = region_containing(s);
    if (!region) return false;

    uintptr_t rbase = reinterpret_cast<uintptr_t>(region->base);
    if (e > rbase + region->size) return false;
    if (s >= e) return false;

    int prot = page_to_prot(page_protect);
    uintptr_t cursor = s;
    while (cursor < e) {
        // Skip over spans already committed.
        const CommittedBlock* covering = nullptr;
        for (const auto& b : region->commits) {
            uintptr_t bs = rbase + b.offset;
            if (bs <= cursor && cursor < bs + b.size) {
                covering = &b;
                break;
            }
        }
        if (covering) {
            cursor = rbase + covering->offset + covering->size;
            continue;
        }

        uintptr_t next = e;
        for (const auto& b : region->commits) {
            uintptr_t bs = rbase + b.offset;
            if (bs > cursor && bs < next) next = bs;
        }

        if (mmap(reinterpret_cast<void*>(cursor), next - cursor, prot,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) {
            return false;
        }
        region->commits.push_back({cursor - rbase, next - cursor, page_protect});
        cursor = next;
    }

    merge_commits(region->commits);
    return true;
}

bool WindowsAddressSpace::protect(void* address, size_t size, uint32_t new_protect, uint32_t* old_protect) {
    if (!address || size == 0) return true;
    uintptr_t s = page_floor(reinterpret_cast<uintptr_t>(address));
    uintptr_t e = page_ceil(reinterpret_cast<uintptr_t>(address) + size);
    std::lock_guard<std::mutex> lock(mutex_);
    return protect_range(s, e, new_protect, old_protect);
}

bool WindowsAddressSpace::protect_range(uintptr_t s, uintptr_t e, uint32_t new_protect, uint32_t* old_protect) {
    MemoryRegion* region = region_containing(s);
    if (!region) return false;

    uintptr_t rbase = reinterpret_cast<uintptr_t>(region->base);
    if (e > rbase + region->size) return false;
    if (s >= e) return false;

    int prot = page_to_prot(new_protect);
    bool reported_old = false;
    std::vector<CommittedBlock> result;

    for (const auto& b : region->commits) {
        uintptr_t bs = rbase + b.offset;
        uintptr_t be = bs + b.size;
        if (be <= s || bs >= e) {
            result.push_back(b);
            continue;
        }

        if (!reported_old) {
            if (old_protect) *old_protect = b.page_protect;
            reported_old = true;
        }

        if (bs < s) result.push_back({bs - rbase, s - bs, b.page_protect});

        uintptr_t mid_s = bs > s ? bs : s;
        uintptr_t mid_e = be < e ? be : e;
        mprotect(reinterpret_cast<void*>(mid_s), mid_e - mid_s, prot);
        result.push_back({mid_s - rbase, mid_e - mid_s, new_protect});

        if (be > e) result.push_back({e - rbase, be - e, b.page_protect});
    }

    region->commits = std::move(result);
    merge_commits(region->commits);
    return true;
}

bool WindowsAddressSpace::decommit(void* address, size_t size) {
    if (!address || size == 0) return false;
    uintptr_t s = page_floor(reinterpret_cast<uintptr_t>(address));
    uintptr_t e = page_ceil(reinterpret_cast<uintptr_t>(address) + size);
    std::lock_guard<std::mutex> lock(mutex_);
    return decommit_range(s, e);
}

bool WindowsAddressSpace::decommit_range(uintptr_t s, uintptr_t e) {
    MemoryRegion* region = region_containing(s);
    if (!region) return false;

    uintptr_t rbase = reinterpret_cast<uintptr_t>(region->base);
    if (e > rbase + region->size) return false;
    if (s >= e) return false;

    std::vector<CommittedBlock> result;
    for (const auto& b : region->commits) {
        uintptr_t bs = rbase + b.offset;
        uintptr_t be = bs + b.size;
        if (be <= s || bs >= e) {
            result.push_back(b);
            continue;
        }

        if (bs < s) result.push_back({bs - rbase, s - bs, b.page_protect});

        uintptr_t mid_s = bs > s ? bs : s;
        uintptr_t mid_e = be < e ? be : e;
        mprotect(reinterpret_cast<void*>(mid_s), mid_e - mid_s, PROT_NONE);

        if (be > e) result.push_back({e - rbase, be - e, b.page_protect});
    }

    region->commits = std::move(result);
    merge_commits(region->commits);
    return true;
}

bool WindowsAddressSpace::release(void* address) {
    std::lock_guard<std::mutex> lock(mutex_);
    uintptr_t a = reinterpret_cast<uintptr_t>(address);

    for (auto it = regions_.begin(); it != regions_.end(); ++it) {
        if (reinterpret_cast<uintptr_t>(it->base) == a) {
            munmap(it->base, it->size);
            regions_.erase(it);
            return true;
        }
    }
    return false;
}

bool WindowsAddressSpace::query(void* address, MemoryBasicInformation* out) const {
    if (!out) return false;
    std::lock_guard<std::mutex> lock(mutex_);

    uintptr_t a = page_floor(reinterpret_cast<uintptr_t>(address));
    const MemoryRegion* region = region_containing(a);
    if (!region) return false;

    uintptr_t rbase = reinterpret_cast<uintptr_t>(region->base);
    const CommittedBlock* block = nullptr;
    for (const auto& b : region->commits) {
        uintptr_t bs = rbase + b.offset;
        if (a >= bs && a < bs + b.size) {
            block = &b;
            break;
        }
    }

    memset(out, 0, sizeof(*out));
    out->allocation_base = region->base;
    out->allocation_protect = region->allocation_protect;

    if (block) {
        out->base_address = reinterpret_cast<void*>(rbase + block->offset);
        out->region_size = block->size;
        out->state = MEM_COMMIT;
        out->protect = block->page_protect;
    } else {
        out->base_address = reinterpret_cast<void*>(a);
        out->region_size = rbase + region->size - a;
        out->state = MEM_RESERVE;
        out->protect = PAGE_NOACCESS;
    }
    out->type = MEM_PRIVATE;
    return true;
}

int WindowsAddressSpace::page_to_prot(uint32_t page_protect) {
    uint32_t base = page_protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    int prot;
    switch (base) {
        case PAGE_NOACCESS: prot = PROT_NONE; break;
        case PAGE_READONLY: prot = PROT_READ; break;
        case PAGE_READWRITE: prot = PROT_READ | PROT_WRITE; break;
        case PAGE_WRITECOPY: prot = PROT_READ | PROT_WRITE; break;
        case PAGE_EXECUTE: prot = PROT_EXEC; break;
        case PAGE_EXECUTE_READ: prot = PROT_EXEC | PROT_READ; break;
        case PAGE_EXECUTE_READWRITE: prot = PROT_EXEC | PROT_READ | PROT_WRITE; break;
        case PAGE_EXECUTE_WRITECOPY: prot = PROT_EXEC | PROT_READ | PROT_WRITE; break;
        default: prot = PROT_NONE; break;
    }
    if (prot != PROT_NONE) prot |= PROT_EXEC;
    return prot;
}

size_t WindowsAddressSpace::get_used_address_space() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& region : regions_) {
        for (const auto& b : region.commits) {
            total += b.size;
        }
    }
    return total;
}

void WindowsAddressSpace::print_layout() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t committed = 0;
    size_t reserved = 0;
    for (const auto& region : regions_) {
        reserved += region.size;
        for (const auto& b : region.commits) committed += b.size;
    }

    printf("Windows Address Space Layout:\n");
    printf("Low region: %p-%p (8GB)\n", low_region_,
           static_cast<char*>(low_region_) + low_region_size_);
    printf("High region: %p-%p (8GB)\n",
           static_cast<char*>(high_region_) + high_region_size_,
           static_cast<char*>(high_region_) + high_region_size_ + high_region_size_);

    printf("Committed: %zu MB (%zu reservations, %zu bytes reserved)\n",
           committed / (1024 * 1024), regions_.size(), reserved);
}

} // namespace ThetaZ
