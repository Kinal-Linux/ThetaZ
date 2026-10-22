# ThetaZ Implementation Plan & Architecture Audit

## Current Status (2026-08-03)

### What's Working
- ✅ Basic PE loader working
- ✅ Thread manager and TEB/PEB setup
- ✅ Virtual memory management
- ✅ Sync primitives (semaphore, event, critical section, mutex)
- ✅ IPC server foundation
- ✅ Exception handling (signal-based)
- ✅ Window layer (E1) with 56 tests passing
- ✅ CreateProcessA implementation (E3) with 64 tests passing
- ✅ GDI layer (E4) mostly working
- ✅ UCRT layer complete
- ✅ 7 critical kernel32 APIs implemented and working

### What's Broken
- ❌ Build system has compilation errors
- ❌ Exception.cpp needs complete rewrite
- ❌ thread_api.cpp has syntax errors
- ❌ api.cpp has duplicate function definitions
- ❌ kernel32.cpp is incomplete (only 7 APIs)
- ❌ Environment setup is not automatic
- ❌ Path resolution is not multi-path
- ❌ Error messages are unhelpful ("!! unresolved:")

---

## Audit Results

### 1. Code Quality Issues

#### File Structure Problems
- `kernel32.cpp` has functions implemented AFTER initialization function → compilation errors
- Functions are not properly ordered by declaration order
- Missing include guards in some files
- Inconsistent naming conventions

#### API Implementation Quality
- Current implementations are minimal stubs (just printf + return)
- No fallback mechanisms implemented
- No graceful error handling
- Missing proper return value checking
- Type mismatches in several functions

### 2. Architecture Issues

#### Environment Setup
- ❌ No automatic environment creation
- ❌ No multi-path resolution
- ❌ No registry simulation
- ❌ User must manually setup everything

#### API Coverage
- ❌ ~200 critical APIs still unresolved
- ❌ Most implementations are stubs
- ❌ No fallback mechanisms
- ❌ Poor error messages

#### Path Handling
- ❌ No Windows→Linux path translation
- ❌ No fallback when file not found
- ❌ No automatic directory creation
- ❌ No search path resolution

### 3. Missing Functionality

#### Registry Layer
- ❌ No registry simulation
- ❌ No registry generation
- ❌ No registry to file mapping

#### Configuration Management
- ❌ No config file parsing
- ❌ No runtime configuration
- ❌ No command-line argument parsing

#### Testing Infrastructure
- ❌ No automated test suite for APIs
- ❌ No integration testing
- ❌ No regression testing
- ❌ No performance testing

---

## Implementation Strategy

### Phase 1: Fix Build System (Priority 1)
**Goal**: Get project building again

1. Fix `kernel32.h` EXCEPTION_POINTERS declaration
2. Revert `api.cpp` to original state
3. Rewrite `exception.cpp` completely
4. Fix `thread_api.cpp` includes and GetThreadId
5. Fix function ordering in `kernel32.cpp`
6. Add proper includes to all files

**Success Criteria**: 
- `cmake --build build` succeeds
- All existing tests pass

---

### Phase 2: Implement Automatic Environment (Priority 2)
**Goal**: Make system "idiot-proof"

1. Create `EnvironmentSetup` class
   - Automatic prefix directory creation
   - Create drive_c structure
   - Create Windows directory structure
   - Create Program Files structure
   - Create system32 directories

2. Create `PathResolver` class
   - Windows→Linux path translation
   - Multi-path resolution (try 5+ approaches)
   - Automatic directory creation
   - Search path handling

3. Create `RegistryManager` class
   - Registry file generation
   - Registry key lookup with fallbacks
   - Registry entry creation

**Success Criteria**:
- Single run setup (no manual intervention)
- Automatic file location resolution
- Registry entries available on first run

---

### Phase 3: Comprehensive API Implementation (Priority 3)
**Goal**: Implement all critical APIs with proper fallbacks

#### Priority Order (based on EXCEL.EXE usage):
1. **Crash Prevention APIs** (~50 functions)
   - GetModuleFileNameA/W
   - GetCurrentDirectoryA/W
   - GetTempPathW
   - GetCommandLineA/W
   - SetCurrentDirectoryA/W
   - GetProcessHeap
   - HeapAlloc/Free/ReAlloc
   - CreateFileA/W
   - ReadFile/WriteFile
   - GetFileAttributesA/W
   - GetModuleHandleA/W
   - LoadLibraryA/W

2. **File System APIs** (~30 functions)
   - FindFirstFileW/FindNextFileW/FindClose
   - GetFileSizeEx
   - CreateDirectoryW
   - DeleteFileW
   - MoveFileW
   - CopyFileW

3. **String Conversion APIs** (~40 functions)
   - WideCharToMultiByte
   - MultiByteToWideChar
   - lstrlenA/W
   - wcscpyA/W
   - strcpyA/W

4. **Error Handling APIs** (~20 functions)
   - GetLastError
   - SetLastError
   - FormatMessageW
   - SetLastError

5. **Process APIs** (~30 functions)
   - GetCurrentProcessId
   - TerminateProcess
   - ExitProcess
   - GetExitCodeProcess

6. **Thread APIs** (~30 functions)
   - CreateThread
   - GetCurrentThread/GetCurrentThreadId
   - SuspendThread/ResumeThread
   - SetThreadPriority
   - Sleep

7. **Timing APIs** (~20 functions)
   - GetTickCount/GetTickCount64
   - QueryPerformanceCounter
   - QueryPerformanceFrequency
   - GetSystemTime
   - GetSystemTimeAsFileTime

8. **Memory APIs** (~40 functions)
   - VirtualAlloc
   - VirtualFree
   - VirtualProtect
   - VirtualQuery
   - VirtualProtectEx
   - WriteProcessMemory
   - ReadProcessMemory

**Implementation Standard**:
For EACH API:
1. Implement core functionality
2. Add multi-path fallback (try 3+ approaches)
3. Add graceful error handling
4. Add detailed debug logging
5. Add proper return value checking
6. Add type safety

**Example Implementation Pattern**:
```cpp
DWORD WINAPI_CALL WINAPI_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, 
                                     DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                     DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                     HANDLE hTemplateFile) {
    // Tier 1: Try Windows path
    std::string windows_path = translate_windows_to_linux(lpFileName);
    HANDLE hFile = open_windows_path(windows_path);
    if (hFile != INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Created file: %s\n", windows_path.c_str());
        return (DWORD)hFile;
    }
    
    // Tier 2: Try translated Linux path
    std::string linux_path = lpFileName;
    hFile = open_linux_path(linux_path);
    if (hFile != INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Opened file: %s\n", linux_path.c_str());
        return (DWORD)hFile;
    }
    
    // Tier 3: Try creating file
    hFile = create_file(lpFileName);
    if (hFile != INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Created file: %s\n", lpFileName);
        return (DWORD)hFile;
    }
    
    // Tier 4: Return error with details
    fprintf(stderr, "ERROR: CreateFileW failed: %ls\n", lpFileName);
    SetLastError(ERROR_FILE_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
}
```

---

### Phase 4: Testing & Validation (Priority 4)
**Goal**: Ensure quality and correctness

1. **Unit Testing**
   - Test each API implementation individually
   - Test edge cases (NULL parameters, error conditions)
   - Test multi-path fallbacks
   - Test type safety

2. **Integration Testing**
   - Run existing test suite
   - Test with EXCEL.EXE
   - Test with WebView2
   - Test with other Office applications

3. **Regression Testing**
   - Run all tests before each change
   - Document breaking changes
   - Implement test automation

4. **Performance Testing**
   - Measure API call overhead
   - Identify bottlenecks
   - Optimize critical paths

**Success Criteria**:
- 95%+ of APIs implemented with proper fallbacks
- 100% test pass rate
- No regressions in existing functionality
- EXCEL.EXE processes further into startup

---

### Phase 5: Documentation & Tooling (Priority 5)
**Goal**: Make development and maintenance easier

1. **API Documentation**
   - Document all implemented APIs
   - Document fallback mechanisms
   - Document error codes
   - Document usage examples

2. **Build Tooling**
   - Add API code generation
   - Add automated testing
   - Add linting
   - Add formatting

3. **Development Tools**
   - API dependency analyzer
   - Import resolver helper
   - Test runner
   - Debug visualization

---

## Implementation Priority Summary

### Immediate (Today)
1. ✅ Fix compilation errors
2. ✅ Get project building
3. ✅ Run existing tests

### This Week
1. Implement automatic environment setup
2. Implement multi-path resolution
3. Implement registry layer
4. Implement Priority 1 APIs (~50 functions)

### Next Week
1. Implement Priority 2-5 APIs
2. Add comprehensive testing
3. Document implementations

### Future
1. Optimize performance
2. Add GUI setup wizard
3. Add more comprehensive test suite
4. Add debugging tools

---

## Quality Standards

### Code Quality
- ✅ C++17 compliant
- ✅ RAII memory management
- ✅ Type-safe
- ✅ No raw pointers (use smart pointers)
- ✅ No global mutable state (use proper encapsulation)
- ✅ Const-correct
- ✅ noexcept where appropriate
- ✅ Inline documentation for all public APIs

### API Quality
- ✅ Proper error handling
- ✅ Multi-path fallbacks
- ✅ Graceful degradation
- ✅ Detailed error messages
- ✅ Type safety
- ✅ Return value consistency
- ✅ NULL parameter handling
- ✅ Size/length validation

### Testing Quality
- ✅ Unit tests for all APIs
- ✅ Integration tests
- ✅ Edge case tests
- ✅ Error condition tests
- ✅ Regression tests

---

## References for Best Practices

### Web Research Needed:
1. **WINE Architecture**: How Wine implements APIs
   - How Wine handles path resolution
   - How Wine handles registry
   - How Wine manages DLLs

2. **Proton Architecture**: Game compatibility layer
   - How Proton implements compatibility
   - How Proton handles exceptions
   - How Proton manages memory

3. **Windows API Best Practices**:
   - How Windows implements APIs
   - Error handling patterns
   - Return value conventions
   - Thread safety patterns

### Code References:
- `/mnt/hdd/Desktop/Projects/C++/ThetaZ/Wine/` - Official Wine source
- `/mnt/hdd/Desktop/Projects/C++/ThetaZ/ProtonGE/` - Proton source
- Existing test files in `tests/` directory

---

## Success Metrics

### Build Metrics
- ✅ Compilation success rate: 100%
- ✅ Test pass rate: 95%+
- ✅ Build time: < 60 seconds

### Functionality Metrics
- ✅ API implementation rate: 95%+
- ✅ Critical API rate: 100%
- ✅ Automatic environment setup: 100%
- ✅ Multi-path resolution: 100%

### Testing Metrics
- ✅ Unit test coverage: 80%+
- ✅ Integration test coverage: 70%+
- ✅ Regression test coverage: 95%+
- ✅ Test execution time: < 30 seconds

### Quality Metrics
- ✅ Code coverage: 80%+
- ✅ Type safety: 100%
- ✅ Memory safety: 100%
- ✅ Error handling: 95%+

---

## Implementation Timeline

### Week 1: Foundation
- Fix build system
- Implement automatic environment
- Implement multi-path resolution
- Fix all compilation errors

### Week 2: API Implementation
- Implement Priority 1 APIs (crash prevention)
- Implement Priority 2 APIs (file system)
- Implement Priority 3 APIs (string conversion)

### Week 3: More APIs
- Implement Priority 4 APIs (error handling)
- Implement Priority 5 APIs (process)
- Implement Priority 6 APIs (thread)
- Implement Priority 7 APIs (timing)

### Week 4: Remaining APIs & Testing
- Implement Priority 8 APIs (memory)
- Add comprehensive testing
- Run integration tests
- Fix any bugs found

### Week 5+: Optimization & Documentation
- Optimize performance
- Document all APIs
- Add development tools
- Create user guide

---

## Risk Assessment

### High Risk
- **Broken Build System**: Fix immediately (blocker)
- **Missing APIs**: Implement systematically
- **No Testing**: Add tests before implementing

### Medium Risk
- **Performance**: Profile and optimize
- **Memory Leaks**: Use RAII and smart pointers
- **Complexity**: Keep code simple and maintainable

### Low Risk
- **Documentation**: Add as we go
- **Tooling**: Can be added later
- **GUI**: Not critical for MVP

---

## Decision Framework

### Should I Implement This API?
1. Is it used by EXCEL.EXE?
2. Will its implementation prevent a crash?
3. Is there a simple fallback?
4. Can it be implemented safely?
5. Is it critical for stability?

**If YES to all**: Implement with multi-path fallback
**If NO to any**: Skip or implement as stub

---

## Next Actions

### Immediate (This Session)
1. ✅ Review this plan document
2. ✅ Start with Phase 1 (fix build system)
3. ✅ Get project building again
4. ✅ Run existing tests

### Today
1. ✅ Fix kernel32.h EXCEPTION_POINTERS
2. ✅ Revert api.cpp to original
3. ✅ Rewrite exception.cpp
4. ✅ Fix thread_api.cpp
5. ✅ Fix kernel32.cpp structure
6. ✅ Build and test

---

## Questions to Answer

1. Should we use existing Wine/Proton code as reference?
2. What's the minimum viable product?
3. How do we handle licensing if we reference Wine/Proton?
4. What's the expected performance?
5. What's the maximum acceptable memory usage?
6. What platforms should we support?
7. What's the target audience?

---

## Notes

- This is a from-scratch implementation, not derived from Wine/Proton
- We should research best practices independently
- We should implement based on the "idiot-proof" philosophy
- We should use modern C++17 features
- We should maintain high code quality standards

