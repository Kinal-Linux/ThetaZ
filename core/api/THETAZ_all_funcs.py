all_funcs = {
    "THETAZ_ActivateActCtx": """BOOL WINAPI_CALL THETAZ_ActivateActCtx(HANDLE c,ULONG_PTR* k){if(k)*k=1;return TRUE;}""",
    "THETAZ_AddAccessAllowedAce": """BOOL WINAPI_CALL THETAZ_AddAccessAllowedAce(void* pAcl, DWORD dwAceRevision, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AccessMask; (void)pSid;
    return 1;
}""",
    "THETAZ_AddAccessAllowedAceEx": """BOOL WINAPI_CALL THETAZ_AddAccessAllowedAceEx(void* pAcl, DWORD dwAceRevision, DWORD AceFlags, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AceFlags; (void)AccessMask; (void)pSid;
    return 1;
}""",
    "THETAZ_AddAccessDeniedAce": """BOOL WINAPI_CALL THETAZ_AddAccessDeniedAce(void* pAcl, DWORD dwAceRevision, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AccessMask; (void)pSid;
    return 1;
}""",
    "THETAZ_AddAccessDeniedAceEx": """BOOL WINAPI_CALL THETAZ_AddAccessDeniedAceEx(void* pAcl, DWORD dwAceRevision, DWORD AceFlags, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AceFlags; (void)AccessMask; (void)pSid;
    return 1;
}""",
    "THETAZ_AddAtomA": """ATOM WINAPI_CALL THETAZ_AddAtomA(const char* n){return (ATOM)(uintptr_t)(n+0x10000);}""",
    "THETAZ_AdjustTokenPrivileges": """BOOL WINAPI_CALL THETAZ_AdjustTokenPrivileges(HANDLE TokenHandle, BOOL DisableAllPrivileges,
    void* NewState, DWORD BufferLength, void* PreviousState, LPDWORD ReturnLength) {
    (void)TokenHandle; (void)DisableAllPrivileges;
    (void)NewState; (void)BufferLength; (void)PreviousState; (void)ReturnLength;
    return 1;
}""",
    "THETAZ_AdjustWindowRect": """int WINAPI_CALL THETAZ_AdjustWindowRect(void* rect, uint32_t style, int menu) { return 1; }""",
    "THETAZ_AdjustWindowRectEx": """int WINAPI_CALL THETAZ_AdjustWindowRectEx(void* rect, uint32_t style, int menu, uint32_t exstyle) { return 1; }""",
    "THETAZ_AllocateAndInitializeSid": """BOOL WINAPI_CALL THETAZ_AllocateAndInitializeSid(void* pIdentifierAuthority, BYTE nSubAuthorityCount,
    DWORD dwSubAuthority0, DWORD dwSubAuthority1, DWORD dwSubAuthority2, DWORD dwSubAuthority3,
    DWORD dwSubAuthority4, DWORD dwSubAuthority5, DWORD dwSubAuthority6, DWORD dwSubAuthority7,
    void** ppSid) {
    (void)pIdentifierAuthority; (void)nSubAuthorityCount;
    (void)dwSubAuthority0; (void)dwSubAuthority1; (void)dwSubAuthority2; (void)dwSubAuthority3;
    (void)dwSubAuthority4; (void)dwSubAuthority5; (void)dwSubAuthority6; (void)dwSubAuthority7;
    if (ppSid) {
        static uint8_t fake_sid[12] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        *ppSid = fake_sid;
    }
    return 1;
}""",
    "THETAZ_AllocateLocallyUniqueId": """BOOL WINAPI_CALL THETAZ_AllocateLocallyUniqueId(uint64_t* pLuid) {
    if (pLuid) *pLuid = 1;
    return 1;
}""",
    "THETAZ_AllowSetForegroundWindow": """BOOL WINAPI_CALL THETAZ_AllowSetForegroundWindow(DWORD dwProcessId) {
    (void)dwProcessId;
    return 1;
}""",
    "THETAZ_AppendMenuW": """int WINAPI_CALL THETAZ_AppendMenuW(uint32_t hmenu, uint32_t flags, uint32_t id, const uint16_t* str) { return 1; }""",
    "THETAZ_ApplicationRecoveryFinished": """void WINAPI_CALL THETAZ_ApplicationRecoveryFinished(BOOL s){}""",
    "THETAZ_ApplicationRecoveryInProgress": """HRESULT WINAPI_CALL THETAZ_ApplicationRecoveryInProgress(BOOL* c){if(c)*c=FALSE;return 0;}""",
    "THETAZ_AssignProcessToJobObject": """BOOL WINAPI_CALL THETAZ_AssignProcessToJobObject(HANDLE j,HANDLE p){return TRUE;}""",
    "THETAZ_BindIoCompletionCallback": """BOOL WINAPI_CALL THETAZ_BindIoCompletionCallback(HANDLE f,void* c,DWORD flags){return FALSE;}""",
    "THETAZ_CallWindowProcA": """LRESULT WINAPI_CALL THETAZ_CallWindowProcA(void* proc, uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    if (!proc) return 0;
    return ((LRESULT(WINAPI_CALL*)(uint32_t, uint32_t, WPARAM, LPARAM))proc)(hwnd, msg, wParam, lParam);
}""",
    "THETAZ_CallWindowProcW": """LRESULT WINAPI_CALL THETAZ_CallWindowProcW(void* proc, uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    return THETAZ_CallWindowProcA(proc, hwnd, msg, wParam, lParam);
}""",
    "THETAZ_CancelThreadpoolIo": """void WINAPI_CALL THETAZ_CancelThreadpoolIo(TP_IO* i){}""",
    "THETAZ_CertCompareCertificate": """BOOL WINAPI_CALL THETAZ_CertCompareCertificate(DWORD dwCertEncodingType, void* pCertContext1, void* pCertContext2) {
    (void)dwCertEncodingType; (void)pCertContext1; (void)pCertContext2;
    return 0;
}""",
    "THETAZ_CertDeleteCertificateFromStore": """BOOL WINAPI_CALL THETAZ_CertDeleteCertificateFromStore(void* pCertContext) {
    (void)pCertContext;
    return 1;
}""",
    "THETAZ_CertFreeCertificateContext": """void WINAPI_CALL THETAZ_CertFreeCertificateContext(void* pCertContext) {
    (void)pCertContext;
}""",
    "THETAZ_CertOpenSystemStoreW": """HANDLE WINAPI_CALL THETAZ_CertOpenSystemStoreW(void* hprov, LPCWSTR szSubsystemProtocol) {
    (void)hprov; (void)szSubsystemProtocol;
    static uint64_t g_cert_handle_counter = 0x2000;
    return reinterpret_cast<HANDLE>(++g_cert_handle_counter);
}""",
    "THETAZ_CertSelectionGetSerializedBlob": """HRESULT WINAPI_CALL THETAZ_CertSelectionGetSerializedBlob(void* pCertContext, void** ppCertBlob) {
    (void)pCertContext;
    if (ppCertBlob) *ppCertBlob = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_CertVerifyCertificateChainPolicy": """BOOL WINAPI_CALL THETAZ_CertVerifyCertificateChainPolicy(void* pszPolicyOID, void* pChainContext,
    void* pPolicyPara, void* pPolicyStatus) {
    (void)pszPolicyOID; (void)pChainContext; (void)pPolicyPara;
    if (pPolicyStatus) memset(pPolicyStatus, 0, 16);
    return 1;
}""",
    "THETAZ_ChangeServiceConfigW": """BOOL WINAPI_CALL THETAZ_ChangeServiceConfigW(void* hService, DWORD dwServiceType, DWORD dwStartType,
    DWORD dwErrorControl, const wchar_t* lpBinaryPathName, const wchar_t* lpLoadOrderGroup,
    LPDWORD lpdwTagId, const wchar_t* lpDependencies, const wchar_t* lpServiceStartName,
    const wchar_t* lpPassword, const wchar_t* lpDisplayName) {
    (void)hService; (void)dwServiceType; (void)dwStartType; (void)dwErrorControl;
    (void)lpBinaryPathName; (void)lpLoadOrderGroup; (void)lpdwTagId;
    (void)lpDependencies; (void)lpServiceStartName; (void)lpPassword; (void)lpDisplayName;
    return 0;
}""",
    "THETAZ_CheckDlgButton": """BOOL WINAPI_CALL THETAZ_CheckDlgButton(WinHandle hdlg, int id, uint32_t check) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl) return FALSE_VALUE;
    return THETAZ_SendMessageA(ctrl, BM_SETCHECK, check ? 1 : 0, 0) ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_CheckDlgButton_Wrapper": """BOOL WINAPI_CALL THETAZ_CheckDlgButton_Wrapper(WinHandle hdlg, int id, uint32_t check) {
    if (x86_dispatch_active()) return THETAZ_CheckDlgButton(hdlg, id, check);
    return WINAPI64_CheckDlgButton(hdlg, id, check);
}""",
    "THETAZ_CheckMenuItem": """int WINAPI_CALL THETAZ_CheckMenuItem(uint32_t hmenu, uint32_t id, uint32_t flags) { return 0; }""",
    "THETAZ_CheckTokenMembership": """BOOL WINAPI_CALL THETAZ_CheckTokenMembership(HANDLE TokenHandle, void* SidToCheck, PBOOL IsAdmin) {
    (void)TokenHandle; (void)SidToCheck;
    if (IsAdmin) *IsAdmin = 0;
    return 1;
}""",
    "THETAZ_ChrCmpIA": """BOOL WINAPI_CALL THETAZ_ChrCmpIA(wchar_t a,wchar_t b){return towlower_compat(a)!=towlower_compat(b);}""",
    "THETAZ_ChrCmpIW": """BOOL WINAPI_CALL THETAZ_ChrCmpIW(wchar_t a,wchar_t b){return towlower_compat(a)!=towlower_compat(b);}""",
    "THETAZ_ClientToScreen": """int WINAPI_CALL THETAZ_ClientToScreen(uint32_t hwnd, void* pt) { return 1; }""",
    "THETAZ_CloseServiceHandle": """BOOL WINAPI_CALL THETAZ_CloseServiceHandle(void* hSCObject) {
    (void)hSCObject;
    return 1;
}""",
    "THETAZ_CloseThreadpoolIo": """void WINAPI_CALL THETAZ_CloseThreadpoolIo(TP_IO* i){}""",
    "THETAZ_CoCreateFreeThreadedMarshaler": """HRESULT WINAPI_CALL THETAZ_CoCreateFreeThreadedMarshaler(void* punkOuter, void** ppunkMarshaler) {
    (void)punkOuter;
    if (ppunkMarshaler) *ppunkMarshaler = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_CoCreateGuid": """HRESULT WINAPI_CALL THETAZ_CoCreateGuid(GUID* pguid) {
    if (!pguid) return E_INVALIDARG;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint32_t seed = static_cast<uint32_t>(tv.tv_sec) ^ static_cast<uint32_t>(tv.tv_usec)
                    ^ static_cast<uint32_t>(getpid()) ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pguid));
    pguid->Data1 = seed;
    pguid->Data2 = static_cast<uint16_t>((seed >> 16) ^ (seed & 0xFFFF));
    pguid->Data3 = static_cast<uint16_t>((seed >> 8) ^ (seed & 0xFF));
    pguid->Data4[0] = 0x80;
    pguid->Data4[1] = static_cast<uint8_t>(seed >> 24);
    pguid->Data4[2] = static_cast<uint8_t>(seed >> 16);
    pguid->Data4[3] = static_cast<uint8_t>(seed >> 8);
    pguid->Data4[4] = 0x80;
    pguid->Data4[5] = static_cast<uint8_t>(seed);
    pguid->Data4[6] = static_cast<uint8_t>(seed >> 4);
    pguid->Data4[7] = static_cast<uint8_t>(seed >> 12);
    return S_OK;
}""",
    "THETAZ_CoFileTimeNow": """HRESULT WINAPI_CALL THETAZ_CoFileTimeNow(uint64_t* pft) {
    if (!pft) return E_INVALIDARG;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t ft = static_cast<uint64_t>(tv.tv_sec) * 10000000ULL + 116444736000000000ULL
                  + static_cast<uint64_t>(tv.tv_usec) * 10ULL;
    *pft = ft;
    return S_OK;
}""",
    "THETAZ_CoFreeUnusedLibraries": """void WINAPI_CALL THETAZ_CoFreeUnusedLibraries() {}""",
    "THETAZ_CoInitializeEx": """HRESULT WINAPI_CALL THETAZ_CoInitializeEx(void* pvReserved, DWORD dwCoInit) {
    (void)pvReserved; (void)dwCoInit;
    return S_OK;
}""",
    "THETAZ_CoRegisterPSClsid": """HRESULT WINAPI_CALL THETAZ_CoRegisterPSClsid(GUID* riid, GUID* rclsid) {
    (void)riid; (void)rclsid;
    return S_OK;
}""",
    "THETAZ_CoSetProxyBlanket": """HRESULT WINAPI_CALL THETAZ_CoSetProxyBlanket(void* pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
    void* pServerPrincName, DWORD dwRpcnAuthnLevel, DWORD dwImpLevel,
    void* pAuthInfo, DWORD dwCapabilities) {
    (void)pProxy; (void)dwAuthnSvc; (void)dwAuthzSvc; (void)pServerPrincName;
    (void)dwRpcnAuthnLevel; (void)dwImpLevel; (void)pAuthInfo; (void)dwCapabilities;
    return S_OK;
}""",
    "THETAZ_CoTaskMemFree": """void WINAPI_CALL THETAZ_CoTaskMemFree(LPVOID pv) {
    (void)pv;
}""",
    "THETAZ_ColorMatchToTarget": """DWORD WINAPI_CALL THETAZ_ColorMatchToTarget(void* hdc, void* hdcTarget, DWORD mode) {
    (void)hdc; (void)hdcTarget; (void)mode;
    return 1;
}""",
    "THETAZ_CombineRgn": """int WINAPI_CALL THETAZ_CombineRgn(uint32_t hdest, uint32_t hsrc1, uint32_t hsrc2, int mode) {
    if (!hdest) return 0;
    return 1;
}""",
    "THETAZ_CompareFileTime": """int WINAPI_CALL THETAZ_CompareFileTime(const FILETIME* ft1, const FILETIME* ft2) {
    if (!ft1 || !ft2) return -1;
    if (ft2->dwHighDateTime > ft1->dwHighDateTime) return -1;
    if (ft2->dwHighDateTime < ft1->dwHighDateTime) return 1;
    if (ft2->dwLowDateTime > ft1->dwLowDateTime) return -1;
    if (ft2->dwLowDateTime < ft1->dwLowDateTime) return 1;
    return 0;
}""",
    "THETAZ_CompareStringA": """int WINAPI_CALL THETAZ_CompareStringA(uint32_t lcid, uint32_t flags, const char* s1, int l1, const char* s2, int l2) {
    if (!s1 || !s2) return 2;
    if (l1 == -1) l1 = (int)strlen(s1);
    if (l2 == -1) l2 = (int)strlen(s2);
    int r = strncmp(s1, s2, std::min(l1, l2));
    if (r != 0) return r < 0 ? 1 : 3;
    if (l1 < l2) return 1;
    if (l1 > l2) return 3;
    return 2;
}""",
    "THETAZ_CompareStringOrdinal": """int WINAPI_CALL THETAZ_CompareStringOrdinal(const uint16_t* s1, int l1, const uint16_t* s2, int l2, int ignore) {
    if (!s1 || !s2) return 0;
    if (l1 == -1) l1 = wstrlen(s1);
    if (l2 == -2) l2 = wstrlen(s2);
    int r = wstrncmp(s1, s2, std::min(l1, l2));
    if (r != 0) return r < 0 ? 1 : 3;
    if (l1 < l2) return 1;
    if (l1 > l2) return 3;
    return 2;
}""",
    "THETAZ_ConnectNamedPipe": """BOOL WINAPI_CALL THETAZ_ConnectNamedPipe(HANDLE p,void* o){return FALSE;}""",
    "THETAZ_ControlService": """BOOL WINAPI_CALL THETAZ_ControlService(void* hService, DWORD dwControl, void* lpServiceStatus) {
    (void)hService; (void)dwControl; (void)lpServiceStatus;
    return 0;
}""",
    "THETAZ_ConvertSidToStringSidW": """BOOL WINAPI_CALL THETAZ_ConvertSidToStringSidW(void* Sid, wchar_t* StringSid) {
    (void)Sid; (void)StringSid;
    return 0;
}""",
    "THETAZ_CopyRect": """int WINAPI_CALL THETAZ_CopyRect(void* dst, const void* src) {
    if (dst && src) memcpy(dst, src, 16);
    return 1;
}""",
    "THETAZ_CopySid": """BOOL WINAPI_CALL THETAZ_CopySid(DWORD nDestinationSidLength, void* pDestinationSid, void* pSourceSid) {
    (void)nDestinationSidLength; (void)pDestinationSid; (void)pSourceSid;
    return 1;
}""",
    "THETAZ_CreateActCtxW": """HANDLE WINAPI_CALL THETAZ_CreateActCtxW(void* i){return (HANDLE)(uintptr_t)0x50000;}""",
    "THETAZ_CreateBindCtx": """HRESULT WINAPI_CALL THETAZ_CreateBindCtx(DWORD reserved, void** ppbc) {
    (void)reserved;
    if (ppbc) *ppbc = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_CreateHardLinkW": """BOOL WINAPI_CALL THETAZ_CreateHardLinkW(const wchar_t* l,const wchar_t* t,void* s){return FALSE;}""",
    "THETAZ_CreateJobObjectW": """HANDLE WINAPI_CALL THETAZ_CreateJobObjectW(void* s,const wchar_t* n){return (HANDLE)(uintptr_t)0x30000;}""",
    "THETAZ_CreateNamedPipeW": """BOOL WINAPI_CALL THETAZ_CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                          DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes){return FALSE;}""",
    "THETAZ_CreateProcessA": """BOOL WINAPI_CALL THETAZ_CreateProcessA(LPCSTR application, LPSTR commandline,
                                              void* process, void* thread,
                                              BOOL inherit, DWORD flags, void* env,
                                              LPCSTR current_dir, void* startup_info,
                                              void* process_info) {
    (void)process;
    (void)thread;
    (void)inherit;
    (void)flags;
    (void)env;
    (void)current_dir;
    (void)startup_info;
    if (!process_info) return FALSE_VALUE;

    std::string target = resolve_target(application, commandline);
    if (target.empty()) {
        printf("[kernel32] CreateProcessA: no application or command line\n");
        return FALSE_VALUE;
    }

    PELoader& loader = PELoader::instance();

    // Self-relaunch guard: if the caller is already a child process and the
    // target matches the main module, count how many active copies exist. On
    // real Windows each child gets its own address space; in our in-process
    // emulator they share one, so unbounded copies exhaust the low zone.
    LoadedModule* main_mod = loader.find_module_by_base(get_main_module_base());
    if (main_mod && main_mod->name[0]) {
        const char* mn = main_mod->name;
        size_t mn_len = strlen(mn);
        if (target.size() <= mn_len && strncasecmp(mn + (mn_len - target.size()),
                                                    target.c_str(), target.size()) == 0) {
            uint32_t copy_count = loader.count_modules_named(mn);
            if (copy_count >= 4) {
                printf("[kernel32] CreateProcessA: %u active copies of \"%s\", skipping recursive spawn\n",
                       copy_count, target.c_str());
                return FALSE_VALUE;
            }
        }
    }

    LoadedModule* module = loader.load_image(target.c_str());
    if (!module && target.find(".exe") == std::string::npos) {
        module = loader.load_image((target + ".exe").c_str());
    }
    if (!module) {
        // Self-relaunch: the app truncated its own module path. Load a FRESH
        // copy of the main image (own .data/relocations) so the child runs as a
        // real separate process instead of re-initializing the parent's globals.
        LoadedModule* main_mod = loader.find_module_by_base(get_main_module_base());
        if (main_mod && main_mod->name && strncmp(main_mod->name, target.c_str(), target.size()) == 0) {
            module = loader.load_image(main_mod->name);
            printf("[kernel32] CreateProcessA: self-relaunch, loaded fresh copy at base=0x%lX\n",
                   module ? module->base : 0UL);
        }
    }
    // A 32-bit child must live below 4GB (identity-mapped guest pointers). If
    // the fresh copy fell back to a high host address (every low candidate
    // taken by the parent's image and the Wine DLLs), reload it at one of the
    // dedicated relaunch bases. These sit above the low Wine DLL range and
    // below the guest heap, so they stay free for re-mapped main images.
    if (module && module->base >= 0x100000000ull && target.size() < 512) {
        std::string name = module->name ? module->name : "";
        // Unload the SPECIFIC copy we just loaded (its high host address).
        // Never unload by name: parent and child copies of the same image share
        // a name, so a name-based unload would munmap the parent at 0x400000.
        uint64_t old_base = module->base;
        loader.unload_image(old_base);
        module = nullptr;
        const uint64_t relaunch_bases[] = {0x20000000, 0x30000000, 0x40000000, 0x50000000};
        for (uint64_t rb : relaunch_bases) {
            module = loader.load_image(name.c_str(), rb);
            if (module) {
                printf("[kernel32] CreateProcessA: remapped child to base=0x%lX\n", module->base);
                break;
            }
        }
    }
    if (!module) {
        printf("[kernel32] CreateProcessA: failed to load \"%s\"\n", target.c_str());
        return FALSE_VALUE;
    }
    printf("[kernel32] CreateProcessA: loaded \"%s\" base=0x%lX entry=0x%lX\n",
           module->name, module->base, module->entry_point);

    uint64_t child_pid = alloc_child_pid();
    ThetaServer& server = ThetaServer::instance();
    server.register_process(child_pid, getpid());
    set_guest_process_image_base(child_pid, module->base);

    uint64_t tid = ThreadManager::instance().create_thread(module->entry_point, 0, 0, child_pid);
    if (tid == 0) {
        printf("[kernel32] CreateProcessA: failed to start child thread\n");
        return FALSE_VALUE;
    }
    server.set_process_main_thread(child_pid, tid);

    // The child's return code is reported by thread_entry (thread.cpp) when the
    // guest main() returns, via terminate_process(child_pid, rc). Do NOT mark
    // the child terminated here — that would hand the parent an immediate
    // exit code 0 while the child is still running.

    // PROCESS_INFORMATION layout depends on the guest bitness:
    //   x86 guest:  hProcess @0, hThread @4, dwProcessId @8, dwThreadId @0xC
    //               (16 bytes, all 4-byte fields).
    //   x64 guest:  hProcess @0 (8B), hThread @8 (8B), dwProcessId @16 (4B),
    //               dwThreadId @20 (4B)  (32 bytes).
    // The child pid doubles as the process handle in both cases.
    uint8_t* pi = static_cast<uint8_t*>(process_info);
    if (guest_is_32bit()) {
        *reinterpret_cast<uint32_t*>(pi + 0x00) = static_cast<uint32_t>(child_pid);
        *reinterpret_cast<uint32_t*>(pi + 0x04) = static_cast<uint32_t>(tid);
        *reinterpret_cast<uint32_t*>(pi + 0x08) = static_cast<uint32_t>(child_pid);
        *reinterpret_cast<uint32_t*>(pi + 0x0C) = static_cast<uint32_t>(tid);
    } else {
        *reinterpret_cast<uint64_t*>(pi + 0x00) = static_cast<uint64_t>(child_pid);
        *reinterpret_cast<uint64_t*>(pi + 0x08) = static_cast<uint64_t>(tid);
        *reinterpret_cast<uint32_t*>(pi + 0x10) = static_cast<uint32_t>(child_pid);
        *reinterpret_cast<uint32_t*>(pi + 0x14) = static_cast<uint32_t>(tid);
    }
    return TRUE_VALUE;
}""",
    "THETAZ_CreateProcessAsUserW": """BOOL WINAPI_CALL THETAZ_CreateProcessAsUserW(void* hToken, const wchar_t* lpApplicationName, wchar_t* lpCommandLine,
    void* lpProcessAttributes, void* lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags,
    void* lpEnvironment, const wchar_t* lpCurrentDirectory, void* lpStartupInfo, void* lpProcessInformation) {
    (void)hToken; (void)lpApplicationName; (void)lpCommandLine;
    (void)lpProcessAttributes; (void)lpThreadAttributes; (void)bInheritHandles; (void)dwCreationFlags;
    (void)lpEnvironment; (void)lpCurrentDirectory; (void)lpStartupInfo; (void)lpProcessInformation;
    return 0;
}""",
    "THETAZ_CreateProcessW": """BOOL WINAPI_CALL THETAZ_CreateProcessW(LPCWSTR application, LPWSTR commandline,
                                              void* process, void* thread,
                                              BOOL inherit, DWORD flags, void* env,
                                              LPCWSTR current_dir, void* startup_info,
                                              void* process_info) {
    std::string app = wchar_to_utf8(application);
    std::string cmd = wchar_to_utf8(commandline);
    return THETAZ_CreateProcessA(app.empty() ? nullptr : app.c_str(),
                                 cmd.empty() ? nullptr : const_cast<char*>(cmd.c_str()),
                                 process, thread, inherit, flags, env,
                                 nullptr, startup_info, process_info);
}""",
    "THETAZ_CreateSemaphoreW": """HANDLE WINAPI_CALL THETAZ_CreateSemaphoreW(void* sa,LONG i,LONG m,const wchar_t* n){return (HANDLE)(uintptr_t)0x20000;}""",
    "THETAZ_CreateStreamOnHGlobal": """HRESULT WINAPI_CALL THETAZ_CreateStreamOnHGlobal(void* hGlobal, BOOL fDeleteOnRelease, void** ppstm) {
    (void)hGlobal; (void)fDeleteOnRelease;
    if (ppstm) *ppstm = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_CreateWellKnownSid": """BOOL WINAPI_CALL THETAZ_CreateWellKnownSid(DWORD WellKnownSidType, void* pDomainSid,
    void* pSid, DWORD* cbSid) {
    (void)WellKnownSidType; (void)pDomainSid; (void)pSid; (void)cbSid;
    return 0;
}""",
    "THETAZ_CredEnumerateW": """int WINAPI_CALL THETAZ_CredEnumerateW(const uint16_t* filter, uint32_t flags, uint32_t* count, void** credentials) {
    if (count) *count = 0;
    if (credentials) *credentials = nullptr;
    return 0;
}""",
    "THETAZ_CredFree": """int WINAPI_CALL THETAZ_CredFree(void* buffer) { return 1; }""",
    "THETAZ_CredWriteW": """int WINAPI_CALL THETAZ_CredWriteW(const void* cred, uint32_t flags) { return 0; }""",
    "THETAZ_CryptAcquireContextW": """BOOL WINAPI_CALL THETAZ_CryptAcquireContextW(uint64_t* phProv, const wchar_t* szContainer,
    const wchar_t* szProvider, DWORD dwProvType, DWORD dwFlags) {
    (void)szContainer; (void)szProvider; (void)dwProvType; (void)dwFlags;
    if (phProv) *phProv = 0x1000;
    return 1;
}""",
    "THETAZ_CryptContextAddRef": """BOOL WINAPI_CALL THETAZ_CryptContextAddRef(uint64_t hProv, DWORD* pdwReserved, DWORD dwFlags) {
    (void)hProv; (void)pdwReserved; (void)dwFlags;
    return 1;
}""",
    "THETAZ_CryptCreateHash": """BOOL WINAPI_CALL THETAZ_CryptCreateHash(uint64_t hProv, DWORD Algid, uint64_t hKey, DWORD dwFlags, uint64_t* phHash) {
    (void)hProv; (void)Algid; (void)hKey; (void)dwFlags;
    if (phHash) *phHash = 0x2000;
    return 1;
}""",
    "THETAZ_CryptDecrypt": """BOOL WINAPI_CALL THETAZ_CryptDecrypt(uint64_t hKey, uint64_t hHash, BOOL final, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen) {
    (void)hKey; (void)hHash; (void)final; (void)dwFlags;
    (void)pbData; (void)pdwDataLen;
    return 0;
}""",
    "THETAZ_CryptDeriveKey": """BOOL WINAPI_CALL THETAZ_CryptDeriveKey(uint64_t hProv, DWORD Algid, uint64_t hBaseData, DWORD dwFlags, uint64_t* phKey) {
    (void)hProv; (void)Algid; (void)hBaseData; (void)dwFlags;
    if (phKey) *phKey = 0x3000;
    return 1;
}""",
    "THETAZ_CryptDestroyHash": """BOOL WINAPI_CALL THETAZ_CryptDestroyHash(uint64_t hHash) {
    (void)hHash;
    return 1;
}""",
    "THETAZ_CryptDestroyKey": """BOOL WINAPI_CALL THETAZ_CryptDestroyKey(uint64_t hKey) {
    (void)hKey;
    return 1;
}""",
    "THETAZ_CryptEncrypt": """BOOL WINAPI_CALL THETAZ_CryptEncrypt(uint64_t hKey, uint64_t hHash, BOOL final, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen, DWORD dwBufLen) {
    (void)hKey; (void)hHash; (void)final; (void)dwFlags;
    (void)pbData; (void)pdwDataLen; (void)dwBufLen;
    return 0;
}""",
    "THETAZ_CryptEnumProvidersW": """BOOL WINAPI_CALL THETAZ_CryptEnumProvidersW(DWORD dwIndex, DWORD* pdwReserved, DWORD dwFlags,
    DWORD* pdwProvType, wchar_t* szProvName, LPDWORD pcchProvName) {
    (void)dwIndex; (void)pdwReserved; (void)dwFlags;
    (void)pdwProvType; (void)szProvName; (void)pcchProvName;
    return 0;
}""",
    "THETAZ_CryptExportKey": """BOOL WINAPI_CALL THETAZ_CryptExportKey(uint64_t hKey, uint64_t hExpKey, DWORD dwBlobType, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen) {
    (void)hKey; (void)hExpKey; (void)dwBlobType; (void)dwFlags;
    (void)pbData; (void)pdwDataLen;
    return 0;
}""",
    "THETAZ_CryptGenKey": """BOOL WINAPI_CALL THETAZ_CryptGenKey(uint64_t hProv, DWORD Algid, DWORD dwFlags, uint64_t* phKey) {
    (void)hProv; (void)Algid; (void)dwFlags;
    if (phKey) *phKey = 0x3000;
    return 1;
}""",
    "THETAZ_CryptGenRandom": """BOOL WINAPI_CALL THETAZ_CryptGenRandom(uint64_t hProv, DWORD dwLen, uint8_t* pbBuffer) {
    (void)hProv;
    if (pbBuffer && dwLen > 0) memset(pbBuffer, 0x42, dwLen);
    return 1;
}""",
    "THETAZ_CryptGetHashParam": """BOOL WINAPI_CALL THETAZ_CryptGetHashParam(uint64_t hHash, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags) {
    (void)hHash; (void)dwParam; (void)pbData; (void)pcbData; (void)dwFlags;
    return 1;
}""",
    "THETAZ_CryptGetKeyParam": """BOOL WINAPI_CALL THETAZ_CryptGetKeyParam(uint64_t hKey, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags) {
    (void)hKey; (void)dwParam; (void)pbData; (void)pcbData; (void)dwFlags;
    return 0;
}""",
    "THETAZ_CryptGetProvParam": """BOOL WINAPI_CALL THETAZ_CryptGetProvParam(uint64_t hProv, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags) {
    (void)hProv; (void)dwParam; (void)pbData; (void)pcbData; (void)dwFlags;
    return 0;
}""",
    "THETAZ_CryptGetUserKey": """BOOL WINAPI_CALL THETAZ_CryptGetUserKey(uint64_t hProv, DWORD dwKeySpec, uint64_t* phKey) {
    (void)hProv; (void)dwKeySpec;
    if (phKey) *phKey = 0x3000;
    return 1;
}""",
    "THETAZ_CryptHashData": """BOOL WINAPI_CALL THETAZ_CryptHashData(uint64_t hHash, const uint8_t* pbData, DWORD dwDataLen, DWORD dwFlags) {
    (void)hHash; (void)pbData; (void)dwDataLen; (void)dwFlags;
    return 1;
}""",
    "THETAZ_CryptImportKey": """BOOL WINAPI_CALL THETAZ_CryptImportKey(uint64_t hProv, const uint8_t* pbData, DWORD dwDataLen,
    uint64_t hPubKey, DWORD dwFlags, uint64_t* phKey) {
    (void)hProv; (void)pbData; (void)dwDataLen; (void)hPubKey; (void)dwFlags;
    if (phKey) *phKey = 0x3000;
    return 1;
}""",
    "THETAZ_CryptProtectMemory": """BOOL WINAPI_CALL THETAZ_CryptProtectMemory(void* pvData, DWORD cbData, DWORD dwFlags) {
    (void)pvData; (void)cbData; (void)dwFlags;
    return 1;
}""",
    "THETAZ_CryptReleaseContext": """BOOL WINAPI_CALL THETAZ_CryptReleaseContext(uint64_t hProv, DWORD dwFlags) {
    (void)hProv; (void)dwFlags;
    return 1;
}""",
    "THETAZ_CryptSetProvParam": """BOOL WINAPI_CALL THETAZ_CryptSetProvParam(uint64_t hProv, DWORD dwParam, const uint8_t* pbData, DWORD dwFlags) {
    (void)hProv; (void)dwParam; (void)pbData; (void)dwFlags;
    return 0;
}""",
    "THETAZ_CryptSignHashW": """BOOL WINAPI_CALL THETAZ_CryptSignHashW(uint64_t hHash, DWORD dwKeySpec, const wchar_t* szDescription,
    DWORD dwFlags, uint8_t* pbSignature, LPDWORD pdwSigLen) {
    (void)hHash; (void)dwKeySpec; (void)szDescription; (void)dwFlags;
    (void)pbSignature; (void)pdwSigLen;
    return 0;
}""",
    "THETAZ_CryptUIDlgSelectCertificateFromStore": """int WINAPI_CALL THETAZ_CryptUIDlgSelectCertificateFromStore(void* hCertStore, void* hwnd,
    void* pwszTitle, void* pwszDisplayString, DWORD dwDontUseColumn, DWORD dwFlags, void* pvReserved) {
    (void)hCertStore; (void)hwnd; (void)pwszTitle; (void)pwszDisplayString;
    (void)dwDontUseColumn; (void)dwFlags; (void)pvReserved;
    return 0;
}""",
    "THETAZ_CryptUnprotectMemory": """BOOL WINAPI_CALL THETAZ_CryptUnprotectMemory(void* pvData, DWORD cbData, DWORD dwFlags) {
    (void)pvData; (void)cbData; (void)dwFlags;
    return 1;
}""",
    "THETAZ_CryptVerifySignatureW": """BOOL WINAPI_CALL THETAZ_CryptVerifySignatureW(uint64_t hHash, const uint8_t* pbSignature, DWORD dwSigLen,
    uint64_t hPubKey, const wchar_t* szDescription, DWORD dwFlags) {
    (void)hHash; (void)pbSignature; (void)dwSigLen;
    (void)hPubKey; (void)szDescription; (void)dwFlags;
    return 0;
}""",
    "THETAZ_DPtoLP": """int WINAPI_CALL THETAZ_DPtoLP(uint32_t hdc, void* pt, int count) {
    return 1;
}""",
    "THETAZ_DeactivateActCtx": """BOOL WINAPI_CALL THETAZ_DeactivateActCtx(DWORD f,ULONG_PTR k){return TRUE;}""",
    "THETAZ_DebugBreak": """void WINAPI_CALL THETAZ_DebugBreak(){}""",
    "THETAZ_DefWindowProcW": """LRESULT WINAPI_CALL THETAZ_DefWindowProcW(uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    using DefWindowProcA_fn = LRESULT(WINAPI_CALL*)(uint32_t, uint32_t, WPARAM, LPARAM);
    auto* fn = (DefWindowProcA_fn)ApiRegistry::instance().find_function("user32.dll", "DefWindowProcA");
    if (!fn) return 0;
    return fn(hwnd, msg, wParam, lParam);
}""",
    "THETAZ_DeleteService": """BOOL WINAPI_CALL THETAZ_DeleteService(void* hService) {
    (void)hService;
    return 0;
}""",
    "THETAZ_DestroyCaret": """int WINAPI_CALL THETAZ_DestroyCaret() { return 1; }""",
    "THETAZ_DestroyIcon": """int WINAPI_CALL THETAZ_DestroyIcon(uint32_t hicon) { return 1; }""",
    "THETAZ_DestroyMenu": """BOOL WINAPI_CALL THETAZ_DestroyMenu(void* hMenu){
    (void)hMenu;
    return TRUE;
}""",
    "THETAZ_DestroyWindow": """BOOL WINAPI_CALL THETAZ_DestroyWindow(WinHandle hwnd) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    uint64_t wnd_proc = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(h);
        if (!win) return FALSE_VALUE;
        wnd_proc = win->wnd_proc;
    }
    call_wnd_proc(wnd_proc, h, WM_DESTROY, 0, 0);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        // Destroy children (X11 subwindows die with the parent, but our
        // registry needs cleaning).
        for (auto it = g_windows.begin(); it != g_windows.end();) {
            if (it->second.parent == h) {
                auto xit = g_x11_windows.find(it->first);
                if (xit != g_x11_windows.end()) {
                    g_x11_hwnds.erase(xit->second);
                    g_x11_windows.erase(xit);
                }
                it = g_windows.erase(it);
            } else {
                ++it;
            }
        }
        g_windows.erase(h);
        auto xit = g_x11_windows.find(h);
        if (xit != g_x11_windows.end()) {
            g_x11_hwnds.erase(xit->second);
            g_x11_windows.erase(xit);
        }
    }
    return TRUE_VALUE;
}""",
    "THETAZ_DestroyWindow_Wrapper": """BOOL WINAPI_CALL THETAZ_DestroyWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) {
        return THETAZ_DestroyWindow(hwnd);
    } else {
        return WINAPI64_DestroyWindow(hwnd);
    }
}""",
    "THETAZ_DialogBoxParamA": """int WINAPI_CALL THETAZ_DialogBoxParamA(uint32_t hinst, const char* dlg, uint32_t parent, void* proc, LPARAM init) { return -1; }""",
    "THETAZ_DialogBoxParamA_Wrapper": """int WINAPI_CALL THETAZ_DialogBoxParamA_Wrapper(WinHandle hInstance, const char* name,
                                                      WinHandle hWndParent, void* lpDialogFunc,
                                                      uint64_t dwInitParam) {
    if (x86_dispatch_active()) return THETAZ_DialogBoxParamA(hInstance, name, hWndParent, lpDialogFunc, dwInitParam);
    return WINAPI64_DialogBoxParamA(hInstance, name, hWndParent, lpDialogFunc, dwInitParam);
}""",
    "THETAZ_DialogBoxParamW": """int WINAPI_CALL THETAZ_DialogBoxParamW(uint32_t hinst, const uint16_t* dlg, uint32_t parent, void* proc, LPARAM init) { return -1; }""",
    "THETAZ_DisconnectNamedPipe": """BOOL WINAPI_CALL THETAZ_DisconnectNamedPipe(HANDLE p){return TRUE;}""",
    "THETAZ_DispatchMessageW": """int WINAPI_CALL THETAZ_DispatchMessageW(const void* msg) {
    if (!msg) return 0;
    uint32_t hwnd = *(const uint32_t*)((const uint8_t*)msg);
    if (!hwnd) return 0;
    return 0;
}""",
    "THETAZ_DosDateTimeToFileTime": """BOOL WINAPI_CALL THETAZ_DosDateTimeToFileTime(WORD d,WORD t,FILETIME* f){if(f){f->dwLowDateTime=f->dwHighDateTime=0;}return TRUE;}""",
    "THETAZ_DrawEdge": """int WINAPI_CALL THETAZ_DrawEdge(uint32_t hdc, void* rect, uint32_t edge, uint32_t flags) { return 1; }""",
    "THETAZ_DrawFocusRect": """int WINAPI_CALL THETAZ_DrawFocusRect(uint32_t hdc, const void* rect) { return 1; }""",
    "THETAZ_DrawFrameControl": """int WINAPI_CALL THETAZ_DrawFrameControl(uint32_t hdc, void* rect, uint32_t type, uint32_t state) { return 1; }""",
    "THETAZ_DrawIcon": """int WINAPI_CALL THETAZ_DrawIcon(uint32_t hdc, int x, int y, uint32_t hicon) { return 1; }""",
    "THETAZ_DrawIconEx": """int WINAPI_CALL THETAZ_DrawIconEx(uint32_t hdc, int x, int y, uint32_t hicon, int w, int h, uint32_t step, uint32_t hbr, uint32_t flags) { return 1; }""",
    "THETAZ_DrawTextA": """int WINAPI_CALL THETAZ_DrawTextA(uint32_t hdc, const char* str, int len, void* rect, uint32_t format) {
    if (len == -1) len = str ? (int)strlen(str) : 0;
    return len;
}""",
    "THETAZ_DrawTextW": """int WINAPI_CALL THETAZ_DrawTextW(uint32_t hdc, const uint16_t* str, int len, void* rect, uint32_t format) {
    if (len == -1) len = str ? wstrlen(str) : 0;
    return len;
}""",
    "THETAZ_DuplicateToken": """BOOL WINAPI_CALL THETAZ_DuplicateToken(HANDLE ExistingTokenHandle, DWORD ImpersonationLevel, HANDLE* DuplicateTokenHandle) {
    (void)ExistingTokenHandle; (void)ImpersonationLevel; (void)DuplicateTokenHandle;
    return 0;
}""",
    "THETAZ_EnableScrollBar": """int WINAPI_CALL THETAZ_EnableScrollBar(uint32_t hwnd, uint32_t bar, uint32_t flags) { return 1; }""",
    "THETAZ_EnableWindow": """BOOL WINAPI_CALL THETAZ_EnableWindow(WinHandle hwnd, BOOL enable) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(h);
    if (!win) return FALSE_VALUE;
    BOOL was = win->enabled ? TRUE_VALUE : FALSE_VALUE;
    win->enabled = (enable != 0);
    return was;
}""",
    "THETAZ_EnableWindow_Wrapper": """BOOL WINAPI_CALL THETAZ_EnableWindow_Wrapper(WinHandle hwnd, BOOL enable) {
    if (x86_dispatch_active()) return THETAZ_EnableWindow(hwnd, enable);
    return WINAPI64_EnableWindow(hwnd, enable);
}""",
    "THETAZ_EndDialog": """BOOL WINAPI_CALL THETAZ_EndDialog(WinHandle hdlg, int result) {
    uint64_t h = reinterpret_cast<uint64_t>(hdlg);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_dialog_result_active = true;
        g_dialog_result = result;
        auto xit = g_x11_windows.find(h);
        if (xit != g_x11_windows.end()) {
            g_x11_hwnds.erase(xit->second);
            g_x11_windows.erase(xit);
        }
        g_windows.erase(h);
    }
    return TRUE_VALUE;
}""",
    "THETAZ_EndDialog_Wrapper": """BOOL WINAPI_CALL THETAZ_EndDialog_Wrapper(WinHandle hdlg, int result) {
    if (x86_dispatch_active()) return THETAZ_EndDialog(hdlg, result);
    return WINAPI64_EndDialog(hdlg, result);
}""",
    "THETAZ_EndPaint": """BOOL WINAPI_CALL THETAZ_EndPaint(WinHandle hwnd, const void* ps) {
    (void)hwnd;
    (void)ps;
    return TRUE_VALUE;
}""",
    "THETAZ_EndPaint_Wrapper": """BOOL WINAPI_CALL THETAZ_EndPaint_Wrapper(WinHandle hwnd, const void* ps) {
    if (x86_dispatch_active()) {
        return THETAZ_EndPaint(hwnd, ps);
    } else {
        return WINAPI64_EndPaint(hwnd, ps);
    }
}""",
    "THETAZ_EnumCalendarInfoA": """BOOL WINAPI_CALL THETAZ_EnumCalendarInfoA(void* p,LCID l,CALID c,CALINFO i,DWORD f){return FALSE;}""",
    "THETAZ_EnumCalendarInfoExEx": """BOOL WINAPI_CALL THETAZ_EnumCalendarInfoExEx(void* p,const wchar_t* f,LCID l,CALID c,DWORD i,DWORD fl,int64_t lp){return FALSE;}""",
    "THETAZ_EnumChildWindows": """BOOL WINAPI_CALL THETAZ_EnumChildWindows(void* hWnd,void* lpEnumFunc,long lParam){
    (void)hWnd;(void)lpEnumFunc;(void)lParam;
    return TRUE;
}""",
    "THETAZ_EnumDateFormatsExEx": """BOOL WINAPI_CALL THETAZ_EnumDateFormatsExEx(void* p,const wchar_t* l,DWORD f,int64_t lp){return FALSE;}""",
    "THETAZ_EnumDependentServicesW": """BOOL WINAPI_CALL THETAZ_EnumDependentServicesW(void* hService, DWORD dwServiceState,
    void* lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned) {
    (void)hService; (void)dwServiceState; (void)lpServices; (void)cbBufSize;
    if (pcbBytesNeeded) *pcbBytesNeeded = 0;
    if (lpServicesReturned) *lpServicesReturned = 0;
    return 0;
}""",
    "THETAZ_EnumResourceNamesW": """BOOL WINAPI_CALL THETAZ_EnumResourceNamesW(HMODULE h,const wchar_t* t,void* cb,LONG_PTR p){return TRUE;}""",
    "THETAZ_EnumSystemLanguageGroups": """BOOL WINAPI_CALL THETAZ_EnumSystemLanguageGroups(void* p,DWORD f,int64_t lp){return FALSE;}""",
    "THETAZ_EnumTimeFormatsEx": """BOOL WINAPI_CALL THETAZ_EnumTimeFormatsEx(void* p,const wchar_t* l,DWORD f,int64_t lp){return FALSE;}""",
    "THETAZ_EnumUILanguagesW": """BOOL WINAPI_CALL THETAZ_EnumUILanguagesW(void* p,DWORD f,int64_t lp){return FALSE;}""",
    "THETAZ_EqualSid": """BOOL WINAPI_CALL THETAZ_EqualSid(void* pSid1, void* pSid2) {
    (void)pSid1; (void)pSid2;
    return 0;
}""",
    "THETAZ_EventRegister": """DWORD WINAPI_CALL THETAZ_EventRegister(GUID* pProviderId, void* pEnableCallback, void* pCallbackContext, uint64_t* regHandle) {
    (void)pProviderId; (void)pEnableCallback; (void)pCallbackContext;
    if (regHandle) *regHandle = 0;
    return 0;
}""",
    "THETAZ_EventSetInformation": """DWORD WINAPI_CALL THETAZ_EventSetInformation(uint64_t regHandle, uint32_t infoClass, void* pInfo, uint16_t infoLength) {
    (void)regHandle; (void)infoClass; (void)pInfo; (void)infoLength;
    return 0;
}""",
    "THETAZ_EventUnregister": """DWORD WINAPI_CALL THETAZ_EventUnregister(uint64_t regHandle) {
    (void)regHandle;
    return 0;
}""",
    "THETAZ_EventWriteTransfer": """DWORD WINAPI_CALL THETAZ_EventWriteTransfer(uint64_t regHandle, void* pActivityId, void* pRelatedActivityId,
    uint64_t descriptor, uint32_t userDataCount, void* userData) {
    (void)regHandle; (void)pActivityId; (void)pRelatedActivityId;
    (void)descriptor; (void)userDataCount; (void)userData;
    return 0;
}""",
    "THETAZ_ExcludeClipRect": """int WINAPI_CALL THETAZ_ExcludeClipRect(uint32_t hdc, int l, int t, int r, int b) {
    return 1;
}""",
    "THETAZ_ExitProcess": """void WINAPI_CALL THETAZ_ExitProcess(uint32_t exit_code) {
    terminate_current_process(exit_code);
}""",
    "THETAZ_ExitThread": """void WINAPI_CALL THETAZ_ExitThread(DWORD dwExitCode) {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    if (pid == 0 && ThreadManager::instance().get_current_thread() <= 1) {
        terminate_current_process(dwExitCode);
    }
    ThreadManager::instance().exit_thread(dwExitCode);
}""",
    "THETAZ_ExtTextOutW": """int WINAPI_CALL THETAZ_ExtTextOutW(uint32_t hdc, int x, int y, uint32_t options, const void* rect, const uint16_t* str, uint32_t count, const int32_t* dx) {
    return 1;
}""",
    "THETAZ_FileTimeToDosDateTime": """BOOL WINAPI_CALL THETAZ_FileTimeToDosDateTime(const FILETIME* f,WORD* d,WORD* t){if(d)*d=0;if(t)*t=0;return TRUE;}""",
    "THETAZ_FileTimeToLocalFileTime": """BOOL WINAPI_CALL THETAZ_FileTimeToLocalFileTime(const FILETIME* f,FILETIME* l){if(l&&f)*l=*f;return TRUE;}""",
    "THETAZ_FillRect": """int WINAPI_CALL THETAZ_FillRect(uint32_t hdc, const void* rect, uint32_t hbr) { return 1; }""",
    "THETAZ_FillRgn": """int WINAPI_CALL THETAZ_FillRgn(uint32_t hdc, uint32_t hrgn, uint32_t hbr) {
    return 1;
}""",
    "THETAZ_FindActCtxSectionStringW": """BOOL WINAPI_CALL THETAZ_FindActCtxSectionStringW(DWORD f,const GUID* g,ULONG s,const wchar_t* str,void* r){return FALSE;}""",
    "THETAZ_FindAtomA": """ATOM WINAPI_CALL THETAZ_FindAtomA(const char* n){return (ATOM)(uintptr_t)(n+0x10000);}""",
    "THETAZ_FindAtomW": """ATOM WINAPI_CALL THETAZ_FindAtomW(const wchar_t* n){return (ATOM)(uintptr_t)((const char*)n+0x10000);}""",
    "THETAZ_FindNLSStringEx": """int WINAPI_CALL THETAZ_FindNLSStringEx(const wchar_t* l,DWORD f,const wchar_t* s1,int l1,const wchar_t* s2,int l2,int* u,void* c,void* r,int so){return -1;}""",
    "THETAZ_FindNextChangeNotification": """int WINAPI_CALL THETAZ_FindNextChangeNotification(void* hChangeHandle){
    (void)hChangeHandle;
    return TRUE;
}""",
    "THETAZ_FindPackagesByPackageFamily": """HRESULT WINAPI_CALL THETAZ_FindPackagesByPackageFamily(const wchar_t* f,uint32_t* c,wchar_t** b,uint32_t* bl,wchar_t* b2){if(c)*c=0;return 0x80073D54;}""",
    "THETAZ_FlushInstructionCache": """BOOL WINAPI_CALL THETAZ_FlushInstructionCache(HANDLE p,const void* a,S s){return TRUE;}""",
    "THETAZ_FlushProcessWriteBuffers": """BOOL WINAPI_CALL THETAZ_FlushProcessWriteBuffers(){return TRUE;}""",
    "THETAZ_FoldStringW": """int WINAPI_CALL THETAZ_FoldStringW(DWORD f,const wchar_t* s,int sl,wchar_t* d,int dl){
    if(d&&s&&dl>=sl){memcpy(d,s,sl*2);return sl;}return sl;
}""",
    "THETAZ_FrameRect": """int WINAPI_CALL THETAZ_FrameRect(uint32_t hdc, const void* rect, uint32_t hbr) { return 1; }""",
    "THETAZ_FrameRgn": """int WINAPI_CALL THETAZ_FrameRgn(uint32_t hdc, uint32_t hrgn, uint32_t hbr, int w, int h) {
    return 1;
}""",
    "THETAZ_FreeAddrInfoW": """void WINAPI_CALL THETAZ_FreeAddrInfoW(void* pAddrInfo){
    (void)pAddrInfo;
}""",
    "THETAZ_FreeLibraryAndExitThread": """void WINAPI_CALL THETAZ_FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode) {
    (void)hLibModule; // builtin modules are never unloaded
    THETAZ_ExitThread(dwExitCode);
}""",
    "THETAZ_FreeLibraryWhenCallbackReturns": """void WINAPI_CALL THETAZ_FreeLibraryWhenCallbackReturns(void* m,void* c){}""",
    "THETAZ_GdiAlphaBlend": """int WINAPI_CALL THETAZ_GdiAlphaBlend(uint32_t hdcDst, int xdst, int ydst, int wdst, int hdst, uint32_t hdcSrc, int xsrc, int ysrc, int wsrc, int hsrc, uint32_t blend) {
    return 1;
}""",
    "THETAZ_GetAcceptLanguagesA": """HRESULT WINAPI_CALL THETAZ_GetAcceptLanguagesA(char* b,DWORD* l){if(b&&l&&*l>0){strncpy(b,"en-US",*l);*l=6;}return S_OK;}""",
    "THETAZ_GetAcceptLanguagesW": """HRESULT WINAPI_CALL THETAZ_GetAcceptLanguagesW(wchar_t* b,DWORD* l){if(b&&l&&*l>0){wcsncpy(b,L"en-US",*l);*l=6;}return S_OK;}""",
    "THETAZ_GetAce": """BOOL WINAPI_CALL THETAZ_GetAce(void* a,DWORD i,void* ace){return FALSE;}""",
    "THETAZ_GetAtomNameA": """UINT WINAPI_CALL THETAZ_GetAtomNameA(ATOM a,char* b,int s){if(b&&s>0)b[0]=0;return 0;}""",
    "THETAZ_GetAtomNameW": """UINT WINAPI_CALL THETAZ_GetAtomNameW(ATOM a,wchar_t* b,int s){if(b&&s>0)b[0]=0;return 0;}""",
    "THETAZ_GetBkMode": """int WINAPI_CALL THETAZ_GetBkMode(uint32_t hdc) {
    auto it = g_gdi_dcs.find(hdc);
    return it != g_gdi_dcs.end() ? it->second.bk_mode : 1;
}""",
    "THETAZ_GetCalendarInfoEx": """int WINAPI_CALL THETAZ_GetCalendarInfoEx(const wchar_t* n,LCID l,const SYSTEMTIME* s,CALINFO o,wchar_t* b,int bl,DWORD* v){
    if(b&&bl>0)wcscpy(b,L"Gregorian");if(v)*v=0;return 0;
}""",
    "THETAZ_GetCalendarInfoW": """int WINAPI_CALL THETAZ_GetCalendarInfoW(LCID l,CALID c,CALINFO o,wchar_t* b,int bl,DWORD* v){
    if(b&&bl>0)wcscpy(b,L"Gregorian");if(v)*v=0;return 0;
}""",
    "THETAZ_GetClassFile": """HRESULT WINAPI_CALL THETAZ_GetClassFile(void* szFilename, void** ppsclsid) {
    (void)szFilename;
    if (ppsclsid) *ppsclsid = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_GetClassNameW": """int WINAPI_CALL THETAZ_GetClassNameW(uint32_t hwnd, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}""",
    "THETAZ_GetClipBox": """int WINAPI_CALL THETAZ_GetClipBox(uint32_t hdc, void* rect) {
    if (rect) memset(rect, 0, 16);
    return 1;
}""",
    "THETAZ_GetClipRgn": """int WINAPI_CALL THETAZ_GetClipRgn(uint32_t hdc, uint32_t hrgn) {
    return 0;
}""",
    "THETAZ_GetComputerNameExA": """BOOL WINAPI_CALL THETAZ_GetComputerNameExA(int NameType,char* lpBuffer,LPDWORD nSize){
    if(lpBuffer&&nSize&&*nSize>0){strcpy(lpBuffer,"THETAPC");*nSize=strlen("THETAPC");return TRUE;}
    return FALSE;
}""",
    "THETAZ_GetConvertStg": """HRESULT WINAPI_CALL THETAZ_GetConvertStg(void* pStg) {
    (void)pStg;
    return E_NOTIMPL;
}""",
    "THETAZ_GetCurrencyFormatEx": """int WINAPI_CALL THETAZ_GetCurrencyFormatEx(const wchar_t* l,DWORD f,const wchar_t* v,const void* fmt,wchar_t* b,int bl){
    if(b&&bl>0){wcscpy(b,v?v:L"0");return (int)wcslen(b);}return 0;
}""",
    "THETAZ_GetCurrentActCtx": """BOOL WINAPI_CALL THETAZ_GetCurrentActCtx(HANDLE* c){if(c)*c=NULL;return FALSE;}""",
    "THETAZ_GetCurrentHwProfileA": """BOOL WINAPI_CALL THETAZ_GetCurrentHwProfileA(void* pHwProfileInfo) {
    (void)pHwProfileInfo;
    return 0;
}""",
    "THETAZ_GetCurrentPackageFullName": """HRESULT WINAPI_CALL THETAZ_GetCurrentPackageFullName(uint32_t* l,wchar_t* n){if(l)*l=0;return 0x80073D54;}""",
    "THETAZ_GetCurrentPackagePath": """HRESULT WINAPI_CALL THETAZ_GetCurrentPackagePath(uint32_t* l,wchar_t* p){if(l)*l=0;return 0x80073D54;}""",
    "THETAZ_GetCurrentProcessId": """DWORD WINAPI_CALL THETAZ_GetCurrentProcessId() {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    return static_cast<DWORD>(pid == 0 ? static_cast<uint64_t>(getpid()) : pid);
}""",
    "THETAZ_GetCurrentProcessorNumber": """DWORD WINAPI_CALL THETAZ_GetCurrentProcessorNumber(){return 0;}""",
    "THETAZ_GetCursorPos": """int WINAPI_CALL THETAZ_GetCursorPos(void* pt) {
    if (pt) memset(pt, 0, 8);
    return 1;
}""",
    "THETAZ_GetDIBits": """int WINAPI_CALL THETAZ_GetDIBits(uint32_t hdc, uint32_t hbm, uint32_t start, uint32_t cLines, void* lpvBits, void* lpbmi, uint32_t usage) {
    return 0;
}""",
    "THETAZ_GetDateFormatA": """int WINAPI_CALL THETAZ_GetDateFormatA(LCID l,DWORD f,const SYSTEMTIME* s,const char* fmt,char* b,int bl){
    if(b&&bl>0){strcpy(b,"01/01/2024");return 10;}return 10;
}""",
    "THETAZ_GetDlgCtrlID": """int WINAPI_CALL THETAZ_GetDlgCtrlID(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return win ? static_cast<int>(win->id) : 0;
}""",
    "THETAZ_GetDlgCtrlID_Wrapper": """int WINAPI_CALL THETAZ_GetDlgCtrlID_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_GetDlgCtrlID(hwnd);
    return WINAPI64_GetDlgCtrlID(hwnd);
}""",
    "THETAZ_GetDlgItemTextA": """int WINAPI_CALL THETAZ_GetDlgItemTextA(WinHandle hdlg, int id, char* buf, int max) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl || !buf || max <= 0) return 0;
    uint64_t r = THETAZ_SendMessageA(ctrl, WM_GETTEXT, static_cast<uint64_t>(max),
                                     reinterpret_cast<uint64_t>(buf));
    return static_cast<int>(r);
}""",
    "THETAZ_GetDlgItemTextA_Wrapper": """int WINAPI_CALL THETAZ_GetDlgItemTextA_Wrapper(WinHandle hdlg, int id, char* buf, int max) {
    if (x86_dispatch_active()) return THETAZ_GetDlgItemTextA(hdlg, id, buf, max);
    return WINAPI64_GetDlgItemTextA(hdlg, id, buf, max);
}""",
    "THETAZ_GetDynamicTimeZoneInformation": """DWORD WINAPI_CALL THETAZ_GetDynamicTimeZoneInformation(void* t){if(t)memset(t,0,144);return 0;}""",
    "THETAZ_GetEnabledXStateFeatures": """BOOL WINAPI_CALL THETAZ_GetEnabledXStateFeatures(){return 0;}""",
    "THETAZ_GetErrorInfo": """LONG WINAPI_CALL THETAZ_GetErrorInfo(DWORD dwReserved, void** pperrinfo) {
    (void)dwReserved;
    if (pperrinfo) *pperrinfo = nullptr;
    return 0; // S_OK
}""",
    "THETAZ_GetErrorMode": """DWORD WINAPI_CALL THETAZ_GetErrorMode(){return 0;}""",
    "THETAZ_GetExitCodeProcess": """BOOL WINAPI_CALL THETAZ_GetExitCodeProcess(HANDLE process, DWORD* code) {
    if (!code) return FALSE_VALUE;
    ProcessObject* obj = ThetaServer::instance().get_process(reinterpret_cast<uint64_t>(process));
    if (!obj) {
        *code = 0;
        return FALSE_VALUE;
    }
    *code = obj->terminated ? obj->exit_code : 0x103; // STILL_ACTIVE
    return TRUE_VALUE;
}""",
    "THETAZ_GetFileSecurityW": """BOOL WINAPI_CALL THETAZ_GetFileSecurityW(const wchar_t* lpFileName, DWORD SecurityInformation,
    void* pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded) {
    (void)lpFileName; (void)SecurityInformation; (void)pSecurityDescriptor; (void)nLength;
    if (lpnLengthNeeded) *lpnLengthNeeded = 0;
    return 0;
}""",
    "THETAZ_GetFileVersionInfoSizeExW": """DWORD WINAPI_CALL THETAZ_GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    (void)dwFlags; (void)lptstrFilename;
    if (lpdwHandle) *lpdwHandle = 0;
    return 0;
}""",
    "THETAZ_GetFileVersionInfoSizeW": """DWORD WINAPI_CALL THETAZ_GetFileVersionInfoSizeW(const wchar_t* n,DWORD* h){if(h)*h=0;return 0;}""",
    "THETAZ_GetFileVersionInfoW": """BOOL WINAPI_CALL THETAZ_GetFileVersionInfoW(const wchar_t* n,DWORD h,DWORD sz,void* d){return FALSE;}""",
    "THETAZ_GetGeoInfoW": """int WINAPI_CALL THETAZ_GetGeoInfoW(int g,int t,wchar_t* b,int bl,LID la){if(b&&bl>0){wcscpy(b,L"US");return 2;}return 0;}""",
    "THETAZ_GetIconInfo": """int WINAPI_CALL THETAZ_GetIconInfo(uint32_t hicon, void* info) {
    if (info) memset(info, 0, 20);
    return 1;
}""",
    "THETAZ_GetKeyNameTextW": """int WINAPI_CALL THETAZ_GetKeyNameTextW(int32_t lParam, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}""",
    "THETAZ_GetLengthSid": """DWORD WINAPI_CALL THETAZ_GetLengthSid(void* pSid) {
    (void)pSid;
    return 12;
}""",
    "THETAZ_GetLogicalDrives": """DWORD WINAPI_CALL THETAZ_GetLogicalDrives() { return 0x03; }""",
    "THETAZ_GetLogicalProcessorInformationEx": """BOOL WINAPI_CALL THETAZ_GetLogicalProcessorInformationEx(int r,void* b,DWORD* l){if(l)*l=0;return TRUE;}""",
    "THETAZ_GetMessageA": """LONG WINAPI_CALL THETAZ_GetMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    (void)hwnd;
    if (!msg) return -1;

    std::unique_lock<std::mutex> lock(g_window_mutex);
    while (true) {
        if (!g_queue.empty()) {
            GuestMsg m = g_queue.front();
            bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                                  (m.message >= filter_min && m.message <= filter_max);
            if (m.message == WM_QUIT) {
                // WM_QUIT is never filtered and ends the loop.
                g_queue.pop_front();
                g_quit_pending = false;
                write_guest_msg(static_cast<uint8_t*>(msg), m);
                printf("[user32] GetMessageA -> WM_QUIT (%lu)\n", (unsigned long)m.wparam);
                return 0;
            }
            if (matches_filter) {
                g_queue.pop_front();
                write_guest_msg(static_cast<uint8_t*>(msg), m);
                return 1;
            }
            // Doesn't match: leave it queued and wait for a matching message.
        }
        g_queue_cond.wait(lock);
    }
}""",
    "THETAZ_GetMessageA_Wrapper": """LONG WINAPI_CALL THETAZ_GetMessageA_Wrapper(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    if (x86_dispatch_active()) {
        return THETAZ_GetMessageA(msg, hwnd, filter_min, filter_max);
    } else {
        return WINAPI64_GetMessageA(msg, hwnd, filter_min, filter_max);
    }
}""",
    "THETAZ_GetMessageW": """int WINAPI_CALL THETAZ_GetMessageW(void* msg, uint32_t hwnd, uint32_t min, uint32_t max) {
    if (msg) memset(msg, 0, 28);
    return 0;
}""",
    "THETAZ_GetModuleHandleExA": """BOOL WINAPI_CALL THETAZ_GetModuleHandleExA(DWORD flags,const char* name,HMODULE* ph) {
    if(!ph)return FALSE;
    if(!name){*ph=(HMODULE)(uintptr_t)get_main_module_base();return TRUE;}
    for(auto& [base,m]:PELoader::instance().get_modules()){
        std::string mn(m.name);std::string rn(name);
        std::transform(mn.begin(),mn.end(),mn.begin(),::tolower);
        std::transform(rn.begin(),rn.end(),rn.begin(),::tolower);
        if(mn==rn||mn==rn+".dll"){*ph=(HMODULE)(uintptr_t)m.base;return TRUE;}
    }
    return FALSE;
}""",
    "THETAZ_GetMonitorInfoW": """int WINAPI_CALL THETAZ_GetMonitorInfoW(uint32_t hmon, void* info) { return 0; }""",
    "THETAZ_GetNamedPipeClientProcessId": """BOOL WINAPI_CALL THETAZ_GetNamedPipeClientProcessId(HANDLE p,uint64_t* pid){if(pid)*pid=0;return FALSE;}""",
    "THETAZ_GetNamedPipeInfo": """BOOL WINAPI_CALL THETAZ_GetNamedPipeInfo(HANDLE p,DWORD* f,DWORD* o,DWORD* i,DWORD* m){if(f)*f=0;if(o)*o=4096;if(i)*i=4096;if(m)*m=1;return TRUE;}""",
    "THETAZ_GetNumaHighestNodeNumber": """BOOL WINAPI_CALL THETAZ_GetNumaHighestNodeNumber(ULONG* n){if(n)*n=0;return TRUE;}""",
    "THETAZ_GetNumberFormatEx": """int WINAPI_CALL THETAZ_GetNumberFormatEx(const wchar_t* l,DWORD f,const wchar_t* v,const void* fmt,wchar_t* b,int bl){
    if(b&&bl>0){wcscpy(b,v?v:L"0");return (int)wcslen(b);}return 0;
}""",
    "THETAZ_GetObjectA": """int WINAPI_CALL THETAZ_GetObjectA(void* hgdiobj, int cbBuffer, void* lpvObject) {
    (void)hgdiobj; (void)cbBuffer; (void)lpvObject;
    return 0;
}""",
    "THETAZ_GetObjectW": """int WINAPI_CALL THETAZ_GetObjectW(uint32_t hobj, int count, void* buf) {
    if (!buf) return 0;
    memset(buf, 0, count);
    auto it = g_gdi_objects.find(hobj);
    if (it == g_gdi_objects.end()) return 0;
    return count;
}""",
    "THETAZ_GetPackageFamilyName": """HRESULT WINAPI_CALL THETAZ_GetPackageFamilyName(uint64_t* n,uint32_t* l,wchar_t* b){if(l)*l=0;return 0x80073D54;}""",
    "THETAZ_GetPackagePathByFullName": """HRESULT WINAPI_CALL THETAZ_GetPackagePathByFullName(const wchar_t* n,uint32_t* l,wchar_t* p){if(l)*l=0;return 0x80073D54;}""",
    "THETAZ_GetPrivateProfileIntW": """int WINAPI_CALL THETAZ_GetPrivateProfileIntW(const wchar_t* s,const wchar_t* k,int d,const wchar_t* f){return d;}""",
    "THETAZ_GetPrivateProfileStringA": """DWORD WINAPI_CALL THETAZ_GetPrivateProfileStringA(const char* s,const char* k,const char* d,char* b,DWORD l,const char* f){
    if(b&&l>0&&d){strncpy(b,d,l);b[l-1]=0;}return d?(DWORD)strlen(d):0;
}""",
    "THETAZ_GetPrivateProfileStringW": """DWORD WINAPI_CALL THETAZ_GetPrivateProfileStringW(const wchar_t* s,const wchar_t* k,const wchar_t* d,wchar_t* b,DWORD l,const wchar_t* f){
    if(b&&l>0&&d){wcsncpy(b,d,l);b[l-1]=0;}return d?(DWORD)wcslen(d):0;
}""",
    "THETAZ_GetProcessId": """DWORD WINAPI_CALL THETAZ_GetProcessId(HANDLE p){return (DWORD)(uintptr_t)p;}""",
    "THETAZ_GetProcessMitigationPolicy": """BOOL WINAPI_CALL THETAZ_GetProcessMitigationPolicy(HANDLE p,int c,void* b,S s){memset(b,0,s);return TRUE;}""",
    "THETAZ_GetProcessWindowStation": """HANDLE WINAPI_CALL THETAZ_GetProcessWindowStation() {
    return (HANDLE)0x1;
}""",
    "THETAZ_GetScrollInfo": """int WINAPI_CALL THETAZ_GetScrollInfo(uint32_t hwnd, int bar, void* si) { return 0; }""",
    "THETAZ_GetScrollPos": """int WINAPI_CALL THETAZ_GetScrollPos(uint32_t hwnd, int bar) { return 0; }""",
    "THETAZ_GetScrollRange": """int WINAPI_CALL THETAZ_GetScrollRange(uint32_t hwnd, int bar, int* min, int* max) {
    if (min) *min = 0;
    if (max) *max = 0;
    return 1;
}""",
    "THETAZ_GetSecurityDescriptorControl": """BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorControl(void* sd,WORD* c,DWORD* r){if(c)*c=0x8004;if(r)*r=1;return TRUE;}""",
    "THETAZ_GetSecurityDescriptorGroup": """BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorGroup(void* sd,void* g,BOOL* d){return FALSE;}""",
    "THETAZ_GetSecurityDescriptorOwner": """BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorOwner(void* sd,void* o,BOOL* d){return FALSE;}""",
    "THETAZ_GetSecurityDescriptorSacl": """BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorSacl(void* sd,BOOL* p,void* a,BOOL* d){if(p)*p=FALSE;return TRUE;}""",
    "THETAZ_GetServiceDisplayNameW": """BOOL WINAPI_CALL THETAZ_GetServiceDisplayNameW(void* hSCManager, const wchar_t* lpServiceName,
    wchar_t* lpDisplayName, LPDWORD lpcchBuffer) {
    (void)hSCManager; (void)lpServiceName; (void)lpDisplayName;
    if (lpcchBuffer) *lpcchBuffer = 0;
    return 0;
}""",
    "THETAZ_GetShortPathNameW": """DWORD WINAPI_CALL THETAZ_GetShortPathNameW(const wchar_t* l,wchar_t* s,DWORD n){if(l&&s&&n>0)wcsncpy(s,l,n);return l?(DWORD)wcslen(l):0;}""",
    "THETAZ_GetSidLengthRequired": """DWORD WINAPI_CALL THETAZ_GetSidLengthRequired(UCHAR nSubAuthorityCount) {
    (void)nSubAuthorityCount;
    return 12;
}""",
    "THETAZ_GetSystemDirectoryA": """UINT WINAPI_CALL THETAZ_GetSystemDirectoryA(char* buf, UINT size) {
    const char* d="C:\\Windows\\System32"; size_t l=strlen(d);
    if(buf&&size>l) strcpy(buf,d); return (UINT)l;
}""",
    "THETAZ_GetSystemFirmwareTable": """DWORD WINAPI_CALL THETAZ_GetSystemFirmwareTable(DWORD s,DWORD i,void* b,DWORD sz){return 0;}""",
    "THETAZ_GetSystemTimes": """BOOL WINAPI_CALL THETAZ_GetSystemTimes(FILETIME* idle,FILETIME* kernel,FILETIME* user){
    if(idle){idle->dwLowDateTime=idle->dwHighDateTime=0;}if(kernel){kernel->dwLowDateTime=kernel->dwHighDateTime=0;}
    if(user){user->dwLowDateTime=user->dwHighDateTime=0;}return TRUE;
}""",
    "THETAZ_GetSystemWindowsDirectoryW": """UINT WINAPI_CALL THETAZ_GetSystemWindowsDirectoryW(wchar_t* b,UINT c){
    const wchar_t* path=L"C:\\Windows";
    if(b&&c>(UINT)wcslen(path)){wcscpy(b,path);return (UINT)wcslen(path);}
    return (UINT)wcslen(path);
}""",
    "THETAZ_GetTempFileNameA": """DWORD WINAPI_CALL THETAZ_GetTempFileNameA(const char* pa,const char* px,UINT u,char* b,UINT l){
    if(b&&l>0)snprintf(b,l,"%s\\%s%04x.tmp",pa?".":"",px?px:"tmp",u?u:(unsigned)getpid()&0xFFFF);return 0;
}""",
    "THETAZ_GetTextExtentPointA": """int WINAPI_CALL THETAZ_GetTextExtentPointA(uint32_t hdc, const char* str, int len, void* size) {
    return THETAZ_GetTextExtentPoint32A(hdc, str, len, size);
}""",
    "THETAZ_GetTextExtentPointW": """int WINAPI_CALL THETAZ_GetTextExtentPointW(uint32_t hdc, const uint16_t* str, int len, void* size) {
    return THETAZ_GetTextExtentPoint32W(hdc, str, len, size);
}""",
    "THETAZ_GetTextMetricsW": """int WINAPI_CALL THETAZ_GetTextMetricsW(uint32_t hdc, void* tm) {
    if (tm) memset(tm, 0, 60);
    return 1;
}""",
    "THETAZ_GetThreadContext": """BOOL WINAPI_CALL THETAZ_GetThreadContext(HANDLE t,void* c){if(c)memset(c,0,716);return TRUE;}""",
    "THETAZ_GetThreadGroupAffinity": """BOOL WINAPI_CALL THETAZ_GetThreadGroupAffinity(HANDLE t,void* a){if(a)memset(a,0,10);return TRUE;}""",
    "THETAZ_GetThreadId": """DWORD WINAPI_CALL THETAZ_GetThreadId(HANDLE hThread) {
    return hThread ? static_cast<DWORD>(reinterpret_cast<uintptr_t>(hThread) & 0xFFFFFFFF) : 0;
}""",
    "THETAZ_GetTimeFormatA": """int WINAPI_CALL THETAZ_GetTimeFormatA(LCID l,DWORD f,const SYSTEMTIME* s,const char* fmt,char* b,int bl){
    if(b&&bl>0){strcpy(b,"00:00:00");return 8;}return 8;
}""",
    "THETAZ_GetTokenInformation": """BOOL WINAPI_CALL THETAZ_GetTokenInformation(HANDLE hToken, DWORD TokenInformationClass, void* TokenInformation,
    DWORD TokenInformationLength, LPDWORD ReturnLength) {
    (void)hToken; (void)TokenInformationClass; (void)TokenInformation; (void)TokenInformationLength;
    if (ReturnLength) *ReturnLength = 4;
    return 1;
}""",
    "THETAZ_GetUserNameA": """BOOL WINAPI_CALL THETAZ_GetUserNameA(char* lpBuffer, LPDWORD pcbBuffer) {
    (void)lpBuffer; (void)pcbBuffer;
    return 0;
}""",
    "THETAZ_GetUserNameW": """BOOL WINAPI_CALL THETAZ_GetUserNameW(wchar_t* lpBuffer, LPDWORD pcbBuffer) {
    (void)lpBuffer; (void)pcbBuffer;
    return 0;
}""",
    "THETAZ_GetUserObjectInformationW": """BOOL WINAPI_CALL THETAZ_GetUserObjectInformationW(HANDLE hObj, int nIndex, void* pvInfo, DWORD nLength, LPDWORD lpnLengthNeeded) {
    (void)hObj; (void)nIndex; (void)pvInfo; (void)nLength;
    if (lpnLengthNeeded) *lpnLengthNeeded = 0;
    return 0;
}""",
    "THETAZ_GetVersion": """DWORD WINAPI_CALL THETAZ_GetVersion() { return 0x80000006; }""",
    "THETAZ_GetVersionExA": """BOOL WINAPI_CALL THETAZ_GetVersionExA(void* info) {
    if (!info) return FALSE;
    uint32_t* p = (uint32_t*)info;
    p[0]=148; p[1]=6; p[2]=1; p[3]=7601; p[4]=2;
    memset(&p[5],0,128);
    return TRUE;
}""",
    "THETAZ_GetWindowRect": """int WINAPI_CALL THETAZ_GetWindowRect(uint32_t hwnd, void* rect) {
    if (rect) memset(rect, 0, 16);
    return 1;
}""",
    "THETAZ_GetWindowRgn": """int WINAPI_CALL THETAZ_GetWindowRgn(uint32_t hwnd, uint32_t hrgn) { return 0; }""",
    "THETAZ_GetWindowTextA": """int WINAPI_CALL THETAZ_GetWindowTextA(WinHandle hwnd, char* buf, int max) {
    if (!buf || max <= 0) return 0;
    uint64_t r = THETAZ_SendMessageA(hwnd, WM_GETTEXT, static_cast<uint64_t>(max),
                                     reinterpret_cast<uint64_t>(buf));
    return static_cast<int>(r);
}""",
    "THETAZ_GetWindowTextA_Wrapper": """int WINAPI_CALL THETAZ_GetWindowTextA_Wrapper(WinHandle hwnd, char* buf, int max) {
    if (x86_dispatch_active()) return THETAZ_GetWindowTextA(hwnd, buf, max);
    return WINAPI64_GetWindowTextA(hwnd, buf, max);
}""",
    "THETAZ_GetWindowTextLengthA": """int WINAPI_CALL THETAZ_GetWindowTextLengthA(WinHandle hwnd) {
    return static_cast<int>(THETAZ_SendMessageA(hwnd, WM_GETTEXTLENGTH, 0, 0));
}""",
    "THETAZ_GetWindowTextLengthA_Wrapper": """int WINAPI_CALL THETAZ_GetWindowTextLengthA_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_GetWindowTextLengthA(hwnd);
    return WINAPI64_GetWindowTextLengthA(hwnd);
}""",
    "THETAZ_GetWindowTextLengthW": """int WINAPI_CALL THETAZ_GetWindowTextLengthW(uint32_t hwnd) { return 0; }""",
    "THETAZ_GetWindowTextW": """int WINAPI_CALL THETAZ_GetWindowTextW(uint32_t hwnd, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}""",
    "THETAZ_GetWindowsDirectoryA": """UINT WINAPI_CALL THETAZ_GetWindowsDirectoryA(char* buf, UINT size) {
    const char* d="C:\\Windows"; size_t l=strlen(d);
    if(buf&&size>l) strcpy(buf,d); return (UINT)l;
}""",
    "THETAZ_GetWindowsDirectoryW": """UINT WINAPI_CALL THETAZ_GetWindowsDirectoryW(uint16_t* buf, UINT size) {
    const wchar_t* d=L"C:\\Windows"; size_t l=9;
    if(buf&&size>l){for(size_t i=0;i<=l;i++)buf[i]=d[i];} return (UINT)l;
}""",
    "THETAZ_GlobalDeleteAtom": """int WINAPI_CALL THETAZ_GlobalDeleteAtom(uint16_t atom) {
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    for (auto it = g_atom_table.begin(); it != g_atom_table.end(); ++it) {
        if (it->second == atom) { g_atom_table.erase(it); break; }
    }
    return 0;
}""",
    "THETAZ_GlobalFindAtomA": """ATOM WINAPI_CALL THETAZ_GlobalFindAtomA(const char* n){return (ATOM)(uintptr_t)(n+0x20000);}""",
    "THETAZ_GlobalFindAtomW": """ATOM WINAPI_CALL THETAZ_GlobalFindAtomW(const wchar_t* n){return (ATOM)(uintptr_t)((const char*)n+0x20000);}""",
    "THETAZ_GlobalFlags": """DWORD WINAPI_CALL THETAZ_GlobalFlags(HGLOBAL h){return 0;}""",
    "THETAZ_GlobalGetAtomNameW": """UINT WINAPI_CALL THETAZ_GlobalGetAtomNameW(ATOM a,wchar_t* b,int s){if(b&&s>0)b[0]=0;return 0;}""",
    "THETAZ_GlobalUnlock": """BOOL WINAPI_CALL THETAZ_GlobalUnlock(HGLOBAL h){return TRUE;}""",
    "THETAZ_HashData": """HRESULT WINAPI_CALL THETAZ_HashData(const BYTE* d,DWORD dl,BYTE* h,DWORD hl,DWORD a,DWORD s){if(h&&hl>0)memset(h,0,hl);return S_OK;}""",
    "THETAZ_HeapQueryInformation": """BOOL WINAPI_CALL THETAZ_HeapQueryInformation(HANDLE h,int c,void* b,S l,S* r){if(b&&l>=8)memset(b,0,l);if(r)*r=0;return TRUE;}""",
    "THETAZ_IdnToAscii": """int WINAPI_CALL THETAZ_IdnToAscii(DWORD f,const wchar_t* in,int il,wchar_t* out,int ol){
    if(out&&in&&il>=0&&ol>=il){memcpy(out,in,il*2);return il;}return il;
}""",
    "THETAZ_IdnToUnicode": """int WINAPI_CALL THETAZ_IdnToUnicode(DWORD f,const wchar_t* in,int il,wchar_t* out,int ol){
    if(out&&in&&il>=0&&ol>=il){memcpy(out,in,il*2);return il;}return il;
}""",
    "THETAZ_ImpersonateNamedPipeClient": """BOOL WINAPI_CALL THETAZ_ImpersonateNamedPipeClient(HANDLE hNamedPipe) {
    (void)hNamedPipe;
    return 0;
}""",
    "THETAZ_InitializeAcl": """BOOL WINAPI_CALL THETAZ_InitializeAcl(void* pAcl, DWORD nAclLength, DWORD dwAclRevision) {
    (void)pAcl; (void)nAclLength; (void)dwAclRevision;
    return 1;
}""",
    "THETAZ_InitializeProcThreadAttributeList": """BOOL WINAPI_CALL THETAZ_InitializeProcThreadAttributeList(void* l,DWORD c,DWORD f,S* s){if(s)*s=c*16+8;return TRUE;}""",
    "THETAZ_InitializeSecurityDescriptor": """BOOL WINAPI_CALL THETAZ_InitializeSecurityDescriptor(void* sd,DWORD r){if(sd)memset(sd,0,64);return TRUE;}""",
    "THETAZ_InterlockedFlushSList": """LONG WINAPI_CALL THETAZ_InterlockedFlushSList(void* l){return 0;}""",
    "THETAZ_IntersectClipRect": """int WINAPI_CALL THETAZ_IntersectClipRect(uint32_t hdc, int l, int t, int r, int b) {
    return 1;
}""",
    "THETAZ_IntersectRect": """int WINAPI_CALL THETAZ_IntersectRect(void* dst, const void* r1, const void* r2) {
    if (dst) memset(dst, 0, 16);
    return 0;
}""",
    "THETAZ_InvalidateRect": """BOOL WINAPI_CALL THETAZ_InvalidateRect(WinHandle hwnd, const void* rect, BOOL erase) {
    (void)erase;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    if (!win) return FALSE_VALUE;
    win->dirty = true;
    if (rect) {
        const int* r = static_cast<const int*>(rect);
        win->inv_left = r[0];
        win->inv_top = r[1];
        win->inv_right = r[2];
        win->inv_bottom = r[3];
    } else {
        win->inv_left = 0;
        win->inv_top = 0;
        win->inv_right = win->w;
        win->inv_bottom = win->h;
    }
    return TRUE_VALUE;
}""",
    "THETAZ_InvalidateRect_Wrapper": """BOOL WINAPI_CALL THETAZ_InvalidateRect_Wrapper(WinHandle hwnd, const void* rect, BOOL erase) {
    if (x86_dispatch_active()) {
        return THETAZ_InvalidateRect(hwnd, rect, erase);
    } else {
        return WINAPI64_InvalidateRect(hwnd, rect, erase);
    }
}""",
    "THETAZ_InvalidateRgn": """int WINAPI_CALL THETAZ_InvalidateRgn(uint32_t hwnd, uint32_t hrgn, int erase) { return 1; }""",
    "THETAZ_IsBadCodePtr": """BOOL WINAPI_CALL THETAZ_IsBadCodePtr(const void* ptr) {
    return ptr ? 0 : 1;
}""",
    "THETAZ_IsBadStringPtrA": """BOOL WINAPI_CALL THETAZ_IsBadStringPtrA(const char* ptr, uint32_t len) {
    return ptr ? 0 : 1;
}""",
    "THETAZ_IsBadWritePtr": """BOOL WINAPI_CALL THETAZ_IsBadWritePtr(void* ptr, uint32_t len) {
    return ptr ? 0 : 1;
}""",
    "THETAZ_IsCharBlankW": """BOOL WINAPI_CALL THETAZ_IsCharBlankW(wchar_t c){return c==L' '||c==L'\t';}""",
    "THETAZ_IsCharCntrlW": """BOOL WINAPI_CALL THETAZ_IsCharCntrlW(wchar_t c){return c<32||c==127;}""",
    "THETAZ_IsCharDigitW": """BOOL WINAPI_CALL THETAZ_IsCharDigitW(wchar_t c){return c>=L'0'&&c<=L'9';}""",
    "THETAZ_IsCharPunctW": """BOOL WINAPI_CALL THETAZ_IsCharPunctW(wchar_t c){return ispunct(c);}""",
    "THETAZ_IsCharSpaceA": """BOOL WINAPI_CALL THETAZ_IsCharSpaceA(char c){return c==' '||(c>='\t'&&c<='\r');}""",
    "THETAZ_IsCharSpaceW": """BOOL WINAPI_CALL THETAZ_IsCharSpaceW(wchar_t c){return c==L' '||(c>=L'\t'&&c<=L'\r');}""",
    "THETAZ_IsCharXDigitW": """BOOL WINAPI_CALL THETAZ_IsCharXDigitW(wchar_t c){return isxdigit(c);}""",
    "THETAZ_IsChild": """int WINAPI_CALL THETAZ_IsChild(uint32_t parent, uint32_t child) { return 0; }""",
    "THETAZ_IsDBCSLeadByteEx": """BOOL WINAPI_CALL THETAZ_IsDBCSLeadByteEx(UINT cp,BYTE l){return FALSE;}""",
    "THETAZ_IsDialogMessageW": """int WINAPI_CALL THETAZ_IsDialogMessageW(uint32_t hwnd, void* msg) { return 0; }""",
    "THETAZ_IsInternetESCEnabled": """BOOL WINAPI_CALL THETAZ_IsInternetESCEnabled(){return FALSE;}""",
    "THETAZ_IsValidAcl": """BOOL WINAPI_CALL THETAZ_IsValidAcl(void* a){return TRUE;}""",
    "THETAZ_IsValidSid": """BOOL WINAPI_CALL THETAZ_IsValidSid(void* pSid) {
    (void)pSid;
    return 1;
}""",
    "THETAZ_IsWindow": """BOOL WINAPI_CALL THETAZ_IsWindow(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    return find_window(reinterpret_cast<uint64_t>(hwnd)) ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_IsWindowEnabled": """BOOL WINAPI_CALL THETAZ_IsWindowEnabled(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return (win && win->enabled) ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_IsWindowEnabled_Wrapper": """BOOL WINAPI_CALL THETAZ_IsWindowEnabled_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindowEnabled(hwnd);
    return WINAPI64_IsWindowEnabled(hwnd);
}""",
    "THETAZ_IsWindowUnicode": """int WINAPI_CALL THETAZ_IsWindowUnicode(uint32_t hwnd) { return 1; }""",
    "THETAZ_IsWindowVisible": """BOOL WINAPI_CALL THETAZ_IsWindowVisible(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return (win && win->visible) ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_IsWindowVisible_Wrapper": """BOOL WINAPI_CALL THETAZ_IsWindowVisible_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindowVisible(hwnd);
    return WINAPI64_IsWindowVisible(hwnd);
}""",
    "THETAZ_IsWindow_Wrapper": """BOOL WINAPI_CALL THETAZ_IsWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindow(hwnd);
    return WINAPI64_IsWindow(hwnd);
}""",
    "THETAZ_KillTimer": """BOOL WINAPI_CALL THETAZ_KillTimer(WinHandle hwnd, uintptr_t nIDEvent) {
    uint64_t h = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    size_t erased = g_timers.erase((h << 32) | static_cast<uint32_t>(nIDEvent));
    g_timer_cv.notify_all();
    return erased ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_KillTimer_Wrapper": """BOOL WINAPI_CALL THETAZ_KillTimer_Wrapper(WinHandle hwnd, uintptr_t nIDEvent) {
    if (x86_dispatch_active()) {
        return THETAZ_KillTimer(hwnd, nIDEvent);
    } else {
        return WINAPI64_KillTimer(hwnd, nIDEvent);
    }
}""",
    "THETAZ_LZClose": """void WINAPI_CALL THETAZ_LZClose(HFILE h){}""",
    "THETAZ_LZCopy": """LONG WINAPI_CALL THETAZ_LZCopy(HFILE s,HFILE d){return 0;}""",
    "THETAZ_LZSeek": """LONG WINAPI_CALL THETAZ_LZSeek(HFILE h,LONG o,int origin){return 0;}""",
    "THETAZ_LineTo": """int WINAPI_CALL THETAZ_LineTo(uint32_t hdc, int x, int y) {
    return 1;
}""",
    "THETAZ_LoadStringA": """int WINAPI_CALL THETAZ_LoadStringA(HINSTANCE h,UINT id,char* b,int c){if(b&&c>0){b[0]=0;return 0;}return 0;}""",
    "THETAZ_LoadStringW": """int WINAPI_CALL THETAZ_LoadStringW(HINSTANCE h,UINT id,wchar_t* b,int c){if(b&&c>0){b[0]=0;return 0;}return 0;}""",
    "THETAZ_LocalFileTimeToFileTime": """BOOL WINAPI_CALL THETAZ_LocalFileTimeToFileTime(const FILETIME* l,FILETIME* f){if(f&&l)*f=*l;return TRUE;}""",
    "THETAZ_LookupAccountNameW": """BOOL WINAPI_CALL THETAZ_LookupAccountNameW(const wchar_t* lpSystemName, const wchar_t* lpAccountName,
    void* Sid, LPDWORD cbSid, wchar_t* ReferencedDomainName, LPDWORD cchReferencedDomainName, DWORD* peUse) {
    (void)lpSystemName; (void)lpAccountName; (void)Sid; (void)cbSid;
    (void)ReferencedDomainName; (void)cchReferencedDomainName; (void)peUse;
    return 0;
}""",
    "THETAZ_LookupAccountSidW": """BOOL WINAPI_CALL THETAZ_LookupAccountSidW(const wchar_t* lpSystemName, void* Sid,
    wchar_t* Name, LPDWORD cchName, wchar_t* ReferencedDomainName, LPDWORD cchReferencedDomainName, DWORD* peUse) {
    (void)lpSystemName; (void)Sid;
    (void)Name; (void)cchName; (void)ReferencedDomainName; (void)cchReferencedDomainName; (void)peUse;
    return 0;
}""",
    "THETAZ_LookupPrivilegeValueW": """BOOL WINAPI_CALL THETAZ_LookupPrivilegeValueW(const wchar_t* lpSystemName, const wchar_t* lpName, uint64_t* pLuid) {
    (void)lpSystemName; (void)lpName;
    if (pLuid) *pLuid = 1;
    return 1;
}""",
    "THETAZ_MapDialogRect": """int WINAPI_CALL THETAZ_MapDialogRect(uint32_t hwnd, void* rect) { return 1; }""",
    "THETAZ_MapWindowPoints": """int WINAPI_CALL THETAZ_MapWindowPoints(uint32_t from, uint32_t to, void* pts, int count) { return 1; }""",
    "THETAZ_MessageBoxA": """int WINAPI_CALL THETAZ_MessageBoxA(WinHandle hwnd, const char* text,
                                          const char* caption, uint32_t type) {
    (void)hwnd;
    (void)type;
    fprintf(stderr, "[user32] MessageBoxA: \"%s\" title=\"%s\"\n",
            text ? text : "", caption ? caption : "");
    return 1; // IDOK
}""",
    "THETAZ_MessageBoxA_Wrapper": """int WINAPI_CALL THETAZ_MessageBoxA_Wrapper(WinHandle hwnd, const char* text,
                                                  const char* caption, uint32_t type) {
    if (x86_dispatch_active()) return THETAZ_MessageBoxA(hwnd, text, caption, type);
    return WINAPI64_MessageBoxA(hwnd, text, caption, type);
}""",
    "THETAZ_MoveFileW": """BOOL WINAPI_CALL THETAZ_MoveFileW(const wchar_t* f,const wchar_t* t){
    char fa[260],ta[260];
    wcstombs(fa,f,sizeof(fa));wcstombs(ta,t,sizeof(ta));
    return rename(wine_to_host(fa).c_str(),wine_to_host(ta).c_str())==0;
}""",
    "THETAZ_MoveToEx": """int WINAPI_CALL THETAZ_MoveToEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}""",
    "THETAZ_MoveWindow": """int WINAPI_CALL THETAZ_MoveWindow(uint32_t hwnd, int x, int y, int w, int h, int repaint) { return 1; }""",
    "THETAZ_MsgWaitForMultipleObjects": """DWORD WINAPI_CALL THETAZ_MsgWaitForMultipleObjects(DWORD nCount,const HANDLE* pHandles,BOOL fWaitAll,DWORD dwMilliseconds,DWORD dwWakeMask){
    (void)nCount;(void)pHandles;(void)fWaitAll;(void)dwMilliseconds;(void)dwWakeMask;
    return 0;
}""",
    "THETAZ_NormalizeString": """int WINAPI_CALL THETAZ_NormalizeString(int f,const wchar_t* s,int sl,wchar_t* d,int dl){
    if(d&&s&&dl>=sl){memcpy(d,s,sl*2);return sl;}return sl;
}""",
    "THETAZ_NotifyWinEvent": """void WINAPI_CALL THETAZ_NotifyWinEvent(uint32_t event, uint32_t hwnd, int idObject, int idChild) {}""",
    "THETAZ_OffsetWindowOrgEx": """int WINAPI_CALL THETAZ_OffsetWindowOrgEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}""",
    "THETAZ_OleIsCurrentClipboard": """HRESULT WINAPI_CALL THETAZ_OleIsCurrentClipboard(void* pDataObject) {
    (void)pDataObject;
    return 1; // S_FALSE
}""",
    "THETAZ_OpenFileMappingW": """HANDLE WINAPI_CALL THETAZ_OpenFileMappingW(DWORD a,BOOL i,const wchar_t* n){return NULL;}""",
    "THETAZ_OpenProcessToken": """BOOL WINAPI_CALL THETAZ_OpenProcessToken(HANDLE hProcess, DWORD dwDesiredAccess, HANDLE* phToken) {
    (void)hProcess; (void)dwDesiredAccess;
    if (phToken) *phToken = (HANDLE)0x4000;
    return 1;
}""",
    "THETAZ_OpenSemaphoreW": """HANDLE WINAPI_CALL THETAZ_OpenSemaphoreW(DWORD a,BOOL i,const wchar_t* n){return (HANDLE)(uintptr_t)0x20001;}""",
    "THETAZ_OpenThreadToken": """BOOL WINAPI_CALL THETAZ_OpenThreadToken(HANDLE hThread, DWORD dwDesiredAccess, BOOL bOpenAsSelf, HANDLE* phToken) {
    (void)hThread; (void)dwDesiredAccess; (void)bOpenAsSelf; (void)phToken;
    return 0;
}""",
    "THETAZ_ParseURLA": """HRESULT WINAPI_CALL THETAZ_ParseURLA(const char* u,void* p){return E_NOTIMPL;}""",
    "THETAZ_ParseURLW": """HRESULT WINAPI_CALL THETAZ_ParseURLW(const wchar_t* u,void* p){return E_NOTIMPL;}""",
    "THETAZ_PatBlt": """int WINAPI_CALL THETAZ_PatBlt(uint32_t hdc, int x, int y, int w, int h, uint32_t rop) {
    return 1;
}""",
    "THETAZ_PathAddExtensionA": """BOOL WINAPI_CALL THETAZ_PathAddExtensionA(char* p,const char* e){if(!p||!e)return FALSE;char* d=strrchr(p,'.');if(d&&d>strrchr(p,'\\'))return FALSE;strcat(p,e);return TRUE;}""",
    "THETAZ_PathAddExtensionW": """BOOL WINAPI_CALL THETAZ_PathAddExtensionW(wchar_t* p,const wchar_t* e){if(!p||!e)return FALSE;wchar_t* d=wcsrchr(p,L'.');if(d&&d>wcsrchr(p,L'\\'))return FALSE;wcscat(p,e);return TRUE;}""",
    "THETAZ_PathAppendA": """BOOL WINAPI_CALL THETAZ_PathAppendA(char* b,const char* c){if(!b||!c)return FALSE;int l=(int)strlen(b);if(l>0&&b[l-1]!='\\'&&b[l-1]!='/')strcat(b,"\\");strcat(b,c);return TRUE;}""",
    "THETAZ_PathAppendW": """BOOL WINAPI_CALL THETAZ_PathAppendW(wchar_t* b,const wchar_t* c){if(!b||!c)return FALSE;int l=(int)wcslen(b);if(l>0&&b[l-1]!=L'\\'&&b[l-1]!=L'/')wcscat(b,L"\\");wcscat(b,c);return TRUE;}""",
    "THETAZ_PathCanonicalizeA": """BOOL WINAPI_CALL THETAZ_PathCanonicalizeA(char* b,const char* p){if(b&&p)strcpy(b,p);return TRUE;}""",
    "THETAZ_PathCanonicalizeW": """BOOL WINAPI_CALL THETAZ_PathCanonicalizeW(wchar_t* b,const wchar_t* p){if(b&&p)wcscpy(b,p);return TRUE;}""",
    "THETAZ_PathCchAppend": """BOOL WINAPI_CALL THETAZ_PathCchAppend(wchar_t* p,size_t m,const wchar_t* e){return THETAZ_PathAppendW(p,e);}""",
    "THETAZ_PathCchFindExtension": """HRESULT WINAPI_CALL THETAZ_PathCchFindExtension(const wchar_t* p,size_t m,const wchar_t** e){if(!p||!e)return E_INVALIDARG;const wchar_t* d=wcsrchr(p,L'.');*e=d?d:p+wcslen(p);return S_OK;}""",
    "THETAZ_PathCommonPrefixA": """BOOL WINAPI_CALL THETAZ_PathCommonPrefixA(const char* a,const char* b,char* buf){if(buf)buf[0]=0;return FALSE;}""",
    "THETAZ_PathCommonPrefixW": """BOOL WINAPI_CALL THETAZ_PathCommonPrefixW(const wchar_t* a,const wchar_t* b,wchar_t* buf){if(buf)buf[0]=0;return FALSE;}""",
    "THETAZ_PathCreateFromUrlA": """HRESULT WINAPI_CALL THETAZ_PathCreateFromUrlA(const char* u,char* p,uint32_t* l,DWORD f){if(u&&p&&l){strncpy(p,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_PathCreateFromUrlAlloc": """HRESULT WINAPI_CALL THETAZ_PathCreateFromUrlAlloc(const wchar_t* u,wchar_t** p,DWORD f){if(!u||!p)return E_FAIL;*p=(wchar_t*)malloc((wcslen(u)+1)*2);wcscpy(*p,u);return S_OK;}""",
    "THETAZ_PathCreateFromUrlW": """HRESULT WINAPI_CALL THETAZ_PathCreateFromUrlW(const wchar_t* u,wchar_t* p,uint32_t* l,DWORD f){if(u&&p&&l){wcsncpy(p,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_PathFileExistsA": """BOOL WINAPI_CALL THETAZ_PathFileExistsA(const char* p){
    if(!p)return FALSE;struct stat st;return stat(wine_to_host(p).c_str(),&st)==0;
}""",
    "THETAZ_PathFileExistsW": """BOOL WINAPI_CALL THETAZ_PathFileExistsW(const wchar_t* p){
    if(!p)return FALSE;char a[260];wcstombs(a,p,sizeof(a));struct stat st;return stat(wine_to_host(a).c_str(),&st)==0;
}""",
    "THETAZ_PathGetCharTypeA": """DWORD WINAPI_CALL THETAZ_PathGetCharTypeA(char c){if(c=='\\'||c=='/')return 4;if(c=='.')return 8;return 1;}""",
    "THETAZ_PathGetCharTypeW": """DWORD WINAPI_CALL THETAZ_PathGetCharTypeW(wchar_t c){if(c==L'\\'||c==L'/')return 4;if(c==L'.')return 8;return 1;}""",
    "THETAZ_PathGetDriveNumberA": """int WINAPI_CALL THETAZ_PathGetDriveNumberA(const char* p){if(!p||p[1]!=':')return -1;return(p[0]>='A'&&p[0]<='Z')?p[0]-'A':(p[0]>='a'&&p[0]<='z')?p[0]-'a':-1;}""",
    "THETAZ_PathGetDriveNumberW": """int WINAPI_CALL THETAZ_PathGetDriveNumberW(const wchar_t* p){if(!p||p[1]!=L':')return -1;return(p[0]>=L'A'&&p[0]<=L'Z')?p[0]-L'A':(p[0]>=L'a'&&p[0]<=L'z')?p[0]-L'a':-1;}""",
    "THETAZ_PathIsFileSpecA": """BOOL WINAPI_CALL THETAZ_PathIsFileSpecA(const char* p){return p&&!strchr(p,'\\')&&!strchr(p,'/');}""",
    "THETAZ_PathIsFileSpecW": """BOOL WINAPI_CALL THETAZ_PathIsFileSpecW(const wchar_t* p){return p&&!wcschr(p,L'\\')&&!wcschr(p,L'/');}""",
    "THETAZ_PathIsLFNFileSpecA": """BOOL WINAPI_CALL THETAZ_PathIsLFNFileSpecA(const char* p){return FALSE;}""",
    "THETAZ_PathIsLFNFileSpecW": """BOOL WINAPI_CALL THETAZ_PathIsLFNFileSpecW(const wchar_t* p){return FALSE;}""",
    "THETAZ_PathIsPrefixA": """BOOL WINAPI_CALL THETAZ_PathIsPrefixA(const char* pr,const char* p){return p&&pr&&strncasecmp(p,pr,strlen(pr))==0;}""",
    "THETAZ_PathIsPrefixW": """BOOL WINAPI_CALL THETAZ_PathIsPrefixW(const wchar_t* pr,const wchar_t* p){return p&&pr&&wcsncasecmp(p,pr,(int)wcslen(pr))==0;}""",
    "THETAZ_PathIsRelativeA": """BOOL WINAPI_CALL THETAZ_PathIsRelativeA(const char* p){if(!p||!*p)return TRUE;if(p[1]==':')return FALSE;if(p[0]=='\\'&&p[1]=='\\')return FALSE;return TRUE;}""",
    "THETAZ_PathIsRelativeW": """BOOL WINAPI_CALL THETAZ_PathIsRelativeW(const wchar_t* p){if(!p||!*p)return TRUE;if(p[1]==L':')return FALSE;if(p[0]==L'\\'&&p[1]==L'\\')return FALSE;return TRUE;}""",
    "THETAZ_PathIsRootA": """BOOL WINAPI_CALL THETAZ_PathIsRootA(const char* p){if(!p)return FALSE;if(p[0]=='\\'&&p[1]==0)return TRUE;if(p[1]==':'&&p[2]=='\\'&&p[3]==0)return TRUE;return FALSE;}""",
    "THETAZ_PathIsRootW": """BOOL WINAPI_CALL THETAZ_PathIsRootW(const wchar_t* p){if(!p)return FALSE;if(p[0]==L'\\'&&p[1]==0)return TRUE;if(p[1]==L':'&&p[2]==L'\\'&&p[3]==0)return TRUE;return FALSE;}""",
    "THETAZ_PathIsSameRootA": """BOOL WINAPI_CALL THETAZ_PathIsSameRootA(const char* a,const char* b){return TRUE;}""",
    "THETAZ_PathIsSameRootW": """BOOL WINAPI_CALL THETAZ_PathIsSameRootW(const wchar_t* a,const wchar_t* b){return TRUE;}""",
    "THETAZ_PathIsUNCA": """BOOL WINAPI_CALL THETAZ_PathIsUNCA(const char* p){return p&&p[0]=='\\'&&p[1]=='\\';}""",
    "THETAZ_PathIsUNCServerA": """BOOL WINAPI_CALL THETAZ_PathIsUNCServerA(const char* p){return FALSE;}""",
    "THETAZ_PathIsUNCServerShareA": """BOOL WINAPI_CALL THETAZ_PathIsUNCServerShareA(const char* p){return FALSE;}""",
    "THETAZ_PathIsUNCServerShareW": """BOOL WINAPI_CALL THETAZ_PathIsUNCServerShareW(const wchar_t* p){return FALSE;}""",
    "THETAZ_PathIsUNCServerW": """BOOL WINAPI_CALL THETAZ_PathIsUNCServerW(const wchar_t* p){return FALSE;}""",
    "THETAZ_PathIsUNCW": """BOOL WINAPI_CALL THETAZ_PathIsUNCW(const wchar_t* p){return p&&p[0]==L'\\'&&p[1]==L'\\';}""",
    "THETAZ_PathIsURLA": """BOOL WINAPI_CALL THETAZ_PathIsURLA(const char* p){return FALSE;}""",
    "THETAZ_PathIsURLW": """BOOL WINAPI_CALL THETAZ_PathIsURLW(const wchar_t* p){return FALSE;}""",
    "THETAZ_PathIsValidCharA": """BOOL WINAPI_CALL THETAZ_PathIsValidCharA(char c,DWORD f){return TRUE;}""",
    "THETAZ_PathIsValidCharW": """BOOL WINAPI_CALL THETAZ_PathIsValidCharW(wchar_t c,DWORD f){return TRUE;}""",
    "THETAZ_PathMatchSpecA": """BOOL WINAPI_CALL THETAZ_PathMatchSpecA(const char* p,const char* sp){
    if(!p||!sp)return FALSE;
    if(strcmp(sp,"*.*")==0)return TRUE;
    return fnmatch(sp,p,FNM_CASEFOLD)==0;
}""",
    "THETAZ_PathMatchSpecExA": """HRESULT WINAPI_CALL THETAZ_PathMatchSpecExA(const char* p,const char* sp,DWORD f,void* o){return THETAZ_PathMatchSpecA(p,sp)?S_OK:S_FALSE;}""",
    "THETAZ_PathMatchSpecExW": """HRESULT WINAPI_CALL THETAZ_PathMatchSpecExW(const wchar_t* p,const wchar_t* sp,DWORD f,void* o){return THETAZ_PathMatchSpecW(p,sp)?S_OK:S_FALSE;}""",
    "THETAZ_PathMatchSpecW": """BOOL WINAPI_CALL THETAZ_PathMatchSpecW(const wchar_t* p,const wchar_t* sp){
    if(!p||!sp)return FALSE;size_t pl=wcslen(p),sl=wcslen(sp);
    if(wcscmp(sp,L"*.*")==0)return TRUE;
    if(sp[0]==L'*'&&sp[sl-1]==L'*'){return wcsncasecmp(p,sp+1,sl-2)==0;}
    if(sp[0]==L'*'){return wcscasecmp(p+pl-(sl-1),sp+1)==0||wcscasecmp(p,sp+1)==0;}
    return wcscasecmp(p,sp)==0;
}""",
    "THETAZ_PathParseIconLocationA": """int WINAPI_CALL THETAZ_PathParseIconLocationA(char* p){return 0;}""",
    "THETAZ_PathParseIconLocationW": """int WINAPI_CALL THETAZ_PathParseIconLocationW(wchar_t* p){return 0;}""",
    "THETAZ_PathQuoteSpacesA": """void WINAPI_CALL THETAZ_PathQuoteSpacesA(char* p){}""",
    "THETAZ_PathQuoteSpacesW": """void WINAPI_CALL THETAZ_PathQuoteSpacesW(wchar_t* p){}""",
    "THETAZ_PathRelativePathToA": """BOOL WINAPI_CALL THETAZ_PathRelativePathToA(char* f,const char* fd,DWORD fa,const char* t,DWORD ta){if(f&&t)strcpy(f,t);return TRUE;}""",
    "THETAZ_PathRelativePathToW": """BOOL WINAPI_CALL THETAZ_PathRelativePathToW(wchar_t* f,const wchar_t* fd,DWORD fa,const wchar_t* t,DWORD ta){if(f&&t)wcscpy(f,t);return TRUE;}""",
    "THETAZ_PathRemoveBlanksA": """void WINAPI_CALL THETAZ_PathRemoveBlanksA(char* p){int l=(int)strlen(p);while(l>0&&(p[l-1]==' '||p[l-1]=='\t'))p[--l]=0;}""",
    "THETAZ_PathRemoveBlanksW": """void WINAPI_CALL THETAZ_PathRemoveBlanksW(wchar_t* p){int l=(int)wcslen(p);while(l>0&&(p[l-1]==L' '||p[l-1]==L'\t'))p[--l]=0;}""",
    "THETAZ_PathRemoveExtensionA": """void WINAPI_CALL THETAZ_PathRemoveExtensionA(char* p){if(!p)return;char* e=THETAZ_PathFindExtensionA(p);if(e&&*e=='.')*e=0;}""",
    "THETAZ_PathRemoveExtensionW": """void WINAPI_CALL THETAZ_PathRemoveExtensionW(wchar_t* p){if(!p)return;wchar_t* e=THETAZ_PathFindExtensionW(p);if(e&&*e==L'.')*e=0;}""",
    "THETAZ_PathRemoveFileSpecA": """BOOL WINAPI_CALL THETAZ_PathRemoveFileSpecA(char* p){if(!p)return FALSE;char* s=strrchr(p,'\\');if(!s)s=strrchr(p,'/');if(s&&s>p){*s=0;return TRUE;}return FALSE;}""",
    "THETAZ_PathRemoveFileSpecW": """BOOL WINAPI_CALL THETAZ_PathRemoveFileSpecW(wchar_t* p){if(!p)return FALSE;wchar_t* s=wcsrchr(p,L'\\');if(!s)s=wcsrchr(p,L'/');if(s&&s>p){*s=0;return TRUE;}return FALSE;}""",
    "THETAZ_PathRenameExtensionA": """BOOL WINAPI_CALL THETAZ_PathRenameExtensionA(char* p,const char* e){char* o=THETAZ_PathFindExtensionA(p);if(o)strcpy(o,e);return TRUE;}""",
    "THETAZ_PathRenameExtensionW": """BOOL WINAPI_CALL THETAZ_PathRenameExtensionW(wchar_t* p,const wchar_t* e){wchar_t* o=THETAZ_PathFindExtensionW(p);if(o)wcscpy(o,e);return TRUE;}""",
    "THETAZ_PathSearchAndQualifyA": """BOOL WINAPI_CALL THETAZ_PathSearchAndQualifyA(const char* p,char* b,uint32_t* l){if(b&&p&&l){strncpy(b,p,*l);return TRUE;}return FALSE;}""",
    "THETAZ_PathSearchAndQualifyW": """BOOL WINAPI_CALL THETAZ_PathSearchAndQualifyW(const wchar_t* p,wchar_t* b,uint32_t* l){if(b&&p&&l){wcsncpy(b,p,*l);return TRUE;}return FALSE;}""",
    "THETAZ_PathStripPathA": """void WINAPI_CALL THETAZ_PathStripPathA(char* p){char* n=THETAZ_PathFindFileNameA(p);if(n!=p)strcpy(p,n);}""",
    "THETAZ_PathStripPathW": """void WINAPI_CALL THETAZ_PathStripPathW(wchar_t* p){wchar_t* n=THETAZ_PathFindFileNameW(p);if(n!=p)wcscpy(p,n);}""",
    "THETAZ_PathStripToRootA": """BOOL WINAPI_CALL THETAZ_PathStripToRootA(char* p){if(!p)return FALSE;if(p[1]==':'){p[2]='\\';p[3]=0;return TRUE;}if(p[0]=='\\'&&p[1]=='\\'){p[2]=0;return TRUE;}return FALSE;}""",
    "THETAZ_PathStripToRootW": """BOOL WINAPI_CALL THETAZ_PathStripToRootW(wchar_t* p){if(!p)return FALSE;if(p[1]==L':'){p[2]=L'\\';p[3]=0;return TRUE;}if(p[0]==L'\\'&&p[1]==L'\\'){p[2]=0;return TRUE;}return FALSE;}""",
    "THETAZ_PathUnExpandEnvStringsA": """BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsA(const char* p,char* b,uint32_t* l){if(b&&p&&l){strncpy(b,p,*l);return TRUE;}return FALSE;}""",
    "THETAZ_PathUnExpandEnvStringsW": """BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsW(const wchar_t* p,wchar_t* b,uint32_t* l){if(b&&p&&l){wcsncpy(b,p,*l);return TRUE;}return FALSE;}""",
    "THETAZ_PathUnquoteSpacesA": """void WINAPI_CALL THETAZ_PathUnquoteSpacesA(char* p){}""",
    "THETAZ_PathUnquoteSpacesW": """void WINAPI_CALL THETAZ_PathUnquoteSpacesW(wchar_t* p){}""",
    "THETAZ_PeekMessageA": """BOOL WINAPI_CALL THETAZ_PeekMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    (void)hwnd;
    if (!msg) return FALSE_VALUE;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
        bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                              (it->message >= filter_min && it->message <= filter_max);
        if (it->message == WM_QUIT || matches_filter) {
            GuestMsg m = *it;
            if (remove) {
                g_queue.erase(it);
                if (m.message == WM_QUIT) g_quit_pending = false;
            }
            write_guest_msg(static_cast<uint8_t*>(msg), m);
            return TRUE_VALUE;
        }
    }
    return FALSE_VALUE;
}""",
    "THETAZ_PeekMessageA_Wrapper": """BOOL WINAPI_CALL THETAZ_PeekMessageA_Wrapper(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    if (x86_dispatch_active()) {
        return THETAZ_PeekMessageA(msg, hwnd, filter_min, filter_max, remove);
    } else {
        return WINAPI64_PeekMessageA(msg, hwnd, filter_min, filter_max, remove);
    }
}""",
    "THETAZ_PeekMessageW": """int WINAPI_CALL THETAZ_PeekMessageW(void* msg, uint32_t hwnd, uint32_t min, uint32_t max, uint32_t remove) {
    if (msg) memset(msg, 0, 28);
    return 0;
}""",
    "THETAZ_PolyPolyline": """int WINAPI_CALL THETAZ_PolyPolyline(uint32_t hdc, const void* pt, const uint32_t* counts, uint32_t polys) {
    return 1;
}""",
    "THETAZ_Polygon": """int WINAPI_CALL THETAZ_Polygon(uint32_t hdc, const void* pt, int count) {
    return 1;
}""",
    "THETAZ_PostMessageA": """BOOL WINAPI_CALL THETAZ_PostMessageA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    GuestMsg m;
    m.hwnd = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    m.message = message;
    m.wparam = wparam;
    m.lparam = lparam;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
    return TRUE_VALUE;
}""",
    "THETAZ_PostMessageA_Wrapper": """BOOL WINAPI_CALL THETAZ_PostMessageA_Wrapper(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    if (x86_dispatch_active()) {
        return THETAZ_PostMessageA(hwnd, message, wparam, lparam);
    } else {
        return WINAPI64_PostMessageA(hwnd, message, wparam, lparam);
    }
}""",
    "THETAZ_PostMessageW": """int WINAPI_CALL THETAZ_PostMessageW(uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) { return 1; }""",
    "THETAZ_PostQuitMessage": """void WINAPI_CALL THETAZ_PostQuitMessage(int exit_code) {
    printf("[user32] PostQuitMessage(%d)\n", exit_code);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    g_quit_pending = true;
    g_quit_exit_code = static_cast<DWORD>(exit_code);
    GuestMsg m;
    m.hwnd = 0;
    m.message = WM_QUIT;
    m.wparam = static_cast<uint64_t>(static_cast<uint32_t>(exit_code));
    g_queue.push_back(m);
    g_queue_cond.notify_all();
}""",
    "THETAZ_PostQuitMessage_Wrapper": """void WINAPI_CALL THETAZ_PostQuitMessage_Wrapper(int exit_code) {
    if (x86_dispatch_active()) {
        THETAZ_PostQuitMessage(exit_code);
    } else {
        WINAPI64_PostQuitMessage(exit_code);
    }
}""",
    "THETAZ_PrintWindow": """BOOL WINAPI_CALL THETAZ_PrintWindow(void* hwnd, void* hdcBlt, UINT nFlags) {
    (void)hwnd; (void)hdcBlt; (void)nFlags;
    return 0;
}""",
    "THETAZ_QISearch": """HRESULT WINAPI_CALL THETAZ_QISearch(void* t,void* q,REFIID iid,void** ppv){return E_NOINTERFACE;}""",
    "THETAZ_QueryActCtxW": """BOOL WINAPI_CALL THETAZ_QueryActCtxW(DWORD f,HANDLE c,void* a,DWORD i,void* b,S sz,S* r){if(r)*r=0;return FALSE;}""",
    "THETAZ_QueryServiceConfigW": """BOOL WINAPI_CALL THETAZ_QueryServiceConfigW(void* hService, void* lpServiceConfig, DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
    (void)hService; (void)lpServiceConfig; (void)cbBufSize;
    if (pcbBytesNeeded) *pcbBytesNeeded = 0;
    return 0;
}""",
    "THETAZ_QueryServiceStatusEx": """BOOL WINAPI_CALL THETAZ_QueryServiceStatusEx(void* hService, DWORD InfoLevel, LPBYTE lpBuffer,
    DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
    (void)hService; (void)InfoLevel; (void)lpBuffer; (void)cbBufSize;
    if (pcbBytesNeeded) *pcbBytesNeeded = 0;
    return 0;
}""",
    "THETAZ_QueueUserAPC": """BOOL WINAPI_CALL THETAZ_QueueUserAPC(void* fn,HANDLE h,ULONG_PTR d){return FALSE;}""",
    "THETAZ_QueueUserWorkItem": """BOOL WINAPI_CALL THETAZ_QueueUserWorkItem(void* fn,void* c,ULONG f){return FALSE;}""",
    "THETAZ_RaiseException": """void WINAPI_CALL THETAZ_RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags,
                                           DWORD nNumberOfArguments, void* lpArguments);
    uint32_t args[4];
    args[0] = 0x19930522; // V4 magic (any accepted by __CxxFrameHandler)
    args[1] = p_exception_object;
    args[2] = p_throw_info;
    args[3] = 0;
    THETAZ_RaiseException(0xE06D7363, 1 /* EXCEPTION_NONCONTINUABLE */, 4, args);
}

// CRT local-unwind helpers used by longjmp through SEH frames.
static void WINAPI_CALL ucrt__local_unwind2(void* frame_, int trylevel) {
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t scopetable = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 8));
    uint32_t saved_ebp = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 16));
    uint32_t lvl = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 12));
    if (!scopetable || scopetable >= 0x80000000u) return;
    while (lvl != 0xFFFFFFFFu && lvl != static_cast<uint32_t>(trylevel)) {
        const uint32_t* e =
            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + lvl * 12));
        uint32_t filter = e[1], handler = e[2];
        lvl = e[0];
        if (!filter && handler)
            x86_call_guest_args_ebp(handler, nullptr, 0, saved_ebp);
    }
}""",
    "THETAZ_RaiseFailFastException": """void WINAPI_CALL THETAZ_RaiseFailFastException(void* e,void* c,DWORD f){fprintf(stderr,"[kernel32] RaiseFailFastException\n");abort();}""",
    "THETAZ_ReadProcessMemory": """BOOL WINAPI_CALL THETAZ_ReadProcessMemory(HANDLE p,const void* a,void* b,S sz,S* r){memcpy(b,a,sz);if(r)*r=sz;return TRUE;}""",
    "THETAZ_RectVisible": """int WINAPI_CALL THETAZ_RectVisible(uint32_t hdc, const void* rect) {
    return 0;
}""",
    "THETAZ_Rectangle": """int WINAPI_CALL THETAZ_Rectangle(uint32_t hdc, int l, int t, int r, int b) {
    return 1;
}""",
    "THETAZ_RedrawWindow": """int WINAPI_CALL THETAZ_RedrawWindow(uint32_t hwnd, const void* lprcUpdate, uint32_t hrgnUpdate, uint32_t flags) { return 1; }""",
    "THETAZ_RegEnumValueW": """LONG WINAPI_CALL THETAZ_RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName,
    LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    (void)hKey; (void)dwIndex; (void)lpValueName; (void)lpcchValueName;
    (void)lpReserved; (void)lpType; (void)lpData; (void)lpcbData;
    return 259; // ERROR_NO_MORE_ITEMS
}""",
    "THETAZ_RegisterApplicationRecoveryCallback": """HRESULT WINAPI_CALL THETAZ_RegisterApplicationRecoveryCallback(void* p,void* pa,DWORD pg,DWORD f){return 0;}""",
    "THETAZ_RegisterApplicationRestart": """HRESULT WINAPI_CALL THETAZ_RegisterApplicationRestart(const wchar_t* c,DWORD f){return 0;}""",
    "THETAZ_RegisterTraceGuidsW": """DWORD WINAPI_CALL THETAZ_RegisterTraceGuidsW(const wchar_t* pProviderGuid, void* pEnableCallback,
    const void* ControlGuid, DWORD GuidCount, void* RegistrationHandle) {
    (void)pProviderGuid; (void)pEnableCallback; (void)ControlGuid;
    (void)GuidCount; (void)RegistrationHandle;
    return 0;
}""",
    "THETAZ_ReleaseActCtx": """void WINAPI_CALL THETAZ_ReleaseActCtx(HANDLE c){}""",
    "THETAZ_ReleaseCapture": """int WINAPI_CALL THETAZ_ReleaseCapture() { return 1; }""",
    "THETAZ_ReplaceFileW": """BOOL WINAPI_CALL THETAZ_ReplaceFileW(const wchar_t* o,const wchar_t* r,const wchar_t* b,DWORD f,void* p,void* v){return TRUE;}""",
    "THETAZ_ReportEventW": """BOOL WINAPI_CALL THETAZ_ReportEventW(HANDLE hEventLog, WORD wType, DWORD dwCategory, DWORD dwEventID,
    void* lpUserSid, WORD wNumStrings, DWORD dwDataSize, const wchar_t** lpStrings, void* lpRawData) {
    (void)hEventLog; (void)wType; (void)dwCategory; (void)dwEventID;
    (void)lpUserSid; (void)wNumStrings; (void)dwDataSize; (void)lpStrings; (void)lpRawData;
    return 1;
}""",
    "THETAZ_ResolveLocaleName": """int WINAPI_CALL THETAZ_ResolveLocaleName(const wchar_t* n,wchar_t* b,int bl){if(b&&bl>0)wcscpy(b,L"en-US");return 5;}""",
    "THETAZ_ResumeThread": """BOOL WINAPI_CALL THETAZ_ResumeThread(HANDLE hThread) {
    (void)hThread;
    return 1;
}""",
    "THETAZ_RevertToSelf": """BOOL WINAPI_CALL THETAZ_RevertToSelf() {
    return 1;
}""",
    "THETAZ_RtlAddFunctionTable": """void WINAPI_CALL THETAZ_RtlAddFunctionTable(void* t,DWORD c,uint64_t b){}""",
    "THETAZ_RtlDeleteFunctionTable": """void WINAPI_CALL THETAZ_RtlDeleteFunctionTable(void* t){}""",
    "THETAZ_RtlRestoreContext": """void WINAPI_CALL THETAZ_RtlRestoreContext(void* c,void* e){}""",
    "THETAZ_SHCreateStreamOnFileEx": """HRESULT WINAPI_CALL THETAZ_SHCreateStreamOnFileEx(LPCWSTR pszFile, DWORD grfMode, DWORD dwAttributes,
    BOOL fCreate, void* pstmTemplate, void** ppstm) {
    (void)pszFile; (void)grfMode; (void)dwAttributes; (void)fCreate; (void)pstmTemplate;
    if (ppstm) *ppstm = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_SHCreateStreamOnFileW": """HRESULT WINAPI_CALL THETAZ_SHCreateStreamOnFileW(LPCWSTR pszFile, DWORD grfMode, void** ppstm) {
    (void)pszFile; (void)grfMode;
    if (ppstm) *ppstm = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_SHDeleteKeyW": """LONG WINAPI_CALL THETAZ_SHDeleteKeyW(void* hkey, LPCWSTR pszSubKey) {
    (void)hkey; (void)pszSubKey;
    return 0; // ERROR_SUCCESS
}""",
    "THETAZ_SHGetDesktopFolder": """HRESULT WINAPI_CALL THETAZ_SHGetDesktopFolder(void** ppshf) {
    (void)ppshf;
    return E_NOTIMPL;
}""",
    "THETAZ_SHGetFolderLocation": """HRESULT WINAPI_CALL THETAZ_SHGetFolderLocation(void* hwnd, int csidl, void* hToken, DWORD dwFlags, void** ppidl) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags;
    if (ppidl) *ppidl = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_SHGetFolderPathA": """HRESULT WINAPI_CALL THETAZ_SHGetFolderPathA(void* hwnd, int csidl, void* hToken, DWORD dwFlags, LPSTR pszPath) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags; (void)pszPath;
    return E_NOTIMPL;
}""",
    "THETAZ_SHGetFolderPathW": """HRESULT WINAPI_CALL THETAZ_SHGetFolderPathW(void* hwnd, int csidl, void* hToken, DWORD dwFlags, LPWSTR pszPath) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags; (void)pszPath;
    return E_NOTIMPL;
}""",
    "THETAZ_SHGetKnownFolderPath": """HRESULT WINAPI_CALL THETAZ_SHGetKnownFolderPath(void* rfid, DWORD dwFlags, void* hToken, LPWSTR* ppszPath) {
    (void)rfid; (void)dwFlags; (void)hToken;
    if (ppszPath) *ppszPath = nullptr;
    return E_NOTIMPL;
}""",
    "THETAZ_SHGetMalloc": """HRESULT WINAPI_CALL THETAZ_SHGetMalloc(void** ppMalloc) {
    (void)ppMalloc;
    return E_NOTIMPL;
}""",
    "THETAZ_SHLoadIndirectString": """HRESULT WINAPI_CALL THETAZ_SHLoadIndirectString(const wchar_t* s,wchar_t* b,uint32_t l,void** r){
    if(s&&b&&l>0){wcsncpy(b,s,l);return S_OK;}return E_FAIL;
}""",
    "THETAZ_SHRegCloseUSKey": """void WINAPI_CALL THETAZ_SHRegCloseUSKey(void* h){}""",
    "THETAZ_SHRegGetBoolUSValueA": """BOOL WINAPI_CALL THETAZ_SHRegGetBoolUSValueA(const char* k,const char* n,BOOL i,BOOL d){return d;}""",
    "THETAZ_SHRegGetBoolUSValueW": """BOOL WINAPI_CALL THETAZ_SHRegGetBoolUSValueW(const wchar_t* k,const wchar_t* n,BOOL i,BOOL d){return d;}""",
    "THETAZ_SHTruncateString": """void WINAPI_CALL THETAZ_SHTruncateString(wchar_t* s,int ml){if(s&&ml>=0&&(int)wcslen(s)>ml)s[ml]=0;}""",
    "THETAZ_SLGetGenuineInformation": """LONG WINAPI_CALL THETAZ_SLGetGenuineInformation(void* pQuery, void* pValueName, DWORD* pcbValue, DWORD* pvValue) {
    (void)pQuery; (void)pValueName;
    if (pcbValue) *pcbValue = 0;
    if (pvValue) *pvValue = 0;
    return 0;
}""",
    "THETAZ_SLIsGenuineLocal": """HRESULT WINAPI_CALL THETAZ_SLIsGenuineLocal(void* pSLId, DWORD* pValue, void* pReserved) {
    (void)pSLId;
    if (pValue) *pValue = 1; // SLG_STATE_GENUINE
    (void)pReserved;
    return 0;
}""",
    "THETAZ_SafeArrayCreate": """HRESULT WINAPI_CALL THETAZ_SafeArrayCreate(DWORD vt, UINT cDims, void* rgsabound) {
    (void)vt; (void)cDims; (void)rgsabound;
    return E_NOTIMPL;
}""",
    "THETAZ_SafeArrayDestroy": """HRESULT WINAPI_CALL THETAZ_SafeArrayDestroy(void* psa) {
    (void)psa;
    return E_NOTIMPL;
}""",
    "THETAZ_SafeArrayGetDim": """UINT WINAPI_CALL THETAZ_SafeArrayGetDim(void* psa) {
    (void)psa;
    return 0;
}""",
    "THETAZ_SafeArrayGetLBound": """HRESULT WINAPI_CALL THETAZ_SafeArrayGetLBound(void* psa, UINT nDim, long* plLbound) {
    (void)psa; (void)nDim; (void)plLbound;
    return E_NOTIMPL;
}""",
    "THETAZ_ScreenToClient": """int WINAPI_CALL THETAZ_ScreenToClient(uint32_t hwnd, void* pt) { return 1; }""",
    "THETAZ_ScrollWindow": """int WINAPI_CALL THETAZ_ScrollWindow(uint32_t hwnd, int dx, int dy, const void* rect, const void* clip) { return 1; }""",
    "THETAZ_ScrollWindowEx": """int WINAPI_CALL THETAZ_ScrollWindowEx(uint32_t hwnd, int dx, int dy, const void* rect, const void* clip, uint32_t hrgnUpdate, void* update, uint32_t flags) { return 1; }""",
    "THETAZ_SearchPathA": """DWORD WINAPI_CALL THETAZ_SearchPathA(const char* d,const char* f,const char* e,DWORD l,char* b,char** p){
    if(!b||l==0)return 0;if(d)snprintf(b,l,"%s\\%s",d,f);else snprintf(b,l,"%s",f);if(p)*p=b;return (DWORD)strlen(b);
}""",
    "THETAZ_SearchPathW": """DWORD WINAPI_CALL THETAZ_SearchPathW(const wchar_t* d,const wchar_t* f,const wchar_t* e,DWORD l,wchar_t* b,wchar_t** p){
    if(!b||l==0)return 0;if(d)swprintf((wchar_t*)b,l,L"%ls\\%ls",d,f);else swprintf((wchar_t*)b,l,L"%ls",f);if(p)*p=b;return (DWORD)wcslen(b);
}""",
    "THETAZ_SelectClipRgn": """int WINAPI_CALL THETAZ_SelectClipRgn(uint32_t hdc, uint32_t hrgn) {
    return 1;
}""",
    "THETAZ_SendDlgItemMessageW": """int WINAPI_CALL THETAZ_SendDlgItemMessageW(uint32_t hwnd, int id, uint32_t msg, WPARAM wParam, LPARAM lParam) { return 0; }""",
    "THETAZ_SendMessageW": """LRESULT WINAPI_CALL THETAZ_SendMessageW(uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    using SendMessageA_fn = LRESULT(WINAPI_CALL*)(uint32_t, uint32_t, WPARAM, LPARAM);
    auto* fn = (SendMessageA_fn)ApiRegistry::instance().find_function("user32.dll", "SendMessageA");
    if (!fn) return 0;
    return fn(hwnd, msg, wParam, lParam);
}""",
    "THETAZ_SetCapture": """int WINAPI_CALL THETAZ_SetCapture(uint32_t hwnd) { return 1; }""",
    "THETAZ_SetCaretPos": """int WINAPI_CALL THETAZ_SetCaretPos(int x, int y) { return 1; }""",
    "THETAZ_SetDIBits": """int WINAPI_CALL THETAZ_SetDIBits(uint32_t hdc, uint32_t hbm, uint32_t start, uint32_t cLines, const void* bits, const void* bmi, uint32_t usage) {
    return 0;
}""",
    "THETAZ_SetDlgItemTextA": """BOOL WINAPI_CALL THETAZ_SetDlgItemTextA(WinHandle hdlg, int id, const char* text) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl) return FALSE_VALUE;
    THETAZ_SendMessageA(ctrl, WM_SETTEXT, 0, reinterpret_cast<uint64_t>(text));
    return TRUE_VALUE;
}""",
    "THETAZ_SetDlgItemTextA_Wrapper": """BOOL WINAPI_CALL THETAZ_SetDlgItemTextA_Wrapper(WinHandle hdlg, int id, const char* text) {
    if (x86_dispatch_active()) return THETAZ_SetDlgItemTextA(hdlg, id, text);
    return WINAPI64_SetDlgItemTextA(hdlg, id, text);
}""",
    "THETAZ_SetErrorInfo": """LONG WINAPI_CALL THETAZ_SetErrorInfo(DWORD dwReserved, void* perrinfo) {
    (void)dwReserved;
    (void)perrinfo;
    return 0; // S_OK
}""",
    "THETAZ_SetFileAttributesA": """BOOL WINAPI_CALL THETAZ_SetFileAttributesA(const char* n,DWORD a){return TRUE;}""",
    "THETAZ_SetFileSecurityW": """BOOL WINAPI_CALL THETAZ_SetFileSecurityW(const wchar_t* lpFileName, DWORD SecurityInformation,
    void* pSecurityDescriptor) {
    (void)lpFileName; (void)SecurityInformation; (void)pSecurityDescriptor;
    return 0;
}""",
    "THETAZ_SetInformationJobObject": """BOOL WINAPI_CALL THETAZ_SetInformationJobObject(HANDLE j,int c,void* i,DWORD s){return TRUE;}""",
    "THETAZ_SetNamedPipeHandleState": """BOOL WINAPI_CALL THETAZ_SetNamedPipeHandleState(HANDLE p,DWORD* m,DWORD* c,DWORD* d){return FALSE;}""",
    "THETAZ_SetPolyFillMode": """int WINAPI_CALL THETAZ_SetPolyFillMode(uint32_t hdc, int mode) {
    return 0;
}""",
    "THETAZ_SetPriorityClass": """BOOL WINAPI_CALL THETAZ_SetPriorityClass(HANDLE p,DWORD c){return TRUE;}""",
    "THETAZ_SetProcessAffinityMask": """BOOL WINAPI_CALL THETAZ_SetProcessAffinityMask(HANDLE p,DWORD_PTR m){return TRUE;}""",
    "THETAZ_SetScrollInfo": """int WINAPI_CALL THETAZ_SetScrollInfo(uint32_t hwnd, int bar, const void* si, int redraw) { return 0; }""",
    "THETAZ_SetScrollPos": """int WINAPI_CALL THETAZ_SetScrollPos(uint32_t hwnd, int bar, int pos, int redraw) { return pos; }""",
    "THETAZ_SetScrollRange": """int WINAPI_CALL THETAZ_SetScrollRange(uint32_t hwnd, int bar, int min, int max, int redraw) { return 1; }""",
    "THETAZ_SetSecurityDescriptorDacl": """BOOL WINAPI_CALL THETAZ_SetSecurityDescriptorDacl(void* pSecurityDescriptor, BOOL bDaclPresent,
    void* pDacl, BOOL bDaclDefaulted) {
    (void)pSecurityDescriptor; (void)bDaclPresent; (void)pDacl; (void)bDaclDefaulted;
    return 1;
}""",
    "THETAZ_SetSecurityDescriptorOwner": """BOOL WINAPI_CALL THETAZ_SetSecurityDescriptorOwner(void* pSecurityDescriptor, void* pOwner, BOOL bOwnerDefaulted) {
    (void)pSecurityDescriptor; (void)pOwner; (void)bOwnerDefaulted;
    return 1;
}""",
    "THETAZ_SetSecurityDescriptorSacl": """BOOL WINAPI_CALL THETAZ_SetSecurityDescriptorSacl(void* pSecurityDescriptor, BOOL bSaclPresent,
    void* pSacl, BOOL bSaclDefaulted) {
    (void)pSecurityDescriptor; (void)bSaclPresent; (void)pSacl; (void)bSaclDefaulted;
    return 1;
}""",
    "THETAZ_SetTextAlign": """int WINAPI_CALL THETAZ_SetTextAlign(uint32_t hdc, uint32_t align) {
    return 0;
}""",
    "THETAZ_SetThreadContext": """BOOL WINAPI_CALL THETAZ_SetThreadContext(HANDLE hThread, WOW64CONTEXT* ctx) {
    (void)hThread; (void)ctx;
    // Could be used to modify thread context, but for now just return success
    return TRUE_VALUE;
}""",
    "THETAZ_SetThreadDescription": """HRESULT WINAPI_CALL THETAZ_SetThreadDescription(HANDLE h,const wchar_t* d){return S_OK;}""",
    "THETAZ_SetThreadErrorMode": """DWORD WINAPI_CALL THETAZ_SetThreadErrorMode(DWORD m,DWORD* o){if(o)*o=0;return TRUE;}""",
    "THETAZ_SetThreadGroupAffinity": """BOOL WINAPI_CALL THETAZ_SetThreadGroupAffinity(HANDLE t,const void* a,void* p){return TRUE;}""",
    "THETAZ_SetThreadPreferredUILanguages": """BOOL WINAPI_CALL THETAZ_SetThreadPreferredUILanguages(DWORD f,const wchar_t* l,DWORD* o){if(o)*o=0;return TRUE;}""",
    "THETAZ_SetThreadpoolThreadMaximum": """void WINAPI_CALL THETAZ_SetThreadpoolThreadMaximum(void* p,DWORD n){}""",
    "THETAZ_SetThreadpoolThreadMinimum": """BOOL WINAPI_CALL THETAZ_SetThreadpoolThreadMinimum(void* p,DWORD n){return TRUE;}""",
    "THETAZ_SetViewportOrgEx": """int WINAPI_CALL THETAZ_SetViewportOrgEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}""",
    "THETAZ_SetWindowLongA": """int WINAPI_CALL THETAZ_SetWindowLongA(uint32_t hwnd, int index, int value) { return 0; }""",
    "THETAZ_SetWindowLongW": """int WINAPI_CALL THETAZ_SetWindowLongW(uint32_t hwnd, int index, int value) {
    return THETAZ_SetWindowLongA(hwnd, index, value);
}""",
    "THETAZ_SetWindowOrgEx": """int WINAPI_CALL THETAZ_SetWindowOrgEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}""",
    "THETAZ_SetWindowPos": """int WINAPI_CALL THETAZ_SetWindowPos(uint32_t hwnd, uint32_t insertafter, int x, int y, int cx, int cy, uint32_t flags) { return 1; }""",
    "THETAZ_SetWindowRgn": """int WINAPI_CALL THETAZ_SetWindowRgn(uint32_t hwnd, uint32_t hrgn, int redraw) { return 1; }""",
    "THETAZ_SetWindowTextA": """BOOL WINAPI_CALL THETAZ_SetWindowTextA(WinHandle hwnd, const char* text) {
    if (!hwnd) return FALSE_VALUE;
    return THETAZ_SendMessageA(hwnd, WM_SETTEXT, 0, reinterpret_cast<uint64_t>(text))
        ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_SetWindowTextA_Wrapper": """BOOL WINAPI_CALL THETAZ_SetWindowTextA_Wrapper(WinHandle hwnd, const char* text) {
    if (x86_dispatch_active()) return THETAZ_SetWindowTextA(hwnd, text);
    return WINAPI64_SetWindowTextA(hwnd, text);
}""",
    "THETAZ_SetWindowTextW": """int WINAPI_CALL THETAZ_SetWindowTextW(uint32_t hwnd, const uint16_t* str) { return 1; }""",
    "THETAZ_ShowCaret": """int WINAPI_CALL THETAZ_ShowCaret(uint32_t hwnd) { return 1; }""",
    "THETAZ_ShowScrollBar": """int WINAPI_CALL THETAZ_ShowScrollBar(uint32_t hwnd, int bar, int show) { return 1; }""",
    "THETAZ_ShowWindow": """BOOL WINAPI_CALL THETAZ_ShowWindow(WinHandle hwnd, int cmd_show) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    ::Window xw = 0;
    bool was_visible;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(h);
        if (!win) return FALSE_VALUE;
        was_visible = win->visible;
        win->visible = (cmd_show != 0);
        auto it = g_x11_windows.find(h);
        if (it != g_x11_windows.end()) xw = it->second;
    }
    if (g_x11_display && xw) {
        std::lock_guard<std::mutex> lock(g_x11_mutex);
        if (cmd_show == 0) {
            XUnmapWindow(g_x11_display, xw);
        } else {
            XMapRaised(g_x11_display, xw);
        }
        XFlush(g_x11_display);
    }
    return was_visible ? TRUE_VALUE : FALSE_VALUE;
}""",
    "THETAZ_ShowWindow_Wrapper": """BOOL WINAPI_CALL THETAZ_ShowWindow_Wrapper(WinHandle hwnd, int cmd_show) {
    if (x86_dispatch_active()) {
        return THETAZ_ShowWindow(hwnd, cmd_show);
    } else {
        return WINAPI64_ShowWindow(hwnd, cmd_show);
    }
}""",
    "THETAZ_SignalObjectAndWait": """DWORD WINAPI_CALL THETAZ_SignalObjectAndWait(HANDLE s,HANDLE w,DWORD ms,BOOL a){usleep(ms*1000);return 258;}""",
    "THETAZ_SleepConditionVariableCS": """BOOL WINAPI_CALL THETAZ_SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds){
    if(CriticalSection){THETAZ_LeaveCriticalSection(CriticalSection);usleep(dwMilliseconds*1000);THETAZ_EnterCriticalSection(CriticalSection);}return TRUE;
}""",
    "THETAZ_SleepEx": """BOOL WINAPI_CALL THETAZ_SleepEx(DWORD ms,BOOL a){usleep(ms*1000);return FALSE;}""",
    "THETAZ_StartServiceW": """BOOL WINAPI_CALL THETAZ_StartServiceW(void* hService, DWORD dwNumServiceArgs, const wchar_t** lpServiceArgVectors) {
    (void)hService; (void)dwNumServiceArgs; (void)lpServiceArgVectors;
    return 0;
}""",
    "THETAZ_StartThreadpoolIo": """void WINAPI_CALL THETAZ_StartThreadpoolIo(TP_IO* i){}""",
    "THETAZ_StrCSpnA": """int WINAPI_CALL THETAZ_StrCSpnA(const char* s,const char* se){return(int)(strcspn(s,se));}""",
    "THETAZ_StrCSpnIA": """int WINAPI_CALL THETAZ_StrCSpnIA(const char* s,const char* se){return(int)(strcspn(s,se));}""",
    "THETAZ_StrCSpnIW": """int WINAPI_CALL THETAZ_StrCSpnIW(const wchar_t* s,const wchar_t* se){return(int)(wcscspn(s,se));}""",
    "THETAZ_StrCSpnW": """int WINAPI_CALL THETAZ_StrCSpnW(const wchar_t* s,const wchar_t* se){return(int)(wcscspn(s,se));}""",
    "THETAZ_StrCatChainW": """DWORD WINAPI_CALL THETAZ_StrCatChainW(wchar_t* d,DWORD mx,DWORD idx,const wchar_t* s){if(!d||!s)return 0;int sl=(int)wcslen(s);if((int)idx<mx-1){int cp=std::min(sl,(int)(mx-idx-1));memcpy(d+idx,s,cp*sizeof(wchar_t));d[idx+cp]=0;}return idx+sl;}""",
    "THETAZ_StrCmpA": """int WINAPI_CALL THETAZ_StrCmpA(const char* a,const char* b){return strcmp(a?a:"",b?b:"");}""",
    "THETAZ_StrCmpCA": """int WINAPI_CALL THETAZ_StrCmpCA(const char* a,const char* b){return strcmp(a?a:"",b?b:"");}""",
    "THETAZ_StrCmpCW": """int WINAPI_CALL THETAZ_StrCmpCW(const wchar_t* a,const wchar_t* b){return wcscmp(a?a:L"",b?b:L"");}""",
    "THETAZ_StrCmpICA": """int WINAPI_CALL THETAZ_StrCmpICA(const char* a,const char* b){return strcasecmp(a?a:"",b?b:"");}""",
    "THETAZ_StrCmpICW": """int WINAPI_CALL THETAZ_StrCmpICW(const wchar_t* a,const wchar_t* b){return wcscasecmp(a?a:L"",b?b:L"");}""",
    "THETAZ_StrCmpIW": """int WINAPI_CALL THETAZ_StrCmpIW(const uint16_t* s1, const uint16_t* s2) {
    return wstrcmpi(s1, s2);
}""",
    "THETAZ_StrCmpLogicalW": """int WINAPI_CALL THETAZ_StrCmpLogicalW(const wchar_t* a,const wchar_t* b){return wcscmp(a?a:L"",b?b:L"");}""",
    "THETAZ_StrCmpNA": """int WINAPI_CALL THETAZ_StrCmpNA(const char* a,const char* b,int n){return strncmp(a?a:"",b?b:"",n);}""",
    "THETAZ_StrCmpNCA": """int WINAPI_CALL THETAZ_StrCmpNCA(const char* a,const char* b,int n){return strncasecmp(a?a:"",b?b:"",n);}""",
    "THETAZ_StrCmpNCW": """int WINAPI_CALL THETAZ_StrCmpNCW(const wchar_t* a,const wchar_t* b,int n){return wcsncasecmp(a?a:L"",b?b:L"",n);}""",
    "THETAZ_StrCmpNIA": """int WINAPI_CALL THETAZ_StrCmpNIA(const char* a,const char* b,int n){return strncasecmp(a?a:"",b?b:"",n);}""",
    "THETAZ_StrCmpNICA": """int WINAPI_CALL THETAZ_StrCmpNICA(const char* a,const char* b,int n){if(!a)a="";if(!b)b="";return strncasecmp(a,b,n);}""",
    "THETAZ_StrCmpNICW": """int WINAPI_CALL THETAZ_StrCmpNICW(const wchar_t* a,const wchar_t* b,int n){return wcsncasecmp(a?a:L"",b?b:L"",n);}""",
    "THETAZ_StrCmpNIW": """int WINAPI_CALL THETAZ_StrCmpNIW(const uint16_t* s1, const uint16_t* s2, int n) {
    for (int i = 0; i < n; i++) {
        int d = (int)towlower(s1 ? s1[i] : 0) - (int)towlower(s2 ? s2[i] : 0);
        if (d) return d;
        if (!s1[i]) break;
    }
    return 0;
}""",
    "THETAZ_StrCmpNW": """int WINAPI_CALL THETAZ_StrCmpNW(const wchar_t* a,const wchar_t* b,int n){return wcsncmp(a?a:L"",b?b:L"",n);}""",
    "THETAZ_StrCmpW": """int WINAPI_CALL THETAZ_StrCmpW(const wchar_t* a,const wchar_t* b){return wcscmp(a?a:L"",b?b:L"");}""",
    "THETAZ_StrIsIntlEqualA": """BOOL WINAPI_CALL THETAZ_StrIsIntlEqualA(BOOL cs,const char* a,const char* b,int n){return strncasecmp(a?a:"",b?b:"",n)==0;}""",
    "THETAZ_StrIsIntlEqualW": """BOOL WINAPI_CALL THETAZ_StrIsIntlEqualW(BOOL cs,const wchar_t* a,const wchar_t* b,int n){return wcsncasecmp(a?a:L"",b?b:L"",n)==0;}""",
    "THETAZ_StrSpnA": """int WINAPI_CALL THETAZ_StrSpnA(const char* s,const char* se){return(int)strspn(s,se);}""",
    "THETAZ_StrSpnW": """int WINAPI_CALL THETAZ_StrSpnW(const wchar_t* s,const wchar_t* se){return(int)wcscspn(s,se);}""",
    "THETAZ_StrToIntA": """int WINAPI_CALL THETAZ_StrToIntA(const char* s){return atoi(s?s:"0");}""",
    "THETAZ_StrToIntExA": """int WINAPI_CALL THETAZ_StrToIntExA(const char* s,DWORD f,int* v){if(v)*v=s?atoi(s):0;return S_OK;}""",
    "THETAZ_StrToIntExW": """int WINAPI_CALL THETAZ_StrToIntExW(const wchar_t* s,DWORD f,int* v){if(v)*v=0;return S_OK;}""",
    "THETAZ_StrToIntW": """int WINAPI_CALL THETAZ_StrToIntW(const wchar_t* s){return (int)wcstol(s?s:L"0",nullptr,10);}""",
    "THETAZ_StrTrimA": """void WINAPI_CALL THETAZ_StrTrimA(char* s){if(!s)return;int l=(int)strlen(s);while(l>0&&(s[l-1]==' '||s[l-1]=='\t'))s[--l]=0;char* st=s;while(*st==' '||*st=='\t')st++;if(st!=s)memmove(s,st,strlen(st)+1);}""",
    "THETAZ_StrTrimW": """void WINAPI_CALL THETAZ_StrTrimW(wchar_t* s){if(!s)return;int l=(int)wcslen(s);while(l>0&&(s[l-1]==L' '||s[l-1]==L'\t'))s[--l]=0;wchar_t* st=s;while(*st==L' '||*st==L'\t')st++;if(st!=s)memmove(s,st,(wcslen(st)+1)*2);}""",
    "THETAZ_StretchBlt": """int WINAPI_CALL THETAZ_StretchBlt(uint32_t hdcDst, int dstx, int dsty, int dstw, int dsth, uint32_t hdcSrc, int srcx, int srcy, int srcw, int srch, uint32_t rop) {
    return 1;
}""",
    "THETAZ_StretchDIBits": """int WINAPI_CALL THETAZ_StretchDIBits(uint32_t hdc, int dstx, int dsty, int dstw, int dsth, int srcx, int srcy, int srcw, int srch, const void* bits, const void* bmi, uint32_t usage, uint32_t rop) {
    return 1;
}""",
    "THETAZ_SubtractRect": """int WINAPI_CALL THETAZ_SubtractRect(void* dst, const void* r1, const void* r2) {
    if (dst) memset(dst, 0, 16);
    return 0;
}""",
    "THETAZ_SuspendThread": """BOOL WINAPI_CALL THETAZ_SuspendThread(HANDLE hThread) {
    (void)hThread;
    // Return dummy success
    return 0;
}""",
    "THETAZ_SysFreeString": """void WINAPI_CALL THETAZ_SysFreeString(BSTR bstr) {
    free_bstr(bstr);
}""",
    "THETAZ_SysReAllocString": """int WINAPI_CALL THETAZ_SysReAllocString(BSTR* pbstr, const OLECHAR* psz) {
    if (!pbstr) return 0;
    free_bstr(*pbstr);
    *pbstr = THETAZ_SysAllocString(psz);
    return *pbstr != nullptr ? 1 : 0;
}""",
    "THETAZ_SysReAllocStringLen": """int WINAPI_CALL THETAZ_SysReAllocStringLen(BSTR* pbstr, const OLECHAR* psz, UINT len) {
    if (!pbstr) return 0;
    free_bstr(*pbstr);
    *pbstr = THETAZ_SysAllocStringLen(psz, len);
    return *pbstr != nullptr ? 1 : 0;
}""",
    "THETAZ_SysStringLen": """UINT WINAPI_CALL THETAZ_SysStringLen(BSTR bstr) {
    return static_cast<UINT>(bstr_byte_len(bstr) / sizeof(OLECHAR));
}""",
    "THETAZ_SystemTimeToTzSpecificLocalTime": """BOOL WINAPI_CALL THETAZ_SystemTimeToTzSpecificLocalTime(void* tz,SYSTEMTIME* s,SYSTEMTIME* l){if(l&&s)*l=*s;return TRUE;}""",
    "THETAZ_TerminateJobObject": """BOOL WINAPI_CALL THETAZ_TerminateJobObject(HANDLE j,UINT c){return TRUE;}""",
    "THETAZ_TerminateThread": """BOOL WINAPI_CALL THETAZ_TerminateThread(HANDLE t,DWORD c){return TRUE;}""",
    "THETAZ_TrackMouseEvent": """int WINAPI_CALL THETAZ_TrackMouseEvent(void* tme) { return 1; }""",
    "THETAZ_TrackPopupMenu": """int WINAPI_CALL THETAZ_TrackPopupMenu(uint32_t hmenu, uint32_t flags, int x, int y, int reserved, uint32_t hwnd, const void* rect) { return 1; }""",
    "THETAZ_TranslateMessage": """BOOL WINAPI_CALL THETAZ_TranslateMessage(const void* msg) {
    if (!msg) return FALSE_VALUE;
    const uint8_t* m = static_cast<const uint8_t*>(msg);
    uint32_t message = *reinterpret_cast<const uint32_t*>(m + MSG_MESSAGE);
    if (message == WM_KEYDOWN) {
        uint64_t wparam = *reinterpret_cast<const uint32_t*>(m + MSG_WPARAM);
        // Synthesize WM_CHAR for printable-ish keys (no keyboard state model).
        THETAZ_PostMessageA(reinterpret_cast<WinHandle>(*reinterpret_cast<const uint32_t*>(m + MSG_HWND)),
                            WM_CHAR, wparam, 1);
        return TRUE_VALUE;
    }
    return FALSE_VALUE;
}""",
    "THETAZ_TranslateMessage_Wrapper": """BOOL WINAPI_CALL THETAZ_TranslateMessage_Wrapper(const void* msg) {
    if (x86_dispatch_active()) {
        return THETAZ_TranslateMessage(msg);
    } else {
        return WINAPI64_TranslateMessage(msg);
    }
}""",
    "THETAZ_TrySubmitThreadpoolCallback": """BOOL WINAPI_CALL THETAZ_TrySubmitThreadpoolCallback(void* c,void* x,void* e){return TRUE;}""",
    "THETAZ_UnionRect": """int WINAPI_CALL THETAZ_UnionRect(void* dst, const void* r1, const void* r2) {
    if (dst) memset(dst, 0, 16);
    return 1;
}""",
    "THETAZ_UnregisterClassA": """BOOL WINAPI_CALL THETAZ_UnregisterClassA(const char* lpClassName,void* hInstance){
    (void)lpClassName;(void)hInstance;
    return TRUE;
}""",
    "THETAZ_UnregisterClassW": """int WINAPI_CALL THETAZ_UnregisterClassW(const uint16_t* cls, uint32_t hinst) { return 1; }""",
    "THETAZ_UnregisterTraceGuids": """DWORD WINAPI_CALL THETAZ_UnregisterTraceGuids(void* RegistrationHandle) {
    (void)RegistrationHandle;
    return 0;
}""",
    "THETAZ_UpdateProcThreadAttribute": """BOOL WINAPI_CALL THETAZ_UpdateProcThreadAttribute(void* l,DWORD f,DWORD a,void* v,S sz,void* p,void* r){return TRUE;}""",
    "THETAZ_UpdateWindow": """BOOL WINAPI_CALL THETAZ_UpdateWindow(WinHandle hwnd) {
    (void)hwnd;
    return TRUE_VALUE;
}""",
    "THETAZ_UpdateWindow_Wrapper": """BOOL WINAPI_CALL THETAZ_UpdateWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) {
        return THETAZ_UpdateWindow(hwnd);
    } else {
        return WINAPI64_UpdateWindow(hwnd);
    }
}""",
    "THETAZ_UrlApplySchemeA": """HRESULT WINAPI_CALL THETAZ_UrlApplySchemeA(const char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlApplySchemeW": """HRESULT WINAPI_CALL THETAZ_UrlApplySchemeW(const wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlCanonicalizeA": """HRESULT WINAPI_CALL THETAZ_UrlCanonicalizeA(const char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlCanonicalizeW": """HRESULT WINAPI_CALL THETAZ_UrlCanonicalizeW(const wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlCombineA": """HRESULT WINAPI_CALL THETAZ_UrlCombineA(const char* ba,const char* re,char* co,uint32_t* l,DWORD f){if(co&&ba&&l){strncpy(co,ba,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlCombineW": """HRESULT WINAPI_CALL THETAZ_UrlCombineW(const wchar_t* ba,const wchar_t* re,wchar_t* co,uint32_t* l,DWORD f){if(co&&ba&&l){wcsncpy(co,ba,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlCompareA": """HRESULT WINAPI_CALL THETAZ_UrlCompareA(const char* a,const char* b,DWORD f){if(!a||!b)return 0;return strcmp(a,b);}""",
    "THETAZ_UrlCompareW": """HRESULT WINAPI_CALL THETAZ_UrlCompareW(const wchar_t* a,const wchar_t* b,DWORD f){if(!a||!b)return 0;return wcscmp(a,b);}""",
    "THETAZ_UrlCreateFromPathA": """HRESULT WINAPI_CALL THETAZ_UrlCreateFromPathA(const char* p,char* u,uint32_t* l,DWORD f){if(u&&p&&l){strncpy(u,p,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlCreateFromPathW": """HRESULT WINAPI_CALL THETAZ_UrlCreateFromPathW(const wchar_t* p,wchar_t* u,uint32_t* l,DWORD f){if(u&&p&&l){wcsncpy(u,p,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlEscapeA": """HRESULT WINAPI_CALL THETAZ_UrlEscapeA(const char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlEscapeW": """HRESULT WINAPI_CALL THETAZ_UrlEscapeW(const wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlFixupA": """HRESULT WINAPI_CALL THETAZ_UrlFixupA(const char* u,char* b,uint32_t* l){return E_NOTIMPL;}""",
    "THETAZ_UrlFixupW": """HRESULT WINAPI_CALL THETAZ_UrlFixupW(const wchar_t* u,wchar_t* b,uint32_t* l){return E_NOTIMPL;}""",
    "THETAZ_UrlGetPartA": """HRESULT WINAPI_CALL THETAZ_UrlGetPartA(const char* u,char* p,uint32_t* l,DWORD pi,DWORD f){if(p&&u&&l){strncpy(p,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlGetPartW": """HRESULT WINAPI_CALL THETAZ_UrlGetPartW(const wchar_t* u,wchar_t* p,uint32_t* l,DWORD pi,DWORD f){if(p&&u&&l){wcsncpy(p,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlGetSchemeA": """HRESULT WINAPI_CALL THETAZ_UrlGetSchemeA(const char* u,DWORD* s){if(s)*s=0;return S_OK;}""",
    "THETAZ_UrlGetSchemeW": """HRESULT WINAPI_CALL THETAZ_UrlGetSchemeW(const wchar_t* u,DWORD* s){if(s)*s=0;return S_OK;}""",
    "THETAZ_UrlHashA": """HRESULT WINAPI_CALL THETAZ_UrlHashA(const char* u,BYTE* h,DWORD l){if(h&&l>0)memset(h,0,l);return S_OK;}""",
    "THETAZ_UrlHashW": """HRESULT WINAPI_CALL THETAZ_UrlHashW(const wchar_t* u,BYTE* h,DWORD l){if(h&&l>0)memset(h,0,l);return S_OK;}""",
    "THETAZ_UrlIsA": """BOOL WINAPI_CALL THETAZ_UrlIsA(const char* u,DWORD f){return FALSE;}""",
    "THETAZ_UrlIsNoHistoryA": """BOOL WINAPI_CALL THETAZ_UrlIsNoHistoryA(const char* u){return FALSE;}""",
    "THETAZ_UrlIsNoHistoryW": """BOOL WINAPI_CALL THETAZ_UrlIsNoHistoryW(const wchar_t* u){return FALSE;}""",
    "THETAZ_UrlIsOpaqueA": """BOOL WINAPI_CALL THETAZ_UrlIsOpaqueA(const char* u){return FALSE;}""",
    "THETAZ_UrlIsOpaqueW": """BOOL WINAPI_CALL THETAZ_UrlIsOpaqueW(const wchar_t* u){return FALSE;}""",
    "THETAZ_UrlIsW": """BOOL WINAPI_CALL THETAZ_UrlIsW(const wchar_t* u,DWORD f){return FALSE;}""",
    "THETAZ_UrlUnescapeA": """HRESULT WINAPI_CALL THETAZ_UrlUnescapeA(char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_UrlUnescapeInPlaceA": """HRESULT WINAPI_CALL THETAZ_UrlUnescapeInPlaceA(char* u,DWORD f){return S_OK;}""",
    "THETAZ_UrlUnescapeInPlaceW": """HRESULT WINAPI_CALL THETAZ_UrlUnescapeInPlaceW(wchar_t* u,DWORD f){return S_OK;}""",
    "THETAZ_UrlUnescapeW": """HRESULT WINAPI_CALL THETAZ_UrlUnescapeW(wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}""",
    "THETAZ_ValidateRect": """int WINAPI_CALL THETAZ_ValidateRect(uint32_t hwnd, const void* rect) { return 1; }""",
    "THETAZ_VariantChangeType": """LONG WINAPI_CALL THETAZ_VariantChangeType(void* pvargDest, const void* pvargSrc, WORD wFlags, DWORD vt) {
    (void)wFlags;
    (void)vt;
    return THETAZ_VariantCopy(pvargDest, pvargSrc);
}""",
    "THETAZ_VariantClear": """LONG WINAPI_CALL THETAZ_VariantClear(void* pvarg) {
    if (!pvarg) return 0x80000005; // E_FAIL
    uint8_t* b = static_cast<uint8_t*>(pvarg);
    DWORD vt = 0;
    memcpy(&vt, b, 4);
    if (vt == VT_BSTR) {
        BSTR s = nullptr;
        memcpy(&s, b + 8, sizeof(BSTR));
        free_bstr(s);
    }
    memset(b, 0, 16);
    return 0; // S_OK
}""",
    "THETAZ_VariantCopy": """LONG WINAPI_CALL THETAZ_VariantCopy(void* pvargDest, const void* pvargSrc) {
    if (!pvargDest || !pvargSrc) return 0x80000005; // E_FAIL
    const uint8_t* src = static_cast<const uint8_t*>(pvargSrc);
    uint8_t* dst = static_cast<uint8_t*>(pvargDest);
    DWORD vt = 0;
    memcpy(&vt, src, 4);
    memcpy(dst, src, 16);
    if (vt == VT_BSTR) {
        BSTR s = nullptr;
        memcpy(&s, src + 8, sizeof(BSTR));
        BSTR copy = THETAZ_SysAllocString(s);
        memcpy(dst + 8, &copy, sizeof(BSTR));
    }
    return 0; // S_OK
}""",
    "THETAZ_VariantCopyInd": """LONG WINAPI_CALL THETAZ_VariantCopyInd(void* pvargDest, const void* pvargSrc) {
    return THETAZ_VariantCopy(pvargDest, pvargSrc);
}""",
    "THETAZ_VariantInit": """void WINAPI_CALL THETAZ_VariantInit(void* pvarg) {
    if (pvarg) memset(pvarg, 0, 16);
}""",
    "THETAZ_VerQueryValueW": """BOOL WINAPI_CALL THETAZ_VerQueryValueW(const void* pBlock, LPCWSTR lpSubBlock, void** lplpBuffer, UINT* puLen) {
    (void)pBlock; (void)lpSubBlock;
    if (lplpBuffer) *lplpBuffer = nullptr;
    if (puLen) *puLen = 0;
    return 0;
}""",
    "THETAZ_VirtualFree": """BOOL WINAPI_CALL THETAZ_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    WindowsAddressSpace& space = WindowsAddressSpace::instance();
    if (!lpAddress) return 0;

    if (dwFreeType & MEM_RELEASE) {
        return space.release(lpAddress) ? 1 : 0;
    }
    if (dwFreeType & MEM_DECOMMIT) {
        return space.decommit(lpAddress, dwSize) ? 1 : 0;
    }
    return 0;
}""",
    "THETAZ_VirtualProtect": """BOOL WINAPI_CALL THETAZ_VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD* lpflOldProtect) {
    if (!lpAddress) return 0;
    if (WindowsAddressSpace::instance().protect(lpAddress, dwSize, flNewProtect, lpflOldProtect)) return 1;

    // Not tracked by WindowsAddressSpace: guest images mmap'd by the PELoader,
    // Wine DLL blobs and the shim pool are all deliberately kept RW(X) (guest
    // code patches its IAT/data at runtime), and the identity-mapped 32-bit
    // world is always writable through the interpreter. Restricting protection
    // on natively-executed guest memory would SIGSEGV the host process, so just
    // report success against the real backing mapping.
    MemoryBasicInformation info;
    if (query_from_proc_maps(lpAddress, &info)) {
        if (lpflOldProtect) *lpflOldProtect = info.protect;
        return 1;
    }
    return 0;
}""",
    "THETAZ_WSACleanup": """int WINAPI_CALL THETAZ_WSACleanup() {
    return 0;
}""",
    "THETAZ_WSAGetLastError": """int WINAPI_CALL THETAZ_WSAGetLastError() {
    return g_ws_last_error;
}""",
    "THETAZ_WSASetLastError": """void WINAPI_CALL THETAZ_WSASetLastError(int iError) {
    g_ws_last_error = iError;
}""",
    "THETAZ_WSAStartup": """int WINAPI_CALL THETAZ_WSAStartup(uint16_t wVersionRequested, void* lpWSAData) {
    (void)wVersionRequested;
    if (!lpWSAData) return 10022; // WSAEINVAL
    WSAData* d = static_cast<WSAData*>(lpWSAData);
    memset(d, 0, sizeof(WSAData));
    d->wVersion = 0x0202;
    d->wHighVersion = 0x0202;
    strcpy(d->szDescription, "ThetaZ Winsock 2.0");
    strcpy(d->szSystemStatus, "Running");
    d->iMaxSockets = 1024;
    d->iMaxUdpDg = 65535;
    g_ws_last_error = 0;
    return 0;
}""",
    "THETAZ_WaitForThreadpoolWorkCallbacks": """void WINAPI_CALL THETAZ_WaitForThreadpoolWorkCallbacks(void* w,BOOL c){}""",
    "THETAZ_WaitNamedPipeA": """BOOL WINAPI_CALL THETAZ_WaitNamedPipeA(const char* n,DWORD t){return FALSE;}""",
    "THETAZ_WaitNamedPipeW": """BOOL WINAPI_CALL THETAZ_WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeout){return FALSE;}""",
    "THETAZ_WerRegisterFile": """HRESULT WINAPI_CALL THETAZ_WerRegisterFile(const wchar_t* f,int t,DWORD fl){return 0;}""",
    "THETAZ_WerRegisterRuntimeExceptionModule": """HRESULT WINAPI_CALL THETAZ_WerRegisterRuntimeExceptionModule(const wchar_t* n,void* c){return 0;}""",
    "THETAZ_WerUnregisterRuntimeExceptionModule": """HRESULT WINAPI_CALL THETAZ_WerUnregisterRuntimeExceptionModule(const wchar_t* n){return 0;}""",
    "THETAZ_WinHttpCloseHandle": """BOOL WINAPI_CALL THETAZ_WinHttpCloseHandle(void* hInternet) {
    (void)hInternet;
    return 1;
}""",
    "THETAZ_WinHttpGetIEProxyConfigForCurrentUser": """BOOL WINAPI_CALL THETAZ_WinHttpGetIEProxyConfigForCurrentUser(void* pProxyConfig) {
    (void)pProxyConfig;
    return 0;
}""",
    "THETAZ_WinHttpQueryDataAvailable": """BOOL WINAPI_CALL THETAZ_WinHttpQueryDataAvailable(void* hRequest, DWORD* lpdwNumberOfBytesAvailable) {
    (void)hRequest;
    if (lpdwNumberOfBytesAvailable) *lpdwNumberOfBytesAvailable = 0;
    return 1;
}""",
    "THETAZ_WinHttpReadData": """BOOL WINAPI_CALL THETAZ_WinHttpReadData(void* hRequest, void* lpBuffer, DWORD dwNumberOfBytesToRead, DWORD* lpdwNumberOfBytesRead) {
    (void)hRequest; (void)lpBuffer; (void)dwNumberOfBytesToRead;
    if (lpdwNumberOfBytesRead) *lpdwNumberOfBytesRead = 0;
    return 1;
}""",
    "THETAZ_WinHttpReceiveResponse": """BOOL WINAPI_CALL THETAZ_WinHttpReceiveResponse(void* hRequest, void* lpReserved) {
    (void)hRequest; (void)lpReserved;
    return 1;
}""",
    "THETAZ_WinHttpSendRequest": """BOOL WINAPI_CALL THETAZ_WinHttpSendRequest(void* hRequest, void* pHeaders, DWORD dwHeadersLength,
    void* lpOptional, DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext) {
    (void)hRequest; (void)pHeaders; (void)dwHeadersLength;
    (void)lpOptional; (void)dwOptionalLength; (void)dwTotalLength; (void)dwContext;
    return 1;
}""",
    "THETAZ_WinHttpSetCredentials": """BOOL WINAPI_CALL THETAZ_WinHttpSetCredentials(void* hRequest, LPCWSTR AuthScheme, LPCWSTR pwszUserName,
    LPCWSTR pwszPassword, void* pAuthParams) {
    (void)hRequest; (void)AuthScheme; (void)pwszUserName; (void)pwszPassword; (void)pAuthParams;
    return 1;
}""",
    "THETAZ_WritePrivateProfileStringA": """BOOL WINAPI_CALL THETAZ_WritePrivateProfileStringA(const char* s,const char* k,const char* v,const char* f){return TRUE;}""",
    "THETAZ_WritePrivateProfileStringW": """BOOL WINAPI_CALL THETAZ_WritePrivateProfileStringW(const wchar_t* s,const wchar_t* k,const wchar_t* v,const wchar_t* f){return TRUE;}""",
    "THETAZ__lclose": """BOOL WINAPI_CALL THETAZ__lclose(int fd){return close(fd)==0;}""",
    "THETAZ__llseek": """LONG WINAPI_CALL THETAZ__llseek(int fd,LONG off,int origin){return lseek(fd,off,origin);}""",
    "THETAZ__lread": """UINT WINAPI_CALL THETAZ__lread(int fd,void* buf,UINT cnt){return (UINT)read(fd,buf,cnt);}""",
    "THETAZ_bind": """int WINAPI_CALL THETAZ_bind(uintptr_t s, void* name, int namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::bind(fd, ws_sockaddr(name), static_cast<socklen_t>(namelen));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}""",
    "THETAZ_closesocket": """int WINAPI_CALL THETAZ_closesocket(uintptr_t s) {
    if (!wsock_remove(s)) { g_ws_last_error = 10038; return 1; } // SOCKET_ERROR
    g_ws_last_error = 0;
    return 0;
}""",
    "THETAZ_connect": """int WINAPI_CALL THETAZ_connect(uintptr_t s, void* name, int namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::connect(fd, ws_sockaddr(name), static_cast<socklen_t>(namelen));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}""",
    "THETAZ_fgets": """int WINAPI_CALL THETAZ_fgets(char* buf,int n,void* stream){
    (void)buf;(void)n;(void)stream;
    return 0;
}""",
    "THETAZ_fputs": """int WINAPI_CALL THETAZ_fputs(const char* s,void* stream){
    (void)s;(void)stream;
    return 0;
}""",
    "THETAZ_freeaddrinfo": """void WINAPI_CALL THETAZ_freeaddrinfo(void* pAddrInfo){
    (void)pAddrInfo;
}""",
    "THETAZ_getaddrinfo": """int WINAPI_CALL THETAZ_getaddrinfo(const char* node,const char* service,const void* hints,void** res){
    (void)node;(void)service;(void)hints;(void)res;
    return 0;
}""",
    "THETAZ_gethostname": """int WINAPI_CALL THETAZ_gethostname(char* name, int namelen) {
    int r = ::gethostname(name, namelen);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}""",
    "THETAZ_getpeername": """int WINAPI_CALL THETAZ_getpeername(uintptr_t s, void* name, int* namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t len = namelen ? static_cast<socklen_t>(*namelen) : 0;
    int r = ::getpeername(fd, name ? ws_sockaddr(name) : nullptr, &len);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (namelen) *namelen = static_cast<int>(len);
    return 0;
}""",
    "THETAZ_getsockname": """int WINAPI_CALL THETAZ_getsockname(uintptr_t s, void* name, int* namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t len = namelen ? static_cast<socklen_t>(*namelen) : 0;
    int r = ::getsockname(fd, name ? ws_sockaddr(name) : nullptr, &len);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (namelen) *namelen = static_cast<int>(len);
    return 0;
}""",
    "THETAZ_getsockopt": """int WINAPI_CALL THETAZ_getsockopt(uintptr_t s, int level, int optname, char* optval, int* optlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t len = optlen ? static_cast<socklen_t>(*optlen) : 0;
    int r = ::getsockopt(fd, level, optname, optval, &len);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (optlen) *optlen = static_cast<int>(len);
    return 0;
}""",
    "THETAZ_ioctlsocket": """int WINAPI_CALL THETAZ_ioctlsocket(uintptr_t s, long cmd, unsigned long* argp) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::fcntl(fd, F_GETFL);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (cmd == 0x8004667E) { // FIONBIO
        int fl = *argp ? (r | O_NONBLOCK) : (r & ~O_NONBLOCK);
        if (::fcntl(fd, F_SETFL, fl) < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    }
    return 0;
}""",
    "THETAZ_isprint": """int WINAPI_CALL THETAZ_isprint(int c) { return isprint(c); }""",
    "THETAZ_iswalnum": """int WINAPI_CALL THETAZ_iswalnum(int c) { return iswalnum(c); }""",
    "THETAZ_listen": """int WINAPI_CALL THETAZ_listen(uintptr_t s, int backlog) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::listen(fd, backlog);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}""",
    "THETAZ_lstrcmpA": """int WINAPI_CALL THETAZ_lstrcmpA(const char* a,const char* b){return strcmp(a?a:"",b?b:"");}""",
    "THETAZ_lstrcmpiA": """int WINAPI_CALL THETAZ_lstrcmpiA(const char* a,const char* b){return strcasecmp(a?a:"",b?b:"");}""",
    "THETAZ_lstrlenA": """int WINAPI_CALL THETAZ_lstrlenA(const char* s){return s?(int)strlen(s):0;}""",
    "THETAZ_qsort": """void WINAPI_CALL THETAZ_qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (base && nmemb > 0 && size > 0 && compar) {
        ::qsort(base, nmemb, size, compar);
    }
}""",
    "THETAZ_recv": """int WINAPI_CALL THETAZ_recv(uintptr_t s, char* buf, int len, int flags) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = static_cast<int>(::recv(fd, buf, len, flags));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return r;
}""",
    "THETAZ_recvfrom": """int WINAPI_CALL THETAZ_recvfrom(uintptr_t s, char* buf, int len, int flags, void* from, int* fromlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t fl = fromlen ? static_cast<socklen_t>(*fromlen) : 0;
    int r = static_cast<int>(::recvfrom(fd, buf, len, flags, from ? ws_sockaddr(from) : nullptr, from ? &fl : nullptr));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (fromlen) *fromlen = static_cast<int>(fl);
    return r;
}""",
    "THETAZ_select": """int WINAPI_CALL THETAZ_select(int nfds, void* readfds, void* writefds, void* exceptfds, void* timeout) {
    (void)nfds;
    // Windows fd_set (x86): { DWORD fd_count; SOCKET fd_array[64]; } — 260
    // bytes. Convert the guest sets to host fd_sets (glibc layout differs),
    // call select, then rewrite the guest sets with the ready sockets.
    fd_set h_read, h_write, h_except;
    FD_ZERO(&h_read); FD_ZERO(&h_write); FD_ZERO(&h_except);
    int max_fd = -1;
    auto fill = [&](void* fds, fd_set& out) {
        if (!fds) return;
        uint32_t* g = static_cast<uint32_t*>(fds);
        uint32_t count = g[0];
        if (count > 64) count = 64;
        for (uint32_t i = 0; i < count; i++) {
            int fd = wsock_fd(g[1 + i]);
            if (fd >= 0) { FD_SET(fd, &out); if (fd > max_fd) max_fd = fd; }
        }
    };
    fill(readfds, h_read);
    fill(writefds, h_write);
    fill(exceptfds, h_except);

    timeval* tv = nullptr;
    timeval tv_buf;
    if (timeout) {
        // Windows TIMEVAL: { LONG tv_sec; LONG tv_usec; } — same layout.
        tv_buf.tv_sec = static_cast<long>(static_cast<const int32_t*>(timeout)[0]);
        tv_buf.tv_usec = static_cast<long>(static_cast<const int32_t*>(timeout)[1]);
        tv = &tv_buf;
    }

    int r = ::select(max_fd + 1, readfds ? &h_read : nullptr,
                     writefds ? &h_write : nullptr, exceptfds ? &h_except : nullptr, tv);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; } // SOCKET_ERROR

    // Rewrite the guest sets: keep only the sockets that are ready.
    auto writeback = [&](void* fds, fd_set& in) {
        if (!fds) return;
        uint32_t* g = static_cast<uint32_t*>(fds);
        uint32_t count = g[0];
        if (count > 64) count = 64;
        uint32_t n = 0;
        for (uint32_t i = 0; i < count; i++) {
            int fd = wsock_fd(g[1 + i]);
            if (fd >= 0 && FD_ISSET(fd, &in)) g[1 + n++] = g[1 + i];
        }
        g[0] = n;
    };
    writeback(readfds, h_read);
    writeback(writefds, h_write);
    writeback(exceptfds, h_except);
    return r;
}""",
    "THETAZ_send": """int WINAPI_CALL THETAZ_send(uintptr_t s, const char* buf, int len, int flags) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = static_cast<int>(::send(fd, buf, len, flags));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return r;
}""",
    "THETAZ_sendto": """int WINAPI_CALL THETAZ_sendto(uintptr_t s, const char* buf, int len, int flags, void* to, int tolen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = static_cast<int>(::sendto(fd, buf, len, flags, to ? ws_sockaddr(to) : nullptr, static_cast<socklen_t>(tolen)));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return r;
}""",
    "THETAZ_setsockopt": """int WINAPI_CALL THETAZ_setsockopt(uintptr_t s, int level, int optname, const char* optval, int optlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::setsockopt(fd, level, optname, optval, static_cast<socklen_t>(optlen));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}""",
    "THETAZ_shutdown": """int WINAPI_CALL THETAZ_shutdown(uintptr_t s, int how) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::shutdown(fd, how);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}""",
    "THETAZ_timeBeginPeriod": """DWORD WINAPI_CALL THETAZ_timeBeginPeriod(DWORD p){return 0;}""",
    "THETAZ_timeEndPeriod": """DWORD WINAPI_CALL THETAZ_timeEndPeriod(DWORD p){return 0;}""",
    "THETAZ_timeGetDevCaps": """DWORD WINAPI_CALL THETAZ_timeGetDevCaps(void* c,DWORD s){if(c&&s>=8)memset(c,0,s);return 0;}""",
    "THETAZ_timeGetSystemTime": """DWORD WINAPI_CALL THETAZ_timeGetSystemTime(void* t,DWORD s){return 0;}""",
    "THETAZ_timeGetTime": """DWORD WINAPI_CALL THETAZ_timeGetTime(){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (DWORD)(ts.tv_sec*1000+ts.tv_nsec/1000000);}""",
    "THETAZ_waveOutGetNumDevs": """DWORD WINAPI_CALL THETAZ_waveOutGetNumDevs() {
    return 0;
}""",
    "THETAZ_wcsncmp": """int WINAPI_CALL THETAZ_wcsncmp(const uint16_t* a, const uint16_t* b, size_t n) {
    return wstrncmp(a, b, (int)n);
}""",
    "THETAZ_wsprintfW": """int WINAPI_CALL THETAZ_wsprintfW(uint16_t* buf, const uint16_t* fmt, ...) {
    if (buf) buf[0] = 0;
    return 0;
}""",
}
