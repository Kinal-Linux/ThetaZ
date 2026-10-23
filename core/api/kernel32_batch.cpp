// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.
// Self-contained batch: ~300 kernel32/kernelbase/shlwapi stub implementations.

#include "kernel32.h"
#include "api.h"
#include "core/api/threadpool.h"
#include "core/loader/loader.h"
#include "core/ntdll/thread.h"
#include "core/ipc/thetaserver.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>
#include <pthread.h>
#include <cwctype>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

extern "C" {
#include <iconv.h>
}

// --- Missing types needed by the implementations ---
#ifndef S_FALSE
#define S_FALSE 0x00000001L
#endif
using HGLOBAL = void*;
using errno_t = int;
using HLOCAL  = void*;
using LANGID  = uint16_t;
using HFILE   = int;
#define HFILE_ERROR (-1)
#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif

// --- C library compatibility for wchar_t helpers ---
static inline wchar_t towlower_compat(wchar_t c) { return (wchar_t)::towlower((wint_t)c); }

// =========================================================================
namespace ThetaZ {
using S = size_t;

std::string wine_to_host(const char* path) {
    if (!path) return "";
    const char* prefix = getenv("WINEPREFIX");
    std::string root = prefix ? prefix : "/home/eronic/Prefixes/ThetaZ";
    std::string p = path;
    if (p.size() >= 2 && p[1] == ':') {
        p = root + "/drive_c/" + p.substr(2);
    } else if (p.compare(0, 2, "\\\\") == 0 || p.compare(0, 2, "//") == 0) {
        p = root + "/drive_c/" + p.substr(2);
    }
    for (size_t i = 0; i < p.size(); i++) {
        if (p[i] == '\\') p[i] = '/';
    }
    return p;
}

// =========================================================================
// kernel32: Version
// =========================================================================
DWORD WINAPI_CALL THETAZ_GetVersion() { return 0x80000006; }
BOOL WINAPI_CALL THETAZ_GetVersionExA(void* info) {
    if (!info) return FALSE;
    uint32_t* p = (uint32_t*)info;
    p[0]=148; p[1]=6; p[2]=1; p[3]=7601; p[4]=2;
    memset(&p[5],0,128);
    return TRUE;
}

// =========================================================================
// kernel32: Logical drives / directories
// =========================================================================
DWORD WINAPI_CALL THETAZ_GetLogicalDrives() { return 0x03; }
UINT WINAPI_CALL THETAZ_GetSystemDirectoryA(char* buf, UINT size) {
    const char* d="C:\\Windows\\System32"; size_t l=strlen(d);
    if(buf&&size>l) strcpy(buf,d); return (UINT)l;
}
UINT WINAPI_CALL THETAZ_GetSystemDirectoryW(uint16_t* buf, UINT size) {
    // 32-bit WOW64 guests report the syswow64 directory, matching the
    // GetSystemDirectoryW behaviour Office's bootstrap relies on to locate
    // its own DLLs. The emulated prefix has no such directories; report the
    // emulated C:\Windows\SysWOW64 so the returned length is consistent.
    const wchar_t* d=L"C:\\Windows\\SysWOW64"; size_t l=19;
    if(buf&&size>l){for(size_t i=0;i<=l;i++)buf[i]=d[i];}
    // Buffer too small (or NULL query): return the required length so the
    // caller retries with a larger buffer, as Windows documents.
    else if(buf&&size<=l) return (UINT)l+1;
    return (UINT)l;
}
UINT WINAPI_CALL THETAZ_GetWindowsDirectoryA(char* buf, UINT size) {
    const char* d="C:\\Windows"; size_t l=strlen(d);
    if(buf&&size>l) strcpy(buf,d); return (UINT)l;
}
UINT WINAPI_CALL THETAZ_GetWindowsDirectoryW(uint16_t* buf, UINT size) {
    const wchar_t* d=L"C:\\Windows"; size_t l=9;
    if(buf&&size>l){for(size_t i=0;i<=l;i++)buf[i]=d[i];} return (UINT)l;
}

// =========================================================================
// kernel32: Toolhelp32
// =========================================================================
struct TH32MOD { uint64_t base; uint32_t sz; wchar_t name[256]; };
struct TH32PROC { DWORD pid; DWORD ppid; wchar_t name[260]; };
static std::vector<TH32MOD> g_t32mods;
static std::vector<TH32PROC> g_t32procs;
struct TH32SNAPDATA { DWORD flags; size_t mi; size_t pi; };
static std::unordered_map<void*,TH32SNAPDATA> g_t32snap;

HANDLE WINAPI_CALL THETAZ_CreateToolhelp32Snapshot(DWORD flags, DWORD pid) {
    auto* snap=new TH32SNAPDATA{flags,0,0};
    if(flags&(0x8|0x10)){
        g_t32mods.clear();
        auto& mods=PELoader::instance().get_modules();
        for(auto& [base,m]:mods){TH32MOD mod{};mod.base=m.base;mod.sz=(uint32_t)m.size;
        for(size_t i=0;i<255&&m.name[i];i++)mod.name[i]=(wchar_t)m.name[i];g_t32mods.push_back(mod);}
    }
    if(flags&0x2){g_t32procs.clear();TH32PROC p{};p.pid=(DWORD)getpid();wcscpy(p.name,L"thetaz.exe");g_t32procs.push_back(p);}
    void* h=(void*)(uintptr_t)(0x10000+(uintptr_t)snap);
    g_t32snap[h]=*snap;
    return h;
}

struct MODULEENTRY32W{DWORD dwSize;DWORD th32ModuleID;DWORD th32ProcessID;DWORD GlblcntUsage;DWORD ProccntUsage;
    uint64_t modBaseAddr;DWORD modBaseSize;HMODULE hModule;wchar_t szModule[256];wchar_t szExePath[260];};
struct PROCESSENTRY32W{DWORD dwSize;DWORD cntUsage;DWORD th32ProcessID;DWORD th32DefaultHeapID;DWORD th32ModuleID;
    DWORD cntThreads;DWORD th32ParentProcessID;LONG pcPriClassBase;DWORD dwFlags;wchar_t szExeFile[260];};
struct THREADENTRY32{DWORD dwSize;DWORD cntUsage;DWORD th32ThreadID;DWORD th32OwnerProcessID;LONG tpBasePri;LONG tpDeltaPri;DWORD dwFlags;};

BOOL WINAPI_CALL THETAZ_Module32FirstW(HANDLE snap, MODULEENTRY32W* me) {
    auto it=g_t32snap.find(snap);if(it==g_t32snap.end()||g_t32mods.empty())return FALSE;
    me->dwSize=sizeof(MODULEENTRY32W);me->modBaseAddr=g_t32mods[0].base;me->modBaseSize=g_t32mods[0].sz;
    memcpy(me->szModule,g_t32mods[0].name,sizeof(me->szModule));memcpy(me->szExePath,g_t32mods[0].name,sizeof(me->szExePath));
    it->second.mi=1;return TRUE;
}
BOOL WINAPI_CALL THETAZ_Module32NextW(HANDLE snap, MODULEENTRY32W* me) {
    auto it=g_t32snap.find(snap);if(it==g_t32snap.end())return FALSE;
    size_t i=it->second.mi;if(i>=g_t32mods.size())return FALSE;
    me->dwSize=sizeof(MODULEENTRY32W);me->modBaseAddr=g_t32mods[i].base;me->modBaseSize=g_t32mods[i].sz;
    memcpy(me->szModule,g_t32mods[i].name,sizeof(me->szModule));memcpy(me->szExePath,g_t32mods[i].name,sizeof(me->szExePath));
    it->second.mi=i+1;return TRUE;
}
BOOL WINAPI_CALL THETAZ_Process32FirstW(HANDLE snap, PROCESSENTRY32W* pe) {
    auto it=g_t32snap.find(snap);if(it==g_t32snap.end()||g_t32procs.empty())return FALSE;
    pe->dwSize=sizeof(PROCESSENTRY32W);pe->th32ProcessID=g_t32procs[0].pid;pe->cntThreads=1;wcscpy(pe->szExeFile,g_t32procs[0].name);
    it->second.pi=1;return TRUE;
}
BOOL WINAPI_CALL THETAZ_Process32NextW(HANDLE snap, PROCESSENTRY32W* pe) {
    auto it=g_t32snap.find(snap);if(it==g_t32snap.end())return FALSE;
    size_t i=it->second.pi;if(i>=g_t32procs.size())return FALSE;
    pe->dwSize=sizeof(PROCESSENTRY32W);pe->th32ProcessID=g_t32procs[i].pid;pe->cntThreads=1;wcscpy(pe->szExeFile,g_t32procs[i].name);
    it->second.pi=i+1;return TRUE;
}
BOOL WINAPI_CALL THETAZ_Thread32First(HANDLE snap, THREADENTRY32* te){te->dwSize=sizeof(THREADENTRY32);return FALSE;}
BOOL WINAPI_CALL THETAZ_Thread32Next(HANDLE snap, THREADENTRY32* te){return FALSE;}

// =========================================================================
// kernel32: Atoms
// =========================================================================
ATOM WINAPI_CALL THETAZ_AddAtomA(const char* n){return (ATOM)(uintptr_t)(n+0x10000);}
ATOM WINAPI_CALL THETAZ_FindAtomA(const char* n){return (ATOM)(uintptr_t)(n+0x10000);}
ATOM WINAPI_CALL THETAZ_FindAtomW(const wchar_t* n){return (ATOM)(uintptr_t)((const char*)n+0x10000);}
UINT WINAPI_CALL THETAZ_GetAtomNameA(ATOM a,char* b,int s){if(b&&s>0)b[0]=0;return 0;}
UINT WINAPI_CALL THETAZ_GetAtomNameW(ATOM a,wchar_t* b,int s){if(b&&s>0)b[0]=0;return 0;}
ATOM WINAPI_CALL THETAZ_GlobalFindAtomA(const char* n){return (ATOM)(uintptr_t)(n+0x20000);}
ATOM WINAPI_CALL THETAZ_GlobalFindAtomW(const wchar_t* n){return (ATOM)(uintptr_t)((const char*)n+0x20000);}
UINT WINAPI_CALL THETAZ_GlobalGetAtomNameW(ATOM a,wchar_t* b,int s){if(b&&s>0)b[0]=0;return 0;}

// =========================================================================
// kernel32: Global/Local
// =========================================================================
LPVOID WINAPI_CALL THETAZ_GlobalLock(HGLOBAL h){return (LPVOID)h;}
BOOL WINAPI_CALL THETAZ_GlobalUnlock(HGLOBAL h){return TRUE;}
HGLOBAL WINAPI_CALL THETAZ_GlobalHandle(const void* p){return (HGLOBAL)p;}
DWORD WINAPI_CALL THETAZ_GlobalFlags(HGLOBAL h){return 0;}
LPVOID WINAPI_CALL THETAZ_LocalLock(HLOCAL h){return (LPVOID)h;}
BOOL WINAPI_CALL THETAZ__lclose(int fd){return close(fd)==0;}
LONG WINAPI_CALL THETAZ__llseek(int fd,LONG off,int origin){return lseek(fd,off,origin);}
UINT WINAPI_CALL THETAZ__lread(int fd,void* buf,UINT cnt){return (UINT)read(fd,buf,cnt);}

// =========================================================================
// kernel32: Module handle ext
// =========================================================================
BOOL WINAPI_CALL THETAZ_GetModuleHandleExA(DWORD flags,const char* name,HMODULE* ph) {
    if(!ph){THETAZ_SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    // GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS: name is an address, not a string.
    if (flags & 0x4) {
        return THETAZ_GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(name), ph);
    }
    std::u16string wide;
    if (name) {
        wide.reserve(strlen(name));
        for (const char* p = name; *p; ++p) {
            wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(*p)));
        }
    }
    return THETAZ_GetModuleHandleExW(
        flags, name ? reinterpret_cast<LPCWSTR>(wide.c_str()) : nullptr, ph);
}

// =========================================================================
// kernel32: Process/thread info
// =========================================================================
DWORD WINAPI_CALL THETAZ_GetProcessId(HANDLE p){return (DWORD)(uintptr_t)p;}
BOOL WINAPI_CALL THETAZ_SetPriorityClass(HANDLE p,DWORD c){return TRUE;}
BOOL WINAPI_CALL THETAZ_SetProcessAffinityMask(HANDLE p,DWORD_PTR m){return TRUE;}
DWORD WINAPI_CALL THETAZ_GetErrorMode(){return 0;}
DWORD WINAPI_CALL THETAZ_SetThreadErrorMode(DWORD m,DWORD* o){if(o)*o=0;return TRUE;}

// =========================================================================
// kernel32: File
// =========================================================================
BOOL WINAPI_CALL THETAZ_SetFileAttributesA(const char* n,DWORD a){return TRUE;}
DWORD WINAPI_CALL THETAZ_SearchPathA(const char* d,const char* f,const char* e,DWORD l,char* b,char** p){
    if(!b||l==0)return 0;if(d)snprintf(b,l,"%s\\%s",d,f);else snprintf(b,l,"%s",f);if(p)*p=b;return (DWORD)strlen(b);
}
DWORD WINAPI_CALL THETAZ_SearchPathW(const wchar_t* d,const wchar_t* f,const wchar_t* e,DWORD l,wchar_t* b,wchar_t** p){
    if(!b||l==0)return 0;if(d)swprintf((wchar_t*)b,l,L"%ls\\%ls",d,f);else swprintf((wchar_t*)b,l,L"%ls",f);if(p)*p=b;return (DWORD)wcslen(b);
}
HANDLE WINAPI_CALL THETAZ_OpenFileMappingW(DWORD a,BOOL i,const wchar_t* n){return NULL;}
LPVOID WINAPI_CALL THETAZ_MapViewOfFileEx(HANDLE m,DWORD a,DWORD oH,DWORD oL,S size,LPVOID addr){return addr;}
BOOL WINAPI_CALL THETAZ_MoveFileW(const wchar_t* f,const wchar_t* t){
    char fa[260],ta[260];
    wcstombs(fa,f,sizeof(fa));wcstombs(ta,t,sizeof(ta));
    return rename(wine_to_host(fa).c_str(),wine_to_host(ta).c_str())==0;
}
BOOL WINAPI_CALL THETAZ_CreateHardLinkW(const wchar_t* l,const wchar_t* t,void* s){return FALSE;}
DWORD WINAPI_CALL THETAZ_GetShortPathNameW(const wchar_t* l,wchar_t* s,DWORD n){if(l&&s&&n>0)wcsncpy(s,l,n);return l?(DWORD)wcslen(l):0;}
DWORD WINAPI_CALL THETAZ_CopyFile2(const wchar_t* f,const wchar_t* t,void* p){return 0x80070070;}
DWORD WINAPI_CALL THETAZ_GetTempFileNameA(const char* pa,const char* px,UINT u,char* b,UINT l){
    if(b&&l>0)snprintf(b,l,"%s\\%s%04x.tmp",pa?".":"",px?px:"tmp",u?u:(unsigned)getpid()&0xFFFF);return 0;
}
BOOL WINAPI_CALL THETAZ_ReplaceFileW(const wchar_t* o,const wchar_t* r,const wchar_t* b,DWORD f,void* p,void* v){return TRUE;}

// =========================================================================
// kernel32: Named pipes
// =========================================================================
BOOL WINAPI_CALL THETAZ_CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                          DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes){return FALSE;}
BOOL WINAPI_CALL THETAZ_ConnectNamedPipe(HANDLE p,void* o){return FALSE;}
BOOL WINAPI_CALL THETAZ_DisconnectNamedPipe(HANDLE p){return TRUE;}
BOOL WINAPI_CALL THETAZ_SetNamedPipeHandleState(HANDLE p,DWORD* m,DWORD* c,DWORD* d){return FALSE;}
BOOL WINAPI_CALL THETAZ_GetNamedPipeInfo(HANDLE p,DWORD* f,DWORD* o,DWORD* i,DWORD* m){if(f)*f=0;if(o)*o=4096;if(i)*i=4096;if(m)*m=1;return TRUE;}
BOOL WINAPI_CALL THETAZ_WaitNamedPipeA(const char* n,DWORD t){return FALSE;}
BOOL WINAPI_CALL THETAZ_WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeout){return FALSE;}
BOOL WINAPI_CALL THETAZ_GetNamedPipeClientProcessId(HANDLE p,uint64_t* pid){if(pid)*pid=0;return FALSE;}

// =========================================================================
// kernel32: Semaphore
// =========================================================================
HANDLE WINAPI_CALL THETAZ_CreateSemaphoreW(void* sa, LONG i, LONG m, const wchar_t* n) {
    (void)sa;
    std::string name;
    if (n) {
        size_t len = wcslen(n);
        name.resize(len * 4 + 1);
        size_t j = 0;
        for (size_t k = 0; k < len; k++) {
            wchar_t c = n[k];
            if (c < 0x80) name[j++] = (char)c;
            else if (c < 0x800) { name[j++] = (char)(0xC0 | (c >> 6)); name[j++] = (char)(0x80 | (c & 0x3F)); }
            else { name[j++] = (char)(0xE0 | (c >> 12)); name[j++] = (char)(0x80 | ((c >> 6) & 0x3F)); name[j++] = (char)(0x80 | (c & 0x3F)); }
        }
        name.resize(j);
    }
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    uint64_t h = ThetaServer::instance().create_semaphore(name, (int)i, (int)m, pid);
    return (HANDLE)(uintptr_t)h;
}
HANDLE WINAPI_CALL THETAZ_OpenSemaphoreW(DWORD a,BOOL i,const wchar_t* n){return (HANDLE)(uintptr_t)0x20001;}

// =========================================================================
// kernel32: Sleep/Signal
// =========================================================================
BOOL WINAPI_CALL THETAZ_SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds){
    if(CriticalSection){THETAZ_LeaveCriticalSection(CriticalSection);usleep(dwMilliseconds*1000);THETAZ_EnterCriticalSection(CriticalSection);}return TRUE;
}
DWORD WINAPI_CALL THETAZ_SignalObjectAndWait(HANDLE s,HANDLE w,DWORD ms,BOOL a){usleep(ms*1000);return 258;}
BOOL WINAPI_CALL THETAZ_SleepEx(DWORD ms,BOOL a){usleep(ms*1000);return FALSE;}

// =========================================================================
// kernel32: Debug/Thread/Job
// =========================================================================
void WINAPI_CALL THETAZ_DebugBreak(){}
BOOL WINAPI_CALL THETAZ_TerminateThread(HANDLE t,DWORD c){return TRUE;}
HANDLE WINAPI_CALL THETAZ_CreateJobObjectW(void* s, const wchar_t* n) {
    (void)s; (void)n;
    THETAZ_SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}
BOOL WINAPI_CALL THETAZ_AssignProcessToJobObject(HANDLE j,HANDLE p){return TRUE;}
BOOL WINAPI_CALL THETAZ_SetInformationJobObject(HANDLE j,int c,void* i,DWORD s){return TRUE;}
BOOL WINAPI_CALL THETAZ_TerminateJobObject(HANDLE j,UINT c){return TRUE;}
BOOL WINAPI_CALL THETAZ_ReadProcessMemory(HANDLE p,const void* a,void* b,S sz,S* r){memcpy(b,a,sz);if(r)*r=sz;return TRUE;}

// =========================================================================
// kernel32: Console/Misc
// =========================================================================
HWND WINAPI_CALL THETAZ_GetConsoleWindow(){return (HWND)(uintptr_t)0x40000;}
BOOL WINAPI_CALL THETAZ_GetProcessMitigationPolicy(HANDLE p,int c,void* b,S s){memset(b,0,s);return TRUE;}
BOOL WINAPI_CALL THETAZ_FlushInstructionCache(HANDLE p,const void* a,S s){return TRUE;}
BOOL WINAPI_CALL THETAZ_FlushProcessWriteBuffers(){return TRUE;}
BOOL WINAPI_CALL THETAZ_HeapQueryInformation(HANDLE h,int c,void* b,S l,S* r){if(b&&l>=8)memset(b,0,l);if(r)*r=0;return TRUE;}
BOOL WINAPI_CALL THETAZ_GetThreadContext(HANDLE t,void* c){if(c)memset(c,0,716);return TRUE;}

// =========================================================================
// kernel32: Locale/NLS
// =========================================================================
LCID WINAPI_CALL THETAZ_ConvertDefaultLocale(LCID l){return l;}
LANGID WINAPI_CALL THETAZ_GetSystemDefaultLangID(){return 0x0409;}
LANGID WINAPI_CALL THETAZ_GetUserDefaultLangID(){return 0x0409;}
LANGID WINAPI_CALL THETAZ_GetSystemDefaultUILanguage(){return 0x0409;}
LANGID WINAPI_CALL THETAZ_GetUserDefaultUILanguage(){return 0x0409;}
LANGID WINAPI_CALL THETAZ_GetThreadUILanguage(){return 0x0409;}
BOOL WINAPI_CALL THETAZ_SetThreadPreferredUILanguages(DWORD f,const wchar_t* l,DWORD* o){if(o)*o=0;return TRUE;}
DWORD WINAPI_CALL THETAZ_GetCurrentProcessorNumber(){return 0;}
BOOL WINAPI_CALL THETAZ_GetNumaHighestNodeNumber(ULONG* n){if(n)*n=0;return TRUE;}
BOOL WINAPI_CALL THETAZ_SetThreadGroupAffinity(HANDLE t,const void* a,void* p){return TRUE;}
BOOL WINAPI_CALL THETAZ_GetThreadGroupAffinity(HANDLE t,void* a){if(a)memset(a,0,10);return TRUE;}
BOOL WINAPI_CALL THETAZ_GetLogicalProcessorInformationEx(int r,void* b,DWORD* l){if(l)*l=0;return TRUE;}
DWORD WINAPI_CALL THETAZ_GetSystemFirmwareTable(DWORD s,DWORD i,void* b,DWORD sz){return 0;}
BOOL WINAPI_CALL THETAZ_GetSystemTimes(FILETIME* idle,FILETIME* kernel,FILETIME* user){
    if(idle){idle->dwLowDateTime=idle->dwHighDateTime=0;}if(kernel){kernel->dwLowDateTime=kernel->dwHighDateTime=0;}
    if(user){user->dwLowDateTime=user->dwHighDateTime=0;}return TRUE;
}

// =========================================================================
// kernel32: NLS/IDN/Date/Time/Calendar
// =========================================================================
BOOL WINAPI_CALL THETAZ_IsDBCSLeadByteEx(UINT cp,BYTE l){return FALSE;}
int WINAPI_CALL THETAZ_IdnToAscii(DWORD f,const wchar_t* in,int il,wchar_t* out,int ol){
    if(out&&in&&il>=0&&ol>=il){memcpy(out,in,il*2);return il;}return il;
}
int WINAPI_CALL THETAZ_IdnToUnicode(DWORD f,const wchar_t* in,int il,wchar_t* out,int ol){
    if(out&&in&&il>=0&&ol>=il){memcpy(out,in,il*2);return il;}return il;
}
int WINAPI_CALL THETAZ_GetDateFormatA(LCID l,DWORD f,const SYSTEMTIME* s,const char* fmt,char* b,int bl){
    if(b&&bl>0){strcpy(b,"01/01/2024");return 10;}return 10;
}
int WINAPI_CALL THETAZ_GetTimeFormatA(LCID l,DWORD f,const SYSTEMTIME* s,const char* fmt,char* b,int bl){
    if(b&&bl>0){strcpy(b,"00:00:00");return 8;}return 8;
}
int WINAPI_CALL THETAZ_GetCalendarInfoW(LCID l,CALID c,CALINFO o,wchar_t* b,int bl,DWORD* v){
    if(b&&bl>0)wcscpy(b,L"Gregorian");if(v)*v=0;return 0;
}
int WINAPI_CALL THETAZ_GetCalendarInfoEx(const wchar_t* n,LCID l,const SYSTEMTIME* s,CALINFO o,wchar_t* b,int bl,DWORD* v){
    if(b&&bl>0)wcscpy(b,L"Gregorian");if(v)*v=0;return 0;
}
int WINAPI_CALL THETAZ_GetCurrencyFormatEx(const wchar_t* l,DWORD f,const wchar_t* v,const void* fmt,wchar_t* b,int bl){
    if(b&&bl>0){wcscpy(b,v?v:L"0");return (int)wcslen(b);}return 0;
}
int WINAPI_CALL THETAZ_GetNumberFormatEx(const wchar_t* l,DWORD f,const wchar_t* v,const void* fmt,wchar_t* b,int bl){
    if(b&&bl>0){wcscpy(b,v?v:L"0");return (int)wcslen(b);}return 0;
}
int WINAPI_CALL THETAZ_GetGeoInfoW(int g,int t,wchar_t* b,int bl,LID la){if(b&&bl>0){wcscpy(b,L"US");return 2;}return 0;}
BOOL WINAPI_CALL THETAZ_EnumCalendarInfoA(void* p,LCID l,CALID c,CALINFO i,DWORD f){return FALSE;}
BOOL WINAPI_CALL THETAZ_EnumCalendarInfoExEx(void* p,const wchar_t* f,LCID l,CALID c,DWORD i,DWORD fl,int64_t lp){return FALSE;}
BOOL WINAPI_CALL THETAZ_EnumDateFormatsExEx(void* p,const wchar_t* l,DWORD f,int64_t lp){return FALSE;}
BOOL WINAPI_CALL THETAZ_EnumTimeFormatsEx(void* p,const wchar_t* l,DWORD f,int64_t lp){return FALSE;}
BOOL WINAPI_CALL THETAZ_EnumSystemLanguageGroups(void* p,DWORD f,int64_t lp){return FALSE;}
BOOL WINAPI_CALL THETAZ_EnumUILanguagesW(void* p,DWORD f,int64_t lp){return FALSE;}
int WINAPI_CALL THETAZ_FindNLSStringEx(const wchar_t* l,DWORD f,const wchar_t* s1,int l1,const wchar_t* s2,int l2,int* u,void* c,void* r,int so){return -1;}
int WINAPI_CALL THETAZ_FoldStringW(DWORD f,const wchar_t* s,int sl,wchar_t* d,int dl){
    if(d&&s&&dl>=sl){memcpy(d,s,sl*2);return sl;}return sl;
}
int WINAPI_CALL THETAZ_NormalizeString(int f,const wchar_t* s,int sl,wchar_t* d,int dl){
    if(d&&s&&dl>=sl){memcpy(d,s,sl*2);return sl;}return sl;
}
int WINAPI_CALL THETAZ_ResolveLocaleName(const wchar_t* n,wchar_t* b,int bl){if(b&&bl>0)wcscpy(b,L"en-US");return 5;}

// =========================================================================
// kernel32: Timezone / FileTime
// =========================================================================
DWORD WINAPI_CALL THETAZ_GetDynamicTimeZoneInformation(void* t){if(t)memset(t,0,144);return 0;}
BOOL WINAPI_CALL THETAZ_SystemTimeToTzSpecificLocalTime(void* tz,SYSTEMTIME* s,SYSTEMTIME* l){if(l&&s)*l=*s;return TRUE;}
BOOL WINAPI_CALL THETAZ_FileTimeToDosDateTime(const FILETIME* f,WORD* d,WORD* t){if(d)*d=0;if(t)*t=0;return TRUE;}
BOOL WINAPI_CALL THETAZ_DosDateTimeToFileTime(WORD d,WORD t,FILETIME* f){if(f){f->dwLowDateTime=f->dwHighDateTime=0;}return TRUE;}
BOOL WINAPI_CALL THETAZ_FileTimeToLocalFileTime(const FILETIME* f,FILETIME* l){if(l&&f)*l=*f;return TRUE;}
BOOL WINAPI_CALL THETAZ_LocalFileTimeToFileTime(const FILETIME* l,FILETIME* f){if(f&&l)*f=*l;return TRUE;}

// =========================================================================
// kernel32: LZ compression
// =========================================================================
void WINAPI_CALL THETAZ_LZClose(HFILE h){}
LONG WINAPI_CALL THETAZ_LZCopy(HFILE s,HFILE d){return 0;}
HFILE WINAPI_CALL THETAZ_LZOpenFileW(const char* n,void* i,WORD a){return HFILE_ERROR;}
LONG WINAPI_CALL THETAZ_LZSeek(HFILE h,LONG o,int origin){return 0;}

// =========================================================================
// kernel32: String comparison
// =========================================================================
int WINAPI_CALL THETAZ_lstrcmpA(const char* a,const char* b){return strcmp(a?a:"",b?b:"");}
int WINAPI_CALL THETAZ_lstrcmpiA(const char* a,const char* b){return strcasecmp(a?a:"",b?b:"");}
int WINAPI_CALL THETAZ_lstrlenA(const char* s){return s?(int)strlen(s):0;}

// =========================================================================
// kernel32: Private profile (INI)
// =========================================================================
int WINAPI_CALL THETAZ_GetPrivateProfileIntW(const wchar_t* s,const wchar_t* k,int d,const wchar_t* f){return d;}
DWORD WINAPI_CALL THETAZ_GetPrivateProfileStringA(const char* s,const char* k,const char* d,char* b,DWORD l,const char* f){
    if(b&&l>0&&d){strncpy(b,d,l);b[l-1]=0;}return d?(DWORD)strlen(d):0;
}
DWORD WINAPI_CALL THETAZ_GetPrivateProfileStringW(const wchar_t* s,const wchar_t* k,const wchar_t* d,wchar_t* b,DWORD l,const wchar_t* f){
    if(b&&l>0&&d){wcsncpy(b,d,l);b[l-1]=0;}return d?(DWORD)wcslen(d):0;
}
BOOL WINAPI_CALL THETAZ_WritePrivateProfileStringA(const char* s,const char* k,const char* v,const char* f){return TRUE;}
BOOL WINAPI_CALL THETAZ_WritePrivateProfileStringW(const wchar_t* s,const wchar_t* k,const wchar_t* v,const wchar_t* f){return TRUE;}

// =========================================================================
// kernel32: Threadpool
// =========================================================================
static TP_POOL g_tp{};static TP_CG g_tpcg{};static TP_IO g_tpio{};
TP_POOL* WINAPI_CALL THETAZ_CreateThreadpool(void* r){return &g_tp;}
TP_CG* WINAPI_CALL THETAZ_CreateThreadpoolCleanupGroup(){return &g_tpcg;}
TP_IO* WINAPI_CALL THETAZ_CreateThreadpoolIo(HANDLE f,void* c,void* x,void* e){g_tpio.handle=f;return &g_tpio;}
void WINAPI_CALL THETAZ_CloseThreadpoolIo(void* i){}
void WINAPI_CALL THETAZ_StartThreadpoolIo(void* i){}
void WINAPI_CALL THETAZ_CancelThreadpoolIo(void* i){}

// =========================================================================
// kernel32: ProcThreadAttr
// =========================================================================

// =========================================================================
// kernel32: Misc
// =========================================================================
BOOL WINAPI_CALL THETAZ_GetEnabledXStateFeatures(){return 0;}
BOOL WINAPI_CALL THETAZ_BindIoCompletionCallback(HANDLE f,void* c,DWORD flags){return FALSE;}
void WINAPI_CALL THETAZ_RaiseFailFastException(void* e,void* c,DWORD f){fprintf(stderr,"[kernel32] RaiseFailFastException\n");abort();}
ULONG WINAPI_CALL THETAZ_InterlockedFlushSList(void* l){return 0;}
void WINAPI_CALL THETAZ_ApplicationRecoveryFinished(BOOL s){}
HRESULT WINAPI_CALL THETAZ_ApplicationRecoveryInProgress(BOOL* c){if(c)*c=FALSE;return 0;}
HRESULT WINAPI_CALL THETAZ_RegisterApplicationRecoveryCallback(void* p,void* pa,DWORD pg,DWORD f){return 0;}
HRESULT WINAPI_CALL THETAZ_RegisterApplicationRestart(const wchar_t* c,DWORD f){return 0;}
HRESULT WINAPI_CALL THETAZ_GetCurrentPackageFullName(uint32_t* l,wchar_t* n){if(l)*l=0;return 0x80073D54;}
HRESULT WINAPI_CALL THETAZ_GetCurrentPackagePath(uint32_t* l,wchar_t* p){if(l)*l=0;return 0x80073D54;}
HRESULT WINAPI_CALL THETAZ_GetPackageFamilyName(uint64_t* n,uint32_t* l,wchar_t* b){if(l)*l=0;return 0x80073D54;}
HRESULT WINAPI_CALL THETAZ_GetPackagePathByFullName(const wchar_t* n,uint32_t* l,wchar_t* p){if(l)*l=0;return 0x80073D54;}
HRESULT WINAPI_CALL THETAZ_FindPackagesByPackageFamily(const wchar_t* f,uint32_t* c,wchar_t** b,uint32_t* bl,wchar_t* b2){if(c)*c=0;return 0x80073D54;}
HRESULT WINAPI_CALL THETAZ_WerRegisterFile(const wchar_t* f,int t,DWORD fl){return 0;}
HRESULT WINAPI_CALL THETAZ_WerRegisterRuntimeExceptionModule(const wchar_t* n,void* c){return 0;}
HRESULT WINAPI_CALL THETAZ_WerUnregisterRuntimeExceptionModule(const wchar_t* n){return 0;}
BOOL WINAPI_CALL THETAZ_Wow64DisableWow64FsRedirection(void* o){return TRUE;}
BOOL WINAPI_CALL THETAZ_Wow64RevertWow64FsRedirection(void* o){return TRUE;}
BOOL WINAPI_CALL THETAZ_IsWow64Process2(HANDLE p,USHORT* m,USHORT* n){if(m)*m=0;if(n)*n=0x8664;return TRUE;}
BOOL WINAPI_CALL THETAZ_K32EnumProcessModules(HANDLE p,void* m,DWORD cb,DWORD* n){if(n)*n=0;return TRUE;}
DWORD WINAPI_CALL THETAZ_K32GetModuleBaseNameA(HANDLE p,void* m,char* n,DWORD s){if(n&&s>0)strcpy(n,"thetaz.exe");return 10;}
DWORD WINAPI_CALL THETAZ_K32GetModuleBaseNameW(HANDLE p,void* m,wchar_t* n,DWORD s){if(n&&s>0)wcscpy(n,L"thetaz.exe");return 10;}
BOOL WINAPI_CALL THETAZ_K32GetModuleInformation(HANDLE p,void* m,void* i,DWORD cb){return FALSE;}
void WINAPI_CALL THETAZ_RtlAddFunctionTable(void* t,DWORD c,uint64_t b){}
void WINAPI_CALL THETAZ_RtlDeleteFunctionTable(void* t){}
void WINAPI_CALL THETAZ_RtlRestoreContext(void* c,void* e){}

// =========================================================================
// kernel32: Multimedia timer
// =========================================================================
DWORD WINAPI_CALL THETAZ_timeBeginPeriod(DWORD p){return 0;}
DWORD WINAPI_CALL THETAZ_timeEndPeriod(DWORD p){return 0;}
DWORD WINAPI_CALL THETAZ_timeGetDevCaps(void* c,DWORD s){if(c&&s>=8)memset(c,0,s);return 0;}
DWORD WINAPI_CALL THETAZ_timeGetSystemTime(void* t,DWORD s){return 0;}
DWORD WINAPI_CALL THETAZ_timeGetTime(){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (DWORD)(ts.tv_sec*1000+ts.tv_nsec/1000000);}

// =========================================================================
// kernel32: ActCtx
// =========================================================================
HANDLE WINAPI_CALL THETAZ_CreateActCtxW(void* i) {
    (void)i;
    void* p = alloc_guest_heap(64);
    if (!p) { THETAZ_SetLastError(ERROR_NOT_ENOUGH_MEMORY); return (HANDLE)(intptr_t)-1; }
    memset(p, 0, 64);
    return (HANDLE)p;
}
BOOL WINAPI_CALL THETAZ_ActivateActCtx(HANDLE c,ULONG_PTR* k){if(k)*k=1;return TRUE;}
BOOL WINAPI_CALL THETAZ_DeactivateActCtx(DWORD f,ULONG_PTR k){return TRUE;}
void WINAPI_CALL THETAZ_ReleaseActCtx(HANDLE c){}
BOOL WINAPI_CALL THETAZ_GetCurrentActCtx(HANDLE* c){if(c)*c=NULL;return FALSE;}
BOOL WINAPI_CALL THETAZ_FindActCtxSectionStringW(DWORD f,const GUID* g,ULONG s,const wchar_t* str,void* r){return FALSE;}
BOOL WINAPI_CALL THETAZ_QueryActCtxW(DWORD f,HANDLE c,void* a,DWORD i,void* b,S sz,S* r){if(r)*r=0;return FALSE;}

// =========================================================================
// kernelbase: Char
// =========================================================================
char* WINAPI_CALL THETAZ_CharLowerA(char* s){
    if(!s)return NULL;if((uintptr_t)s<=0xFFFF)return(char*)(uintptr_t)tolower((int)(uintptr_t)s);
    for(char* p=s;*p;p++)*p=(char)tolower((unsigned char)*p);return s;
}
wchar_t* WINAPI_CALL THETAZ_CharLowerW(wchar_t* s){
    if(!s)return NULL;if((uintptr_t)s<=0xFFFF)return(wchar_t*)(uintptr_t)towlower_compat((wint_t)(uintptr_t)s);
    for(wchar_t* p=s;*p;p++)*p=(wchar_t)towlower_compat((wint_t)*p);return s;
}
BOOL WINAPI_CALL THETAZ_ChrCmpIA(wchar_t a,wchar_t b){return towlower_compat(a)!=towlower_compat(b);}
BOOL WINAPI_CALL THETAZ_ChrCmpIW(wchar_t a,wchar_t b){return towlower_compat(a)!=towlower_compat(b);}

// =========================================================================
// kernelbase: IsChar
// =========================================================================
BOOL WINAPI_CALL THETAZ_IsCharBlankW(wchar_t c){return c==L' '||c==L'\t';}
BOOL WINAPI_CALL THETAZ_IsCharCntrlW(wchar_t c){return c<32||c==127;}
BOOL WINAPI_CALL THETAZ_IsCharDigitW(wchar_t c){return c>=L'0'&&c<=L'9';}
BOOL WINAPI_CALL THETAZ_IsCharPunctW(wchar_t c){return ispunct(c);}
BOOL WINAPI_CALL THETAZ_IsCharSpaceW(wchar_t c){return c==L' '||(c>=L'\t'&&c<=L'\r');}
BOOL WINAPI_CALL THETAZ_IsCharSpaceA(char c){return c==' '||(c>='\t'&&c<='\r');}
BOOL WINAPI_CALL THETAZ_IsCharXDigitW(wchar_t c){return isxdigit(c);}
BOOL WINAPI_CALL THETAZ_IsInternetESCEnabled(){return FALSE;}

// =========================================================================
// kernelbase: Security (minimal)
// =========================================================================
BOOL WINAPI_CALL THETAZ_GetAce(void* a,DWORD i,void* ace){return FALSE;}
BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorControl(void* sd,WORD* c,DWORD* r){if(c)*c=0x8004;if(r)*r=1;return TRUE;}
BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorGroup(void* sd,void* g,BOOL* d){return FALSE;}
BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorOwner(void* sd,void* o,BOOL* d){return FALSE;}
BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorSacl(void* sd,BOOL* p,void* a,BOOL* d){if(p)*p=FALSE;return TRUE;}
BOOL WINAPI_CALL THETAZ_InitializeSecurityDescriptor(void* sd,DWORD r){if(sd)memset(sd,0,64);return TRUE;}
BOOL WINAPI_CALL THETAZ_IsValidAcl(void* a){return TRUE;}

// =========================================================================
// kernelbase: Hash/Version
// =========================================================================
HRESULT WINAPI_CALL THETAZ_HashData(const BYTE* d,DWORD dl,BYTE* h,DWORD hl,DWORD a,DWORD s){if(h&&hl>0)memset(h,0,hl);return S_OK;}
DWORD WINAPI_CALL THETAZ_GetFileVersionInfoSizeW(const wchar_t* n,DWORD* h){if(h)*h=0;return 0;}
BOOL WINAPI_CALL THETAZ_GetFileVersionInfoW(const wchar_t* n,DWORD h,DWORD sz,void* d){return FALSE;}
HRESULT WINAPI_CALL THETAZ_GetAcceptLanguagesA(char* b,DWORD* l){if(b&&l&&*l>0){strncpy(b,"en-US",*l);*l=6;}return S_OK;}
HRESULT WINAPI_CALL THETAZ_GetAcceptLanguagesW(wchar_t* b,DWORD* l){if(b&&l&&*l>0){wcsncpy(b,L"en-US",*l);*l=6;}return S_OK;}

// =========================================================================
// kernelbase: URL/QI/SHLoad
// =========================================================================
HRESULT WINAPI_CALL THETAZ_ParseURLA(const char* u,void* p){return E_NOTIMPL;}
HRESULT WINAPI_CALL THETAZ_ParseURLW(const wchar_t* u,void* p){return E_NOTIMPL;}
HRESULT WINAPI_CALL THETAZ_QISearch(void* t,void* q,REFIID iid,void** ppv){return E_NOINTERFACE;}
HRESULT WINAPI_CALL THETAZ_SHLoadIndirectString(const wchar_t* s,wchar_t* b,uint32_t l,void** r){
    if(s&&b&&l>0){wcsncpy(b,s,l);return S_OK;}return E_FAIL;
}

// =========================================================================
// kernelbase: SHReg
// =========================================================================
LSTATUS WINAPI_CALL THETAZ_SHRegCreateUSKeyA(const char* k,DWORD s,void* e,void** h,DWORD d){if(h)*h=(void*)1;return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegCreateUSKeyW(const wchar_t* k,DWORD s,void* e,void** h,DWORD d){char a[512];wcstombs(a,k,sizeof(a));return THETAZ_SHRegCreateUSKeyA(a,s,e,h,d);}
LSTATUS WINAPI_CALL THETAZ_SHRegOpenUSKeyA(const char* k,DWORD s,void* e,void** h,DWORD d){return THETAZ_SHRegCreateUSKeyA(k,s,e,h,d);}
LSTATUS WINAPI_CALL THETAZ_SHRegOpenUSKeyW(const wchar_t* k,DWORD s,void* e,void** h,DWORD d){char a[512];wcstombs(a,k,sizeof(a));return THETAZ_SHRegCreateUSKeyA(a,s,e,h,d);}
LSTATUS WINAPI_CALL THETAZ_SHRegQueryUSValueA(void* h,const char* n,DWORD* t,void* d,DWORD* cb,BOOL i,const void* df,DWORD dc,BOOL f){if(cb)*cb=0;return 2;}
LSTATUS WINAPI_CALL THETAZ_SHRegQueryUSValueW(void* h,const wchar_t* n,DWORD* t,void* d,DWORD* cb,BOOL i,const void* df,DWORD dc,BOOL f){if(cb)*cb=0;return 2;}
LSTATUS WINAPI_CALL THETAZ_SHRegSetUSValueA(void* h,const char* n,DWORD t,const void* d,DWORD cb,DWORD f){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegSetUSValueW(void* h,const wchar_t* n,DWORD t,const void* d,DWORD cb,DWORD f){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegWriteUSValueA(void* h,const char* n,DWORD t,const void* d,DWORD cb,DWORD f){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegWriteUSValueW(void* h,const wchar_t* n,DWORD t,const void* d,DWORD cb,DWORD f){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegDeleteUSValueA(void* h,const char* n,DWORD t){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegDeleteUSValueW(void* h,const wchar_t* n,DWORD t){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegDeleteEmptyUSKeyA(void* h,const char* s){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegDeleteEmptyUSKeyW(void* h,const wchar_t* s){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegEnumUSKeyA(void* h,DWORD i,char* n,DWORD* nl){return 259;}
LSTATUS WINAPI_CALL THETAZ_SHRegEnumUSKeyW(void* h,DWORD i,wchar_t* n,DWORD* nl){return 259;}
LSTATUS WINAPI_CALL THETAZ_SHRegEnumUSValueA(void* h,DWORD i,char* n,DWORD* nl,DWORD* t,void* d,DWORD* dc){return 259;}
LSTATUS WINAPI_CALL THETAZ_SHRegEnumUSValueW(void* h,DWORD i,wchar_t* n,DWORD* nl,DWORD* t,void* d,DWORD* dc){return 259;}
BOOL WINAPI_CALL THETAZ_SHRegGetBoolUSValueA(const char* k,const char* n,BOOL i,BOOL d){return d;}
BOOL WINAPI_CALL THETAZ_SHRegGetBoolUSValueW(const wchar_t* k,const wchar_t* n,BOOL i,BOOL d){return d;}
LSTATUS WINAPI_CALL THETAZ_SHRegGetUSValueA(const char* k,const char* n,DWORD* t,void* d,DWORD* cb,BOOL i,const void* df,DWORD dc){if(cb)*cb=0;return 2;}
LSTATUS WINAPI_CALL THETAZ_SHRegGetUSValueW(const wchar_t* k,const wchar_t* n,DWORD* t,void* d,DWORD* cb,BOOL i,const void* df,DWORD dc){if(cb)*cb=0;return 2;}
LSTATUS WINAPI_CALL THETAZ_SHRegQueryInfoUSKeyA(void* h,DWORD* sk,DWORD* ms,DWORD* v,DWORD* mv,DWORD* cm,DWORD* dc){return 0;}
LSTATUS WINAPI_CALL THETAZ_SHRegQueryInfoUSKeyW(void* h,DWORD* sk,DWORD* ms,DWORD* v,DWORD* mv,DWORD* cm,DWORD* dc){return 0;}
void WINAPI_CALL THETAZ_SHRegCloseUSKey(void* h){}
void WINAPI_CALL THETAZ_SHTruncateString(wchar_t* s,int ml){if(s&&ml>=0&&(int)wcslen(s)>ml)s[ml]=0;}

// =========================================================================
// kernelbase: Str*
// =========================================================================
char* WINAPI_CALL THETAZ_StrChrA(const char* s,WORD ch){return(char*)strchr(s,ch);}
wchar_t* WINAPI_CALL THETAZ_StrChrW(const wchar_t* s,WORD ch){return(wchar_t*)wcschr(s,ch);}
wchar_t* WINAPI_CALL THETAZ_StrChrNW(const wchar_t* s,WORD ch,UINT mx){const wchar_t* e=s+mx;while(s<e&&*s){if(*s==(wchar_t)ch)return(wchar_t*)s;s++;}return NULL;}
char* WINAPI_CALL THETAZ_StrChrIA(const char* s,WORD ch){for(;*s;s++)if(tolower((unsigned char)*s)==tolower((unsigned char)ch))return(char*)s;return NULL;}
wchar_t* WINAPI_CALL THETAZ_StrChrIW(const wchar_t* s,WORD ch){for(;*s;s++)if(towlower_compat(*s)==towlower_compat((wchar_t)ch))return(wchar_t*)s;return NULL;}
int WINAPI_CALL THETAZ_StrCmpA(const char* a,const char* b){return strcmp(a?a:"",b?b:"");}
int WINAPI_CALL THETAZ_StrCmpW(const wchar_t* a,const wchar_t* b){return wcscmp(a?a:L"",b?b:L"");}
int WINAPI_CALL THETAZ_StrCmpCA(const char* a,const char* b){return strcmp(a?a:"",b?b:"");}
int WINAPI_CALL THETAZ_StrCmpCW(const wchar_t* a,const wchar_t* b){return wcscmp(a?a:L"",b?b:L"");}
int WINAPI_CALL THETAZ_StrCmpICA(const char* a,const char* b){return strcasecmp(a?a:"",b?b:"");}
int WINAPI_CALL THETAZ_StrCmpICW(const wchar_t* a,const wchar_t* b){return wcscasecmp(a?a:L"",b?b:L"");}
int WINAPI_CALL THETAZ_StrCmpNA(const char* a,const char* b,int n){return strncmp(a?a:"",b?b:"",n);}
int WINAPI_CALL THETAZ_StrCmpNW(const wchar_t* a,const wchar_t* b,int n){return wcsncmp(a?a:L"",b?b:L"",n);}
int WINAPI_CALL THETAZ_StrCmpNCA(const char* a,const char* b,int n){return strncasecmp(a?a:"",b?b:"",n);}
int WINAPI_CALL THETAZ_StrCmpNCW(const wchar_t* a,const wchar_t* b,int n){return wcsncasecmp(a?a:L"",b?b:L"",n);}
int WINAPI_CALL THETAZ_StrCmpNIA(const char* a,const char* b,int n){return strncasecmp(a?a:"",b?b:"",n);}
int WINAPI_CALL THETAZ_StrCmpNICW(const wchar_t* a,const wchar_t* b,int n){return wcsncasecmp(a?a:L"",b?b:L"",n);}
int WINAPI_CALL THETAZ_StrCmpLogicalW(const wchar_t* a,const wchar_t* b){return wcscmp(a?a:L"",b?b:L"");}
wchar_t* WINAPI_CALL THETAZ_StrCpyNW(wchar_t* d,const wchar_t* s,int n){if(d&&s&&n>0){wcsncpy(d,s,n);d[n-1]=0;}return d;}
wchar_t* WINAPI_CALL THETAZ_StrCpyNXA(char* d,const char* s,int n){if(d&&s&&n>0){strncpy(d,s,n);d[n-1]=0;}return (wchar_t*)d;}
wchar_t* WINAPI_CALL THETAZ_StrCpyNXW(wchar_t* d,const wchar_t* s,int n){if(d&&s&&n>0){wcsncpy(d,s,n);d[n-1]=0;}return d;}
int WINAPI_CALL THETAZ_StrCSpnA(const char* s,const char* se){return(int)(strcspn(s,se));}
int WINAPI_CALL THETAZ_StrCSpnW(const wchar_t* s,const wchar_t* se){return(int)(wcscspn(s,se));}
int WINAPI_CALL THETAZ_StrCSpnIA(const char* s,const char* se){return(int)(strcspn(s,se));}
int WINAPI_CALL THETAZ_StrCSpnIW(const wchar_t* s,const wchar_t* se){return(int)(wcscspn(s,se));}
char* WINAPI_CALL THETAZ_StrDupA(const char* s){return s?strdup(s):NULL;}
wchar_t* WINAPI_CALL THETAZ_StrDupW(const wchar_t* s){return s?wcsdup(s):NULL;}
BOOL WINAPI_CALL THETAZ_StrIsIntlEqualA(BOOL cs,const char* a,const char* b,int n){return strncasecmp(a?a:"",b?b:"",n)==0;}
BOOL WINAPI_CALL THETAZ_StrIsIntlEqualW(BOOL cs,const wchar_t* a,const wchar_t* b,int n){return wcsncasecmp(a?a:L"",b?b:L"",n)==0;}
char* WINAPI_CALL THETAZ_StrPBrkA(const char* s,const char* se){return(char*)strpbrk(s,se);}
wchar_t* WINAPI_CALL THETAZ_StrPBrkW(const wchar_t* s,const wchar_t* se){return(wchar_t*)wcspbrk(s,se);}
char* WINAPI_CALL THETAZ_StrRChrA(const char* s,int st,WORD ch){const char* la=NULL;for(const char* p=s;(st<0||p<s+st)&&*p;p++)if(*p==(char)ch)la=p;return(char*)la;}
wchar_t* WINAPI_CALL THETAZ_StrRChrW(const wchar_t* s,int st,WORD ch){const wchar_t* la=NULL;for(const wchar_t* p=s;(st<0||p<s+st)&&*p;p++)if(*p==(wchar_t)ch)la=p;return(wchar_t*)la;}
char* WINAPI_CALL THETAZ_StrRChrIA(const char* s,int st,WORD ch){const char* la=NULL;for(const char* p=s;(st<0||p<s+st)&&*p;p++)if(tolower((unsigned char)*p)==tolower((unsigned char)ch))la=p;return(char*)la;}
wchar_t* WINAPI_CALL THETAZ_StrRChrIW(const wchar_t* s,int st,WORD ch){const wchar_t* la=NULL;for(const wchar_t* p=s;(st<0||p<s+st)&&*p;p++)if(towlower_compat(*p)==towlower_compat((wchar_t)ch))la=p;return(wchar_t*)la;}
char* WINAPI_CALL THETAZ_StrRStrIA(const char* s,const char* sub){if(!s||!sub||!*sub)return NULL;int sl=(int)strlen(s),sbl=(int)strlen(sub);for(int i=sl-sbl;i>=0;i--)if(strncasecmp(s+i,sub,sbl)==0)return(char*)(s+i);return NULL;}
wchar_t* WINAPI_CALL THETAZ_StrRStrIW(const wchar_t* s,const wchar_t* sub){if(!s||!sub||!*sub)return NULL;int sl=(int)wcslen(s),sbl=(int)wcslen(sub);for(int i=sl-sbl;i>=0;i--)if(wcsncasecmp(s+i,sub,sbl)==0)return(wchar_t*)(s+i);return NULL;}
int WINAPI_CALL THETAZ_StrSpnA(const char* s,const char* se){return(int)strspn(s,se);}
int WINAPI_CALL THETAZ_StrSpnW(const wchar_t* s,const wchar_t* se){return(int)wcscspn(s,se);}
char* WINAPI_CALL THETAZ_StrStrA(const char* s,const char* sub){return(char*)strstr(s,sub);}
wchar_t* WINAPI_CALL THETAZ_StrStrW(const wchar_t* s,const wchar_t* sub){return(wchar_t*)wcsstr(s,sub);}
char* WINAPI_CALL THETAZ_StrStrIA(const char* s,const char* sub){if(!s||!sub)return NULL;size_t sl=strlen(sub);for(const char* p=s;*p;p++)if(strncasecmp(p,sub,sl)==0)return(char*)p;return NULL;}
wchar_t* WINAPI_CALL THETAZ_StrStrIW(const wchar_t* s,const wchar_t* sub){if(!s||!sub)return NULL;size_t sl=wcslen(sub);for(const wchar_t* p=s;*p;p++)if(wcsncasecmp(p,sub,sl)==0)return(wchar_t*)p;return NULL;}
wchar_t* WINAPI_CALL THETAZ_StrStrNW(const wchar_t* s,const wchar_t* sub,int n){if(!s||!sub)return NULL;size_t sl=wcslen(sub);for(const wchar_t* p=s;p<s+n&&*p;p++)if(wcsncasecmp(p,sub,sl)==0)return(wchar_t*)p;return NULL;}
int WINAPI_CALL THETAZ_StrToIntA(const char* s){return atoi(s?s:"0");}
int WINAPI_CALL THETAZ_StrToIntW(const wchar_t* s){return (int)wcstol(s?s:L"0",nullptr,10);}
BOOL WINAPI_CALL THETAZ_StrToInt64ExA(const char* s,DWORD f,int64_t* v){if(v)*v=atoll(s?s:"0");return TRUE;}
BOOL WINAPI_CALL THETAZ_StrToInt64ExW(const wchar_t* s,DWORD f,int64_t* v){if(v)*v=wcstoll(s?s:L"0",nullptr,10);return TRUE;}
void WINAPI_CALL THETAZ_StrTrimA(char* s){if(!s)return;int l=(int)strlen(s);while(l>0&&(s[l-1]==' '||s[l-1]=='\t'))s[--l]=0;char* st=s;while(*st==' '||*st=='\t')st++;if(st!=s)memmove(s,st,strlen(st)+1);}
void WINAPI_CALL THETAZ_StrTrimW(wchar_t* s){if(!s)return;int l=(int)wcslen(s);while(l>0&&(s[l-1]==L' '||s[l-1]==L'\t'))s[--l]=0;wchar_t* st=s;while(*st==L' '||*st==L'\t')st++;if(st!=s)memmove(s,st,(wcslen(st)+1)*2);}
char* WINAPI_CALL THETAZ_StrCatBuffA(char* d,const char* s,int sz){if(d&&s)strncat(d,s,sz-strlen(d)-1);return d;}
wchar_t* WINAPI_CALL THETAZ_StrCatBuffW(wchar_t* d,const wchar_t* s,int sz){if(d&&s){size_t dl=wcslen(d);wcsncpy(d+dl,s,sz-dl-1);d[sz-1]=0;}return d;}
DWORD WINAPI_CALL THETAZ_StrCatChainW(wchar_t* d,DWORD mx,DWORD idx,const wchar_t* s){if(!d||!s)return 0;int sl=(int)wcslen(s);if((int)idx<mx-1){int cp=std::min(sl,(int)(mx-idx-1));memcpy(d+idx,s,cp*sizeof(wchar_t));d[idx+cp]=0;}return idx+sl;}

// =========================================================================
// shlwapi: Path*
// =========================================================================
wchar_t* WINAPI_CALL THETAZ_PathAddBackslashW(wchar_t* p){int l=(int)wcslen(p);if(l<259&&p[l-1]!=L'\\'){p[l]=L'\\';p[l+1]=0;}return p;}
char* WINAPI_CALL THETAZ_PathAddBackslashA(char* p){int l=(int)strlen(p);if(l<260&&p[l-1]!='\\'){p[l]='\\';p[l+1]=0;}return p;}
BOOL WINAPI_CALL THETAZ_PathAddExtensionW(wchar_t* p,const wchar_t* e){if(!p||!e)return FALSE;wchar_t* d=wcsrchr(p,L'.');if(d&&d>wcsrchr(p,L'\\'))return FALSE;wcscat(p,e);return TRUE;}
BOOL WINAPI_CALL THETAZ_PathAddExtensionA(char* p,const char* e){if(!p||!e)return FALSE;char* d=strrchr(p,'.');if(d&&d>strrchr(p,'\\'))return FALSE;strcat(p,e);return TRUE;}
BOOL WINAPI_CALL THETAZ_PathAppendW(wchar_t* b,const wchar_t* c){if(!b||!c)return FALSE;int l=(int)wcslen(b);if(l>0&&b[l-1]!=L'\\'&&b[l-1]!=L'/')wcscat(b,L"\\");wcscat(b,c);return TRUE;}
BOOL WINAPI_CALL THETAZ_PathAppendA(char* b,const char* c){if(!b||!c)return FALSE;int l=(int)strlen(b);if(l>0&&b[l-1]!='\\'&&b[l-1]!='/')strcat(b,"\\");strcat(b,c);return TRUE;}
BOOL WINAPI_CALL THETAZ_PathCanonicalizeW(wchar_t* b,const wchar_t* p){if(b&&p)wcscpy(b,p);return TRUE;}
BOOL WINAPI_CALL THETAZ_PathCanonicalizeA(char* b,const char* p){if(b&&p)strcpy(b,p);return TRUE;}
BOOL WINAPI_CALL THETAZ_PathCchAppend(wchar_t* p,size_t m,const wchar_t* e){return THETAZ_PathAppendW(p,e);}
HRESULT WINAPI_CALL THETAZ_PathCchFindExtension(const wchar_t* p,size_t m,const wchar_t** e){if(!p||!e)return E_INVALIDARG;const wchar_t* d=wcsrchr(p,L'.');*e=d?d:p+wcslen(p);return S_OK;}
wchar_t* WINAPI_CALL THETAZ_PathCombineW(wchar_t* d,const wchar_t* dir,const wchar_t* f){
    if(!d)return NULL;if(dir)wcscpy(d,dir);else d[0]=0;
    if(f){int l=(int)wcslen(d);if(l>0&&d[l-1]!=L'\\')wcscat(d,L"\\");wcscat(d,f);}return d;
}
char* WINAPI_CALL THETAZ_PathCombineA(char* d,const char* dir,const char* f){
    if(!d)return NULL;if(dir)strcpy(d,dir);else d[0]=0;
    if(f){int l=(int)strlen(d);if(l>0&&d[l-1]!='\\')strcat(d,"\\");strcat(d,f);}return d;
}
BOOL WINAPI_CALL THETAZ_PathCommonPrefixW(const wchar_t* a,const wchar_t* b,wchar_t* buf){if(buf)buf[0]=0;return FALSE;}
BOOL WINAPI_CALL THETAZ_PathCommonPrefixA(const char* a,const char* b,char* buf){if(buf)buf[0]=0;return FALSE;}
HRESULT WINAPI_CALL THETAZ_PathCreateFromUrlW(const wchar_t* u,wchar_t* p,uint32_t* l,DWORD f){if(u&&p&&l){wcsncpy(p,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_PathCreateFromUrlAlloc(const wchar_t* u,wchar_t** p,DWORD f){if(!u||!p)return E_FAIL;*p=(wchar_t*)malloc((wcslen(u)+1)*2);wcscpy(*p,u);return S_OK;}
HRESULT WINAPI_CALL THETAZ_PathCreateFromUrlA(const char* u,char* p,uint32_t* l,DWORD f){if(u&&p&&l){strncpy(p,u,*l);return S_OK;}return E_FAIL;}
BOOL WINAPI_CALL THETAZ_PathFileExistsW(const wchar_t* p){
    if(!p)return FALSE;char a[260];wcstombs(a,p,sizeof(a));struct stat st;return stat(wine_to_host(a).c_str(),&st)==0;
}
BOOL WINAPI_CALL THETAZ_PathFileExistsA(const char* p){
    if(!p)return FALSE;struct stat st;return stat(wine_to_host(p).c_str(),&st)==0;
}
wchar_t* WINAPI_CALL THETAZ_PathFindExtensionW(const wchar_t* p){
    if(!p)return(wchar_t*)L"";const wchar_t* sl=wcsrchr(p,L'\\');const wchar_t* sf=wcsrchr(p,L'/');const wchar_t* s=p;
    if(sl&&sl>s)s=sl+1;if(sf&&sf>s)s=sf+1;const wchar_t* d=wcsrchr(s,L'.');return d?(wchar_t*)d:(wchar_t*)(p+wcslen(p));
}
char* WINAPI_CALL THETAZ_PathFindExtensionA(const char* p){
    if(!p)return(char*)"";const char* sl=strrchr(p,'\\');const char* sf=strrchr(p,'/');const char* s=p;
    if(sl&&sl>s)s=sl+1;if(sf&&sf>s)s=sf+1;const char* d=strrchr(s,'.');return d?(char*)d:(char*)(p+strlen(p));
}
LPWSTR WINAPI_CALL THETAZ_PathFindFileNameW(LPCWSTR p){
    if(!p)return(LPWSTR)L"";uint16_t* sl=reinterpret_cast<uint16_t*>(const_cast<wchar_t*>(wcsrchr(reinterpret_cast<const wchar_t*>(p),L'\\')));uint16_t* sf=reinterpret_cast<uint16_t*>(const_cast<wchar_t*>(wcsrchr(reinterpret_cast<const wchar_t*>(p),L'/')));LPWSTR n=(LPWSTR)p;
    if(sl&&sl>n)n=sl+1;if(sf&&sf>n)n=sf+1;return n;
}
char* WINAPI_CALL THETAZ_PathFindFileNameA(const char* p){
    if(!p)return(char*)"";const char* sl=strrchr(p,'\\');const char* sf=strrchr(p,'/');const char* n=p;
    if(sl&&sl>n)n=sl+1;if(sf&&sf>n)n=sf+1;return(char*)n;
}
BOOL WINAPI_CALL THETAZ_PathSkipRootA(LPSTR p){
    if(!p)return FALSE;
    if(p[0]=='\\'&&p[1]=='\\'){char* s=p+2;while(*s&&*s!='\\')s++;if(*s)s++;while(*s&&*s!='\\')s++;strcpy(p,s);return TRUE;}
    if(p[1]==':'){strcpy(p,p+3);return TRUE;}
    return FALSE;
}
void WINAPI_CALL THETAZ_PathStripPathW(LPWSTR p){LPWSTR n=THETAZ_PathFindFileNameW(p);if(n!=p)wcscpy(reinterpret_cast<wchar_t*>(p),reinterpret_cast<const wchar_t*>(n));}
void WINAPI_CALL THETAZ_PathStripPathA(char* p){char* n=THETAZ_PathFindFileNameA(p);if(n!=p)strcpy(p,n);}
BOOL WINAPI_CALL THETAZ_PathStripToRootW(wchar_t* p){if(!p)return FALSE;if(p[1]==L':'){p[2]=L'\\';p[3]=0;return TRUE;}if(p[0]==L'\\'&&p[1]==L'\\'){p[2]=0;return TRUE;}return FALSE;}
BOOL WINAPI_CALL THETAZ_PathStripToRootA(char* p){if(!p)return FALSE;if(p[1]==':'){p[2]='\\';p[3]=0;return TRUE;}if(p[0]=='\\'&&p[1]=='\\'){p[2]=0;return TRUE;}return FALSE;}
BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsW(const wchar_t* p,wchar_t* b,uint32_t* l){if(b&&p&&l){wcsncpy(b,p,*l);return TRUE;}return FALSE;}
BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsA(const char* p,char* b,uint32_t* l){if(b&&p&&l){strncpy(b,p,*l);return TRUE;}return FALSE;}
void WINAPI_CALL THETAZ_PathUnquoteSpacesW(wchar_t* p){}
void WINAPI_CALL THETAZ_PathUnquoteSpacesA(char* p){}

// =========================================================================
// shlwapi: Url*
// =========================================================================
HRESULT WINAPI_CALL THETAZ_UrlApplySchemeW(const wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlApplySchemeA(const char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlCombineW(const wchar_t* ba,const wchar_t* re,wchar_t* co,uint32_t* l,DWORD f){if(co&&ba&&l){wcsncpy(co,ba,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlCombineA(const char* ba,const char* re,char* co,uint32_t* l,DWORD f){if(co&&ba&&l){strncpy(co,ba,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlCreateFromPathW(const wchar_t* p,wchar_t* u,uint32_t* l,DWORD f){if(u&&p&&l){wcsncpy(u,p,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlCreateFromPathA(const char* p,char* u,uint32_t* l,DWORD f){if(u&&p&&l){strncpy(u,p,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlEscapeW(const wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlEscapeA(const char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlFixupW(const wchar_t* u,wchar_t* b,uint32_t* l){return E_NOTIMPL;}
HRESULT WINAPI_CALL THETAZ_UrlFixupA(const char* u,char* b,uint32_t* l){return E_NOTIMPL;}
HRESULT WINAPI_CALL THETAZ_UrlGetPartW(const wchar_t* u,wchar_t* p,uint32_t* l,DWORD pi,DWORD f){if(p&&u&&l){wcsncpy(p,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlGetPartA(const char* u,char* p,uint32_t* l,DWORD pi,DWORD f){if(p&&u&&l){strncpy(p,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlGetSchemeW(const wchar_t* u,DWORD* s){if(s)*s=0;return S_OK;}
HRESULT WINAPI_CALL THETAZ_UrlGetSchemeA(const char* u,DWORD* s){if(s)*s=0;return S_OK;}
HRESULT WINAPI_CALL THETAZ_UrlHashW(const wchar_t* u,BYTE* h,DWORD l){if(h&&l>0)memset(h,0,l);return S_OK;}
HRESULT WINAPI_CALL THETAZ_UrlHashA(const char* u,BYTE* h,DWORD l){if(h&&l>0)memset(h,0,l);return S_OK;}
BOOL WINAPI_CALL THETAZ_UrlIsW(const wchar_t* u,DWORD f){return FALSE;}
BOOL WINAPI_CALL THETAZ_UrlIsA(const char* u,DWORD f){return FALSE;}
BOOL WINAPI_CALL THETAZ_UrlIsNoHistoryW(const wchar_t* u){return FALSE;}
BOOL WINAPI_CALL THETAZ_UrlIsNoHistoryA(const char* u){return FALSE;}
BOOL WINAPI_CALL THETAZ_UrlIsOpaqueW(const wchar_t* u){return FALSE;}
BOOL WINAPI_CALL THETAZ_UrlIsOpaqueA(const char* u){return FALSE;}
HRESULT WINAPI_CALL THETAZ_UrlUnescapeW(wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlUnescapeA(char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlUnescapeInPlaceW(wchar_t* u,DWORD f){return S_OK;}
HRESULT WINAPI_CALL THETAZ_UrlUnescapeInPlaceA(char* u,DWORD f){return S_OK;}

// --- Additional shlwapi A/W variants ---
void* WINAPI_CALL THETAZ_PathFindNextComponentA(const char* p){
    if(!p)return NULL;
    const char* s=p;
    while(*s&&*s!='\\'&&*s!='/')s++;
    return (void*)(uintptr_t)(s[0]?s+1:s);
}

wchar_t* WINAPI_CALL THETAZ_PathFindNextComponentW(const wchar_t* p){
    if(!p)return (wchar_t*)L"";
    const wchar_t* s=p;
    while(*s&&*s!=L'\\'&&*s!=L'/')s++;
    return (wchar_t*)(s[0]?s+1:s);
}

BOOL WINAPI_CALL THETAZ_PathSkipRootW(wchar_t* p){
    if(!p)return FALSE;
    if(p[0]==L'\\'&&p[1]==L'\\'){wchar_t* s=p+2;while(*s&&*s!=L'\\')s++;if(*s)s++;while(*s&&*s!=L'\\')s++;wcscpy(p,s);return TRUE;}
    if(p[1]==L':'){wcscpy(p,p+3);return TRUE;}
    return FALSE;
}

// --- Missing Path* A/W implementations ---
char* WINAPI_CALL THETAZ_PathGetArgsA(LPCSTR p){if(!p)return (char*)"";const char* s=strchr(p,' ');return s?(char*)s:(char*)p;}
wchar_t* WINAPI_CALL THETAZ_PathGetArgsW(LPCWSTR p){if(!p)return (wchar_t*)L"";const wchar_t* s=wcschr((const wchar_t*)p,L' ');return s?(wchar_t*)s:(wchar_t*)p;}
int WINAPI_CALL THETAZ_PathGetCharTypeA(LPCSTR p, DWORD* type){if(p&&type){*type=GCT_LFNCHAR;return TRUE;}return FALSE;}
int WINAPI_CALL THETAZ_PathGetCharTypeW(LPCWSTR p, DWORD* type){if(p&&type){*type=GCT_LFNCHAR;return TRUE;}return FALSE;}
int WINAPI_CALL THETAZ_PathGetDriveNumberA(LPCSTR p){if(p&&p[1]==':')return p[0]-'A';return -1;}
int WINAPI_CALL THETAZ_PathGetDriveNumberW(LPCWSTR p){if(p&&p[1]==L':')return p[0]-L'A';return -1;}
BOOL WINAPI_CALL THETAZ_PathIsFileSpecA(LPCSTR p){if(!p)return FALSE;return strchr(p,'\\')==NULL && strchr(p,'/')==NULL && strchr(p,':')==NULL;}
BOOL WINAPI_CALL THETAZ_PathIsFileSpecW(LPCWSTR p){if(!p)return FALSE;return wcschr((const wchar_t*)p,L'\\')==NULL && wcschr((const wchar_t*)p,L'/')==NULL && wcschr((const wchar_t*)p,L':')==NULL;}
BOOL WINAPI_CALL THETAZ_PathIsLFNFileSpecA(LPCSTR p){return THETAZ_PathIsFileSpecA(p);}
BOOL WINAPI_CALL THETAZ_PathIsLFNFileSpecW(LPCWSTR p){return THETAZ_PathIsFileSpecW(p);}
BOOL WINAPI_CALL THETAZ_PathIsPrefixA(LPCSTR p, LPCSTR prefix){if(!p||!prefix)return FALSE;return strncmp(p,prefix,strlen(prefix))==0;}
BOOL WINAPI_CALL THETAZ_PathIsPrefixW(LPCWSTR p, LPCWSTR prefix){if(!p||!prefix)return FALSE;return wcsncmp((const wchar_t*)p,(const wchar_t*)prefix,wcslen((const wchar_t*)prefix))==0;}
BOOL WINAPI_CALL THETAZ_PathIsRelativeA(LPCSTR p){if(!p)return FALSE;return p[1]!=':' && p[0]!='\\' && p[0]!='/';}
BOOL WINAPI_CALL THETAZ_PathIsRelativeW(LPCWSTR p){if(!p)return FALSE;return p[1]!=L':' && p[0]!=L'\\' && p[0]!=L'/';}
BOOL WINAPI_CALL THETAZ_PathIsRootA(LPCSTR p){if(!p)return FALSE;return (p[1]==':' && p[2]=='\\' && p[3]=='\0') || (p[0]=='\\' && p[1]=='\\' && p[2]=='\0');}
BOOL WINAPI_CALL THETAZ_PathIsRootW(LPCWSTR p){if(!p)return FALSE;return (p[1]==L':' && p[2]==L'\\' && p[3]==L'\0') || (p[0]==L'\\' && p[1]==L'\\' && p[2]==L'\0');}
BOOL WINAPI_CALL THETAZ_PathIsSameRootA(LPCSTR p1, LPCSTR p2){if(!p1||!p2)return FALSE;return (p1[0]==p2[0] && p1[1]==p2[1] && p1[2]==p2[2]);}
BOOL WINAPI_CALL THETAZ_PathIsSameRootW(LPCWSTR p1, LPCWSTR p2){if(!p1||!p2)return FALSE;return (p1[0]==p2[0] && p1[1]==p2[1] && p1[2]==p2[2]);}
BOOL WINAPI_CALL THETAZ_PathIsUNCA(LPCSTR p){if(!p)return FALSE;return p[0]=='\\' && p[1]=='\\';}
BOOL WINAPI_CALL THETAZ_PathIsUNCW(LPCWSTR p){if(!p)return FALSE;return p[0]==L'\\' && p[1]==L'\\';}
BOOL WINAPI_CALL THETAZ_PathIsUNCServerA(LPCSTR p){return FALSE;}
BOOL WINAPI_CALL THETAZ_PathIsUNCServerW(LPCWSTR p){return FALSE;}
BOOL WINAPI_CALL THETAZ_PathIsUNCServerShareA(LPCSTR p){return FALSE;}
BOOL WINAPI_CALL THETAZ_PathIsUNCServerShareW(LPCWSTR p){return FALSE;}
BOOL WINAPI_CALL THETAZ_PathIsURLA(LPCSTR p){if(!p)return FALSE;return strstr(p,"://")!=NULL;}
BOOL WINAPI_CALL THETAZ_PathIsURLW(LPCWSTR p){if(!p)return FALSE;return wcsstr((const wchar_t*)p,L"://")!=NULL;}
BOOL WINAPI_CALL THETAZ_PathIsValidCharA(LPCSTR p, int type){return TRUE;}
BOOL WINAPI_CALL THETAZ_PathIsValidCharW(LPCWSTR p, int type){return TRUE;}
BOOL WINAPI_CALL THETAZ_PathMatchSpecA(LPCSTR p, LPCSTR sp){return TRUE;}
BOOL WINAPI_CALL THETAZ_PathMatchSpecExA(LPCSTR p, LPCSTR sp, DWORD f, void* o){return TRUE;}
HRESULT WINAPI_CALL THETAZ_PathMatchSpecExW(LPCWSTR p, LPCWSTR sp, DWORD f, void* o){return S_OK;}
int WINAPI_CALL THETAZ_PathParseIconLocationA(LPSTR p){return 0;}
int WINAPI_CALL THETAZ_PathParseIconLocationW(LPCWSTR p){return 0;}
void WINAPI_CALL THETAZ_PathQuoteSpacesA(LPSTR p){}
void WINAPI_CALL THETAZ_PathQuoteSpacesW(LPCWSTR p){}
BOOL WINAPI_CALL THETAZ_PathRelativePathToA(LPSTR p, LPCSTR from, DWORD fa, LPCSTR to, DWORD fb){if(p)strcpy(p,".");return TRUE;}
BOOL WINAPI_CALL THETAZ_PathRelativePathToW(LPCWSTR p, LPCWSTR from, DWORD fa, LPCWSTR to, DWORD fb){if(p)wcscpy((wchar_t*)p,L".");return TRUE;}
void WINAPI_CALL THETAZ_PathRemoveBackslashA(LPSTR p){if(p){int l=strlen(p);if(l>0 && p[l-1]=='\\')p[l-1]='\0';}}
void WINAPI_CALL THETAZ_PathRemoveBackslashW(LPCWSTR p){if(p){int l=wcslen((wchar_t*)p);if(l>0 && ((wchar_t*)p)[l-1]==L'\\')((wchar_t*)p)[l-1]=L'\0';}}
void WINAPI_CALL THETAZ_PathRemoveBlanksA(LPSTR p){}
void WINAPI_CALL THETAZ_PathRemoveBlanksW(LPCWSTR p){}
void WINAPI_CALL THETAZ_PathRemoveFileSpecA(LPSTR p){if(p){char* s=strrchr(p,'\\');if(!s)s=strrchr(p,'/');if(s)*s='\0';}}
void WINAPI_CALL THETAZ_PathRemoveFileSpecW(LPCWSTR p){if(p){wchar_t* s=wcsrchr((wchar_t*)p,L'\\');if(!s)s=wcsrchr((wchar_t*)p,L'/');if(s)*s=L'\0';}}
BOOL WINAPI_CALL THETAZ_PathSearchAndQualifyA(LPCSTR p, LPSTR b, UINT* l){if(b&&p&&l){strncpy(b,p,*l);return TRUE;}return FALSE;}
BOOL WINAPI_CALL THETAZ_PathSearchAndQualifyW(LPCWSTR p, LPWSTR b, UINT* l){if(b&&p&&l){wcsncpy((wchar_t*)b,(const wchar_t*)p,*l);return TRUE;}return FALSE;}
int WINAPI_CALL THETAZ_PathParseIconLocationW(LPWSTR p){(void)p;return 0;}
void WINAPI_CALL THETAZ_PathQuoteSpacesW(LPWSTR p){(void)p;}
BOOL WINAPI_CALL THETAZ_PathRelativePathToW(LPWSTR p, LPCWSTR a, DWORD b, LPCWSTR c, DWORD d){(void)p;(void)a;(void)b;(void)c;(void)d;return FALSE;}
void WINAPI_CALL THETAZ_PathRemoveBackslashW(LPWSTR p){(void)p;}
void WINAPI_CALL THETAZ_PathRemoveBlanksW(LPWSTR p){(void)p;}
void WINAPI_CALL THETAZ_PathRemoveExtensionA(LPSTR p){(void)p;}
void WINAPI_CALL THETAZ_PathRemoveExtensionW(LPWSTR p){(void)p;}
BOOL WINAPI_CALL THETAZ_PathRenameExtensionA(LPSTR a, LPCSTR b){(void)a;(void)b;return FALSE;}
BOOL WINAPI_CALL THETAZ_PathRenameExtensionW(LPWSTR a, LPCWSTR b){(void)a;(void)b;return FALSE;}
BOOL WINAPI_CALL THETAZ_PathSkipRootW(LPWSTR p){(void)p;return FALSE;}
BOOL WINAPI_CALL THETAZ_PathStripToRootW(LPWSTR p){(void)p;return FALSE;}
BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsW(LPCWSTR a, LPWSTR b, UINT* c){(void)a;(void)b;(void)c;return FALSE;}
void WINAPI_CALL THETAZ_PathUnquoteSpacesW(LPWSTR p){(void)p;}
void WINAPI_CALL THETAZ_FreeLibraryWhenCallbackReturns(void* a, void* b){(void)a;(void)b;}
BOOL WINAPI_CALL THETAZ_UpdateProcThreadAttribute(void* a, DWORD b, DWORD c, void* d, SIZE_T e, void* f, void* g){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;return FALSE;}

HRESULT WINAPI_CALL THETAZ_UrlCanonicalizeA(const char* u,char* b,uint32_t* l,DWORD f){if(b&&u&&l){strncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlCanonicalizeW(const wchar_t* u,wchar_t* b,uint32_t* l,DWORD f){if(b&&u&&l){wcsncpy(b,u,*l);return S_OK;}return E_FAIL;}
HRESULT WINAPI_CALL THETAZ_UrlCompareA(const char* a,const char* b,DWORD f){if(!a||!b)return 0;return strcmp(a,b);}
HRESULT WINAPI_CALL THETAZ_UrlCompareW(const wchar_t* a,const wchar_t* b,DWORD f){if(!a||!b)return 0;return wcscmp(a,b);}
void* WINAPI_CALL THETAZ_UrlGetLocationA(const char* u){
    if(!u)return NULL;
    const char* p=strstr(u,"://");
    return (void*)(uintptr_t)(p?p+3:u);
}
void* WINAPI_CALL THETAZ_UrlGetLocationW(const wchar_t* u){
    if(!u)return NULL;
    const wchar_t* p=wcsstr(u,L"://");
    return (void*)(uintptr_t)(p?p+3:u);
}
int WINAPI_CALL THETAZ_StrCmpNICA(const char* a,const char* b,int n){if(!a)a="";if(!b)b="";return strncasecmp(a,b,n);}
void* WINAPI_CALL THETAZ_StrStrNIA(const char* h,const char* n,int l){if(!h||!n)return NULL;size_t nl=strlen(n);for(int i=0;i<=l-(int)nl;i++){if(strncasecmp(h+i,n,nl)==0)return (void*)(uintptr_t)(h+i);}return NULL;}
void* WINAPI_CALL THETAZ_StrStrNIW(const wchar_t* h,const wchar_t* n,int l){if(!h||!n)return NULL;size_t nl=wcslen(n);for(int i=0;i<=l-(int)nl;i++){if(wcsncasecmp(h+i,n,nl)==0)return (void*)(uintptr_t)(h+i);}return NULL;}
int WINAPI_CALL THETAZ_StrToIntExA(const char* s,DWORD f,int* v){if(v)*v=s?atoi(s):0;return S_OK;}
int WINAPI_CALL THETAZ_StrToIntExW(const wchar_t* s,DWORD f,int* v){if(v)*v=0;return S_OK;}

// --- kernel32 missing ---
BOOL WINAPI_CALL THETAZ_EnumResourceNamesW(HMODULE h,const wchar_t* t,void* cb,LONG_PTR p){return TRUE;}
UINT WINAPI_CALL THETAZ_GetSystemWindowsDirectoryW(wchar_t* b,UINT c){
    const wchar_t* path=L"C:\\Windows";
    if(b&&c>(UINT)wcslen(path)){wcscpy(b,path);return (UINT)wcslen(path);}
    return (UINT)wcslen(path);
}
int WINAPI_CALL THETAZ_LoadStringW(HINSTANCE h,UINT id,wchar_t* b,int c){if(b&&c>0){b[0]=0;return 0;}return 0;}
BOOL WINAPI_CALL THETAZ_QueueUserAPC(void* fn,HANDLE h,ULONG_PTR d){return FALSE;}
BOOL WINAPI_CALL THETAZ_QueueUserWorkItem(void* fn,void* c,ULONG f){return FALSE;}
HRESULT WINAPI_CALL THETAZ_SetThreadDescription(HANDLE h,const wchar_t* d){return S_OK;}

int WINAPI_CALL THETAZ_LoadStringA(HINSTANCE h,UINT id,char* b,int c){if(b&&c>0){b[0]=0;return 0;}return 0;}
void* WINAPI_CALL THETAZ_FindResourceExW(void* hModule,const wchar_t* lpType,const wchar_t* lpName,WORD wLanguage){(void)hModule;(void)lpType;(void)lpName;(void)wLanguage;return NULL;}
errno_t WINAPI_CALL THETAZ__i64tow_s(long long val,wchar_t* buf,size_t size,int radix){
    if(!buf||size==0)return EINVAL;
    char tmp[64];
    snprintf(tmp,sizeof(tmp),"%lld",val);
    mbstowcs(buf,tmp,size);
    return 0;
}
BOOL WINAPI_CALL THETAZ_GetComputerNameExA(int NameType,char* lpBuffer,LPDWORD nSize){
    if(lpBuffer&&nSize&&*nSize>0){strcpy(lpBuffer,"THETAPC");*nSize=strlen("THETAPC");return TRUE;}
    return FALSE;
}
double WINAPI_CALL THETAZ_atof(const char* str){return str?atof(str):0.0;}

// =========================================================================
// user32 missing ---
// =========================================================================
DWORD WINAPI_CALL THETAZ_MsgWaitForMultipleObjects(DWORD nCount,const HANDLE* pHandles,BOOL fWaitAll,DWORD dwMilliseconds,DWORD dwWakeMask){
    (void)nCount;(void)pHandles;(void)fWaitAll;(void)dwMilliseconds;(void)dwWakeMask;
    return 0;
}
BOOL WINAPI_CALL THETAZ_DestroyMenu(void* hMenu){
    (void)hMenu;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_EnumChildWindows(void* hWnd,void* lpEnumFunc,long lParam){
    (void)hWnd;(void)lpEnumFunc;(void)lParam;
    return TRUE;
}
void* WINAPI_CALL THETAZ_LoadCursorA(void* hInstance,const char* lpCursorName){
    (void)hInstance;(void)lpCursorName;
    return NULL;
}
BOOL WINAPI_CALL THETAZ_UnregisterClassA(const char* lpClassName,void* hInstance){
    (void)lpClassName;(void)hInstance;
    return TRUE;
}

// =========================================================================
// ws2_32 missing ---
// =========================================================================
void WINAPI_CALL THETAZ_FreeAddrInfoW(void* pAddrInfo){
    (void)pAddrInfo;
}
void WINAPI_CALL THETAZ_freeaddrinfo(void* pAddrInfo){
    (void)pAddrInfo;
}
int WINAPI_CALL THETAZ_getaddrinfo(const char* node,const char* service,const void* hints,void** res){
    (void)node;(void)service;(void)hints;(void)res;
    return 0;
}

// =========================================================================
// kernel32 missing additional ---
// =========================================================================
int WINAPI_CALL THETAZ_FindNextChangeNotification(void* hChangeHandle){
    (void)hChangeHandle;
    return TRUE;
}
int WINAPI_CALL THETAZ_fputs(const char* s,void* stream){
    (void)s;(void)stream;
    return 0;
}
int WINAPI_CALL THETAZ_fgets(char* buf,int n,void* stream){
    (void)buf;(void)n;(void)stream;
    return 0;
}

// =========================================================================
// Registration function
// =========================================================================
void initialize_kernel32_batch() {
    // kernel32.dll / KERNEL32.dll — Version
    register_api("kernel32.dll", "GetVersion", (void*)THETAZ_GetVersion);
    register_api("KERNEL32.dll", "GetVersion", (void*)THETAZ_GetVersion);
    register_api("kernel32.dll", "GetVersionExA", (void*)THETAZ_GetVersionExA);
    register_api("KERNEL32.dll", "GetVersionExA", (void*)THETAZ_GetVersionExA);
    register_api("kernel32.dll", "GetLogicalDrives", (void*)THETAZ_GetLogicalDrives);
    register_api("KERNEL32.dll", "GetLogicalDrives", (void*)THETAZ_GetLogicalDrives);
    register_api("kernel32.dll", "GetSystemDirectoryA", (void*)THETAZ_GetSystemDirectoryA);
    register_api("KERNEL32.dll", "GetSystemDirectoryA", (void*)THETAZ_GetSystemDirectoryA);
    register_api("kernel32.dll", "GetSystemDirectoryW", (void*)THETAZ_GetSystemDirectoryW);
    register_api("KERNEL32.dll", "GetSystemDirectoryW", (void*)THETAZ_GetSystemDirectoryW);
    register_api("kernelbase.dll", "GetSystemDirectoryW", (void*)THETAZ_GetSystemDirectoryW);
    register_api("kernel32.dll", "GetWindowsDirectoryA", (void*)THETAZ_GetWindowsDirectoryA);
    register_api("KERNEL32.dll", "GetWindowsDirectoryA", (void*)THETAZ_GetWindowsDirectoryA);
    register_api("kernel32.dll", "GetWindowsDirectoryW", (void*)THETAZ_GetWindowsDirectoryW);
    register_api("KERNEL32.dll", "GetWindowsDirectoryW", (void*)THETAZ_GetWindowsDirectoryW);
    register_api("kernel32.dll", "CreateToolhelp32Snapshot", (void*)THETAZ_CreateToolhelp32Snapshot);
    register_api("KERNEL32.dll", "CreateToolhelp32Snapshot", (void*)THETAZ_CreateToolhelp32Snapshot);
    register_api("kernel32.dll", "Module32FirstW", (void*)THETAZ_Module32FirstW);
    register_api("KERNEL32.dll", "Module32FirstW", (void*)THETAZ_Module32FirstW);
    register_api("kernel32.dll", "Module32NextW", (void*)THETAZ_Module32NextW);
    register_api("KERNEL32.dll", "Module32NextW", (void*)THETAZ_Module32NextW);
    register_api("kernel32.dll", "Process32FirstW", (void*)THETAZ_Process32FirstW);
    register_api("KERNEL32.dll", "Process32FirstW", (void*)THETAZ_Process32FirstW);
    register_api("kernel32.dll", "Process32NextW", (void*)THETAZ_Process32NextW);
    register_api("KERNEL32.dll", "Process32NextW", (void*)THETAZ_Process32NextW);
    register_api("kernel32.dll", "Thread32First", (void*)THETAZ_Thread32First);
    register_api("KERNEL32.dll", "Thread32First", (void*)THETAZ_Thread32First);
    register_api("kernel32.dll", "Thread32Next", (void*)THETAZ_Thread32Next);
    register_api("KERNEL32.dll", "Thread32Next", (void*)THETAZ_Thread32Next);
    register_api("kernel32.dll", "AddAtomA", (void*)THETAZ_AddAtomA);
    register_api("KERNEL32.dll", "AddAtomA", (void*)THETAZ_AddAtomA);
    register_api("kernel32.dll", "FindAtomA", (void*)THETAZ_FindAtomA);
    register_api("KERNEL32.dll", "FindAtomA", (void*)THETAZ_FindAtomA);
    register_api("kernel32.dll", "FindAtomW", (void*)THETAZ_FindAtomW);
    register_api("KERNEL32.dll", "FindAtomW", (void*)THETAZ_FindAtomW);
    register_api("kernel32.dll", "GetAtomNameA", (void*)THETAZ_GetAtomNameA);
    register_api("KERNEL32.dll", "GetAtomNameA", (void*)THETAZ_GetAtomNameA);
    register_api("kernel32.dll", "GetAtomNameW", (void*)THETAZ_GetAtomNameW);
    register_api("KERNEL32.dll", "GetAtomNameW", (void*)THETAZ_GetAtomNameW);
    register_api("kernel32.dll", "GlobalFindAtomA", (void*)THETAZ_GlobalFindAtomA);
    register_api("KERNEL32.dll", "GlobalFindAtomA", (void*)THETAZ_GlobalFindAtomA);
    register_api("kernel32.dll", "GlobalFindAtomW", (void*)THETAZ_GlobalFindAtomW);
    register_api("KERNEL32.dll", "GlobalFindAtomW", (void*)THETAZ_GlobalFindAtomW);
    register_api("kernel32.dll", "GlobalGetAtomNameW", (void*)THETAZ_GlobalGetAtomNameW);
    register_api("KERNEL32.dll", "GlobalGetAtomNameW", (void*)THETAZ_GlobalGetAtomNameW);
    register_api("kernel32.dll", "GlobalLock", (void*)THETAZ_GlobalLock);
    register_api("KERNEL32.dll", "GlobalLock", (void*)THETAZ_GlobalLock);
    register_api("kernel32.dll", "GlobalUnlock", (void*)THETAZ_GlobalUnlock);
    register_api("KERNEL32.dll", "GlobalUnlock", (void*)THETAZ_GlobalUnlock);
    register_api("kernel32.dll", "GlobalHandle", (void*)THETAZ_GlobalHandle);
    register_api("KERNEL32.dll", "GlobalHandle", (void*)THETAZ_GlobalHandle);
    register_api("kernel32.dll", "GlobalFlags", (void*)THETAZ_GlobalFlags);
    register_api("KERNEL32.dll", "GlobalFlags", (void*)THETAZ_GlobalFlags);
    register_api("kernel32.dll", "LocalLock", (void*)THETAZ_LocalLock);
    register_api("KERNEL32.dll", "LocalLock", (void*)THETAZ_LocalLock);
    register_api("kernel32.dll", "_lclose", (void*)THETAZ__lclose);
    register_api("KERNEL32.dll", "_lclose", (void*)THETAZ__lclose);
    register_api("kernel32.dll", "_llseek", (void*)THETAZ__llseek);
    register_api("KERNEL32.dll", "_llseek", (void*)THETAZ__llseek);
    register_api("kernel32.dll", "_lread", (void*)THETAZ__lread);
    register_api("KERNEL32.dll", "_lread", (void*)THETAZ__lread);
    register_api("kernel32.dll", "GetModuleHandleExA", (void*)THETAZ_GetModuleHandleExA);
    register_api("KERNEL32.dll", "GetModuleHandleExA", (void*)THETAZ_GetModuleHandleExA);
    register_api("kernel32.dll", "GetProcessId", (void*)THETAZ_GetProcessId);
    register_api("KERNEL32.dll", "GetProcessId", (void*)THETAZ_GetProcessId);
    register_api("kernelbase.dll", "GetProcessId", (void*)THETAZ_GetProcessId);
    register_api("kernel32.dll", "SetPriorityClass", (void*)THETAZ_SetPriorityClass);
    register_api("KERNEL32.dll", "SetPriorityClass", (void*)THETAZ_SetPriorityClass);
    register_api("kernel32.dll", "SetProcessAffinityMask", (void*)THETAZ_SetProcessAffinityMask);
    register_api("KERNEL32.dll", "SetProcessAffinityMask", (void*)THETAZ_SetProcessAffinityMask);
    register_api("kernel32.dll", "GetErrorMode", (void*)THETAZ_GetErrorMode);
    register_api("KERNEL32.dll", "GetErrorMode", (void*)THETAZ_GetErrorMode);
    register_api("kernel32.dll", "SetThreadErrorMode", (void*)THETAZ_SetThreadErrorMode);
    register_api("KERNEL32.dll", "SetThreadErrorMode", (void*)THETAZ_SetThreadErrorMode);
    register_api("kernel32.dll", "SetFileAttributesA", (void*)THETAZ_SetFileAttributesA);
    register_api("KERNEL32.dll", "SetFileAttributesA", (void*)THETAZ_SetFileAttributesA);
    register_api("kernel32.dll", "SearchPathA", (void*)THETAZ_SearchPathA);
    register_api("KERNEL32.dll", "SearchPathA", (void*)THETAZ_SearchPathA);
    register_api("kernel32.dll", "SearchPathW", (void*)THETAZ_SearchPathW);
    register_api("KERNEL32.dll", "SearchPathW", (void*)THETAZ_SearchPathW);
    register_api("kernel32.dll", "OpenFileMappingW", (void*)THETAZ_OpenFileMappingW);
    register_api("KERNEL32.dll", "OpenFileMappingW", (void*)THETAZ_OpenFileMappingW);
    register_api("kernel32.dll", "MapViewOfFileEx", (void*)THETAZ_MapViewOfFileEx);
    register_api("KERNEL32.dll", "MapViewOfFileEx", (void*)THETAZ_MapViewOfFileEx);
    register_api("kernel32.dll", "MoveFileW", (void*)THETAZ_MoveFileW);
    register_api("KERNEL32.dll", "MoveFileW", (void*)THETAZ_MoveFileW);
    register_api("kernel32.dll", "CreateHardLinkW", (void*)THETAZ_CreateHardLinkW);
    register_api("KERNEL32.dll", "CreateHardLinkW", (void*)THETAZ_CreateHardLinkW);
    register_api("kernel32.dll", "GetShortPathNameW", (void*)THETAZ_GetShortPathNameW);
    register_api("KERNEL32.dll", "GetShortPathNameW", (void*)THETAZ_GetShortPathNameW);
    register_api("kernel32.dll", "CopyFile2", (void*)THETAZ_CopyFile2);
    register_api("KERNEL32.dll", "CopyFile2", (void*)THETAZ_CopyFile2);
    register_api("kernel32.dll", "GetTempFileNameA", (void*)THETAZ_GetTempFileNameA);
    register_api("KERNEL32.dll", "GetTempFileNameA", (void*)THETAZ_GetTempFileNameA);
    register_api("kernel32.dll", "ReplaceFileW", (void*)THETAZ_ReplaceFileW);
    register_api("KERNEL32.dll", "ReplaceFileW", (void*)THETAZ_ReplaceFileW);
    register_api("kernel32.dll", "CreateNamedPipeW", (void*)THETAZ_CreateNamedPipeW);
    register_api("KERNEL32.dll", "CreateNamedPipeW", (void*)THETAZ_CreateNamedPipeW);
    register_api("kernel32.dll", "ConnectNamedPipe", (void*)THETAZ_ConnectNamedPipe);
    register_api("KERNEL32.dll", "ConnectNamedPipe", (void*)THETAZ_ConnectNamedPipe);
    register_api("kernel32.dll", "DisconnectNamedPipe", (void*)THETAZ_DisconnectNamedPipe);
    register_api("KERNEL32.dll", "DisconnectNamedPipe", (void*)THETAZ_DisconnectNamedPipe);
    register_api("kernel32.dll", "SetNamedPipeHandleState", (void*)THETAZ_SetNamedPipeHandleState);
    register_api("KERNEL32.dll", "SetNamedPipeHandleState", (void*)THETAZ_SetNamedPipeHandleState);
    register_api("kernel32.dll", "GetNamedPipeInfo", (void*)THETAZ_GetNamedPipeInfo);
    register_api("KERNEL32.dll", "GetNamedPipeInfo", (void*)THETAZ_GetNamedPipeInfo);
    register_api("kernel32.dll", "WaitNamedPipeA", (void*)THETAZ_WaitNamedPipeA);
    register_api("KERNEL32.dll", "WaitNamedPipeA", (void*)THETAZ_WaitNamedPipeA);
    register_api("kernel32.dll", "WaitNamedPipeW", (void*)THETAZ_WaitNamedPipeW);
    register_api("KERNEL32.dll", "WaitNamedPipeW", (void*)THETAZ_WaitNamedPipeW);
    register_api("kernel32.dll", "GetNamedPipeClientProcessId", (void*)THETAZ_GetNamedPipeClientProcessId);
    register_api("KERNEL32.dll", "GetNamedPipeClientProcessId", (void*)THETAZ_GetNamedPipeClientProcessId);
    register_api("kernel32.dll", "CreateSemaphoreW", (void*)THETAZ_CreateSemaphoreW);
    register_api("KERNEL32.dll", "CreateSemaphoreW", (void*)THETAZ_CreateSemaphoreW);
    register_api("kernelbase.dll", "CreateSemaphoreW", (void*)THETAZ_CreateSemaphoreW);
    register_api("kernel32.dll", "OpenSemaphoreW", (void*)THETAZ_OpenSemaphoreW);
    register_api("KERNEL32.dll", "OpenSemaphoreW", (void*)THETAZ_OpenSemaphoreW);
    register_api("kernelbase.dll", "OpenSemaphoreW", (void*)THETAZ_OpenSemaphoreW);
    register_api("kernel32.dll", "SleepConditionVariableCS", (void*)THETAZ_SleepConditionVariableCS);
    register_api("KERNEL32.dll", "SleepConditionVariableCS", (void*)THETAZ_SleepConditionVariableCS);
    register_api("kernelbase.dll", "SleepConditionVariableCS", (void*)THETAZ_SleepConditionVariableCS);
    register_api("kernel32.dll", "SignalObjectAndWait", (void*)THETAZ_SignalObjectAndWait);
    register_api("KERNEL32.dll", "SignalObjectAndWait", (void*)THETAZ_SignalObjectAndWait);
    register_api("kernelbase.dll", "SignalObjectAndWait", (void*)THETAZ_SignalObjectAndWait);
    register_api("kernel32.dll", "SleepEx", (void*)THETAZ_SleepEx);
    register_api("KERNEL32.dll", "SleepEx", (void*)THETAZ_SleepEx);
    register_api("kernelbase.dll", "SleepEx", (void*)THETAZ_SleepEx);
    register_api("kernel32.dll", "DebugBreak", (void*)THETAZ_DebugBreak);
    register_api("KERNEL32.dll", "DebugBreak", (void*)THETAZ_DebugBreak);
    register_api("kernel32.dll", "TerminateThread", (void*)THETAZ_TerminateThread);
    register_api("KERNEL32.dll", "TerminateThread", (void*)THETAZ_TerminateThread);
    register_api("kernel32.dll", "CreateJobObjectW", (void*)THETAZ_CreateJobObjectW);
    register_api("KERNEL32.dll", "CreateJobObjectW", (void*)THETAZ_CreateJobObjectW);
    register_api("kernel32.dll", "AssignProcessToJobObject", (void*)THETAZ_AssignProcessToJobObject);
    register_api("KERNEL32.dll", "AssignProcessToJobObject", (void*)THETAZ_AssignProcessToJobObject);
    register_api("kernel32.dll", "SetInformationJobObject", (void*)THETAZ_SetInformationJobObject);
    register_api("KERNEL32.dll", "SetInformationJobObject", (void*)THETAZ_SetInformationJobObject);
    register_api("kernel32.dll", "TerminateJobObject", (void*)THETAZ_TerminateJobObject);
    register_api("KERNEL32.dll", "TerminateJobObject", (void*)THETAZ_TerminateJobObject);
    register_api("kernel32.dll", "ReadProcessMemory", (void*)THETAZ_ReadProcessMemory);
    register_api("KERNEL32.dll", "ReadProcessMemory", (void*)THETAZ_ReadProcessMemory);
    register_api("kernel32.dll", "GetConsoleWindow", (void*)THETAZ_GetConsoleWindow);
    register_api("KERNEL32.dll", "GetConsoleWindow", (void*)THETAZ_GetConsoleWindow);
    register_api("kernel32.dll", "GetProcessMitigationPolicy", (void*)THETAZ_GetProcessMitigationPolicy);
    register_api("KERNEL32.dll", "GetProcessMitigationPolicy", (void*)THETAZ_GetProcessMitigationPolicy);
    register_api("kernel32.dll", "FlushInstructionCache", (void*)THETAZ_FlushInstructionCache);
    register_api("KERNEL32.dll", "FlushInstructionCache", (void*)THETAZ_FlushInstructionCache);
    register_api("kernel32.dll", "FlushProcessWriteBuffers", (void*)THETAZ_FlushProcessWriteBuffers);
    register_api("KERNEL32.dll", "FlushProcessWriteBuffers", (void*)THETAZ_FlushProcessWriteBuffers);
    register_api("kernel32.dll", "HeapQueryInformation", (void*)THETAZ_HeapQueryInformation);
    register_api("KERNEL32.dll", "HeapQueryInformation", (void*)THETAZ_HeapQueryInformation);
    register_api("kernel32.dll", "GetThreadContext", (void*)THETAZ_GetThreadContext);
    register_api("KERNEL32.dll", "GetThreadContext", (void*)THETAZ_GetThreadContext);
    register_api("kernel32.dll", "ConvertDefaultLocale", (void*)THETAZ_ConvertDefaultLocale);
    register_api("KERNEL32.dll", "ConvertDefaultLocale", (void*)THETAZ_ConvertDefaultLocale);
    register_api("kernel32.dll", "GetSystemDefaultLangID", (void*)THETAZ_GetSystemDefaultLangID);
    register_api("KERNEL32.dll", "GetSystemDefaultLangID", (void*)THETAZ_GetSystemDefaultLangID);
    register_api("kernel32.dll", "GetUserDefaultLangID", (void*)THETAZ_GetUserDefaultLangID);
    register_api("KERNEL32.dll", "GetUserDefaultLangID", (void*)THETAZ_GetUserDefaultLangID);
    register_api("kernel32.dll", "GetSystemDefaultUILanguage", (void*)THETAZ_GetSystemDefaultUILanguage);
    register_api("KERNEL32.dll", "GetSystemDefaultUILanguage", (void*)THETAZ_GetSystemDefaultUILanguage);
    register_api("kernel32.dll", "GetUserDefaultUILanguage", (void*)THETAZ_GetUserDefaultUILanguage);
    register_api("KERNEL32.dll", "GetUserDefaultUILanguage", (void*)THETAZ_GetUserDefaultUILanguage);
    register_api("kernel32.dll", "GetThreadUILanguage", (void*)THETAZ_GetThreadUILanguage);
    register_api("KERNEL32.dll", "GetThreadUILanguage", (void*)THETAZ_GetThreadUILanguage);
    register_api("kernel32.dll", "SetThreadPreferredUILanguages", (void*)THETAZ_SetThreadPreferredUILanguages);
    register_api("KERNEL32.dll", "SetThreadPreferredUILanguages", (void*)THETAZ_SetThreadPreferredUILanguages);
    register_api("kernel32.dll", "GetCurrentProcessorNumber", (void*)THETAZ_GetCurrentProcessorNumber);
    register_api("KERNEL32.dll", "GetCurrentProcessorNumber", (void*)THETAZ_GetCurrentProcessorNumber);
    register_api("kernelbase.dll", "GetCurrentProcessorNumber", (void*)THETAZ_GetCurrentProcessorNumber);
    register_api("kernel32.dll", "GetNumaHighestNodeNumber", (void*)THETAZ_GetNumaHighestNodeNumber);
    register_api("KERNEL32.dll", "GetNumaHighestNodeNumber", (void*)THETAZ_GetNumaHighestNodeNumber);
    register_api("kernel32.dll", "SetThreadGroupAffinity", (void*)THETAZ_SetThreadGroupAffinity);
    register_api("KERNEL32.dll", "SetThreadGroupAffinity", (void*)THETAZ_SetThreadGroupAffinity);
    register_api("kernelbase.dll", "SetThreadGroupAffinity", (void*)THETAZ_SetThreadGroupAffinity);
    register_api("kernel32.dll", "GetThreadGroupAffinity", (void*)THETAZ_GetThreadGroupAffinity);
    register_api("KERNEL32.dll", "GetThreadGroupAffinity", (void*)THETAZ_GetThreadGroupAffinity);
    register_api("kernel32.dll", "GetLogicalProcessorInformationEx", (void*)THETAZ_GetLogicalProcessorInformationEx);
    register_api("KERNEL32.dll", "GetLogicalProcessorInformationEx", (void*)THETAZ_GetLogicalProcessorInformationEx);
    register_api("kernel32.dll", "GetSystemFirmwareTable", (void*)THETAZ_GetSystemFirmwareTable);
    register_api("KERNEL32.dll", "GetSystemFirmwareTable", (void*)THETAZ_GetSystemFirmwareTable);
    register_api("kernel32.dll", "GetSystemTimes", (void*)THETAZ_GetSystemTimes);
    register_api("KERNEL32.dll", "GetSystemTimes", (void*)THETAZ_GetSystemTimes);
    register_api("kernel32.dll", "IsDBCSLeadByteEx", (void*)THETAZ_IsDBCSLeadByteEx);
    register_api("KERNEL32.dll", "IsDBCSLeadByteEx", (void*)THETAZ_IsDBCSLeadByteEx);
    register_api("kernel32.dll", "IdnToAscii", (void*)THETAZ_IdnToAscii);
    register_api("KERNEL32.dll", "IdnToAscii", (void*)THETAZ_IdnToAscii);
    register_api("kernel32.dll", "IdnToUnicode", (void*)THETAZ_IdnToUnicode);
    register_api("KERNEL32.dll", "IdnToUnicode", (void*)THETAZ_IdnToUnicode);
    register_api("kernel32.dll", "GetDateFormatA", (void*)THETAZ_GetDateFormatA);
    register_api("KERNEL32.dll", "GetDateFormatA", (void*)THETAZ_GetDateFormatA);
    register_api("kernel32.dll", "GetTimeFormatA", (void*)THETAZ_GetTimeFormatA);
    register_api("KERNEL32.dll", "GetTimeFormatA", (void*)THETAZ_GetTimeFormatA);
    register_api("kernel32.dll", "GetCalendarInfoEx", (void*)THETAZ_GetCalendarInfoEx);
    register_api("KERNEL32.dll", "GetCalendarInfoEx", (void*)THETAZ_GetCalendarInfoEx);
    register_api("kernel32.dll", "GetCalendarInfoW", (void*)THETAZ_GetCalendarInfoW);
    register_api("KERNEL32.dll", "GetCalendarInfoW", (void*)THETAZ_GetCalendarInfoW);
    register_api("kernel32.dll", "GetCurrencyFormatEx", (void*)THETAZ_GetCurrencyFormatEx);
    register_api("KERNEL32.dll", "GetCurrencyFormatEx", (void*)THETAZ_GetCurrencyFormatEx);
    register_api("kernel32.dll", "GetNumberFormatEx", (void*)THETAZ_GetNumberFormatEx);
    register_api("KERNEL32.dll", "GetNumberFormatEx", (void*)THETAZ_GetNumberFormatEx);
    register_api("kernel32.dll", "GetGeoInfoW", (void*)THETAZ_GetGeoInfoW);
    register_api("KERNEL32.dll", "GetGeoInfoW", (void*)THETAZ_GetGeoInfoW);
    register_api("kernel32.dll", "FindNLSStringEx", (void*)THETAZ_FindNLSStringEx);
    register_api("KERNEL32.dll", "FindNLSStringEx", (void*)THETAZ_FindNLSStringEx);
    register_api("kernel32.dll", "FoldStringW", (void*)THETAZ_FoldStringW);
    register_api("KERNEL32.dll", "FoldStringW", (void*)THETAZ_FoldStringW);
    register_api("kernel32.dll", "NormalizeString", (void*)THETAZ_NormalizeString);
    register_api("KERNEL32.dll", "NormalizeString", (void*)THETAZ_NormalizeString);
    register_api("kernel32.dll", "ResolveLocaleName", (void*)THETAZ_ResolveLocaleName);
    register_api("KERNEL32.dll", "ResolveLocaleName", (void*)THETAZ_ResolveLocaleName);
    register_api("kernel32.dll", "GetDynamicTimeZoneInformation", (void*)THETAZ_GetDynamicTimeZoneInformation);
    register_api("KERNEL32.dll", "GetDynamicTimeZoneInformation", (void*)THETAZ_GetDynamicTimeZoneInformation);
    register_api("kernelbase.dll", "GetDynamicTimeZoneInformation", (void*)THETAZ_GetDynamicTimeZoneInformation);
    register_api("kernel32.dll", "SystemTimeToTzSpecificLocalTime", (void*)THETAZ_SystemTimeToTzSpecificLocalTime);
    register_api("KERNEL32.dll", "SystemTimeToTzSpecificLocalTime", (void*)THETAZ_SystemTimeToTzSpecificLocalTime);
    register_api("kernelbase.dll", "SystemTimeToTzSpecificLocalTime", (void*)THETAZ_SystemTimeToTzSpecificLocalTime);
    register_api("kernel32.dll", "FileTimeToDosDateTime", (void*)THETAZ_FileTimeToDosDateTime);
    register_api("KERNEL32.dll", "FileTimeToDosDateTime", (void*)THETAZ_FileTimeToDosDateTime);
    register_api("kernel32.dll", "DosDateTimeToFileTime", (void*)THETAZ_DosDateTimeToFileTime);
    register_api("KERNEL32.dll", "DosDateTimeToFileTime", (void*)THETAZ_DosDateTimeToFileTime);
    register_api("kernel32.dll", "FileTimeToLocalFileTime", (void*)THETAZ_FileTimeToLocalFileTime);
    register_api("KERNEL32.dll", "FileTimeToLocalFileTime", (void*)THETAZ_FileTimeToLocalFileTime);
    register_api("kernel32.dll", "LocalFileTimeToFileTime", (void*)THETAZ_LocalFileTimeToFileTime);
    register_api("KERNEL32.dll", "LocalFileTimeToFileTime", (void*)THETAZ_LocalFileTimeToFileTime);
    register_api("kernel32.dll", "timeBeginPeriod", (void*)THETAZ_timeBeginPeriod);
    register_api("KERNEL32.dll", "timeBeginPeriod", (void*)THETAZ_timeBeginPeriod);
    register_api("kernel32.dll", "timeEndPeriod", (void*)THETAZ_timeEndPeriod);
    register_api("KERNEL32.dll", "timeEndPeriod", (void*)THETAZ_timeEndPeriod);
    register_api("kernel32.dll", "timeGetDevCaps", (void*)THETAZ_timeGetDevCaps);
    register_api("KERNEL32.dll", "timeGetDevCaps", (void*)THETAZ_timeGetDevCaps);
    register_api("kernel32.dll", "timeGetSystemTime", (void*)THETAZ_timeGetSystemTime);
    register_api("KERNEL32.dll", "timeGetSystemTime", (void*)THETAZ_timeGetSystemTime);
    register_api("kernel32.dll", "timeGetTime", (void*)THETAZ_timeGetTime);
    register_api("KERNEL32.dll", "timeGetTime", (void*)THETAZ_timeGetTime);
    register_api("kernel32.dll", "LZClose", (void*)THETAZ_LZClose);
    register_api("KERNEL32.dll", "LZClose", (void*)THETAZ_LZClose);
    register_api("kernel32.dll", "LZCopy", (void*)THETAZ_LZCopy);
    register_api("KERNEL32.dll", "LZCopy", (void*)THETAZ_LZCopy);
    register_api("kernel32.dll", "LZOpenFileW", (void*)THETAZ_LZOpenFileW);
    register_api("KERNEL32.dll", "LZOpenFileW", (void*)THETAZ_LZOpenFileW);
    register_api("kernel32.dll", "LZSeek", (void*)THETAZ_LZSeek);
    register_api("KERNEL32.dll", "LZSeek", (void*)THETAZ_LZSeek);
    register_api("kernel32.dll", "lstrcmpA", (void*)THETAZ_lstrcmpA);
    register_api("KERNEL32.dll", "lstrcmpA", (void*)THETAZ_lstrcmpA);
    register_api("kernel32.dll", "lstrcmpiA", (void*)THETAZ_lstrcmpiA);
    register_api("KERNEL32.dll", "lstrcmpiA", (void*)THETAZ_lstrcmpiA);
    register_api("kernel32.dll", "lstrlenA", (void*)THETAZ_lstrlenA);
    register_api("KERNEL32.dll", "lstrlenA", (void*)THETAZ_lstrlenA);
    register_api("kernel32.dll", "GetPrivateProfileIntW", (void*)THETAZ_GetPrivateProfileIntW);
    register_api("KERNEL32.dll", "GetPrivateProfileIntW", (void*)THETAZ_GetPrivateProfileIntW);
    register_api("kernel32.dll", "GetPrivateProfileStringA", (void*)THETAZ_GetPrivateProfileStringA);
    register_api("KERNEL32.dll", "GetPrivateProfileStringA", (void*)THETAZ_GetPrivateProfileStringA);
    register_api("kernel32.dll", "GetPrivateProfileStringW", (void*)THETAZ_GetPrivateProfileStringW);
    register_api("KERNEL32.dll", "GetPrivateProfileStringW", (void*)THETAZ_GetPrivateProfileStringW);
    register_api("kernel32.dll", "WritePrivateProfileStringA", (void*)THETAZ_WritePrivateProfileStringA);
    register_api("KERNEL32.dll", "WritePrivateProfileStringA", (void*)THETAZ_WritePrivateProfileStringA);
    register_api("kernel32.dll", "WritePrivateProfileStringW", (void*)THETAZ_WritePrivateProfileStringW);
    register_api("KERNEL32.dll", "WritePrivateProfileStringW", (void*)THETAZ_WritePrivateProfileStringW);
    register_api("kernel32.dll", "CreateThreadpool", (void*)THETAZ_CreateThreadpool);
    register_api("KERNEL32.dll", "CreateThreadpool", (void*)THETAZ_CreateThreadpool);
    register_api("kernelbase.dll", "CreateThreadpool", (void*)THETAZ_CreateThreadpool);
    register_api("kernel32.dll", "CreateThreadpoolCleanupGroup", (void*)THETAZ_CreateThreadpoolCleanupGroup);
    register_api("KERNEL32.dll", "CreateThreadpoolCleanupGroup", (void*)THETAZ_CreateThreadpoolCleanupGroup);
    register_api("kernelbase.dll", "CreateThreadpoolCleanupGroup", (void*)THETAZ_CreateThreadpoolCleanupGroup);
    register_api("kernel32.dll", "CreateThreadpoolIo", (void*)THETAZ_CreateThreadpoolIo);
    register_api("KERNEL32.dll", "CreateThreadpoolIo", (void*)THETAZ_CreateThreadpoolIo);
    register_api("kernelbase.dll", "CreateThreadpoolIo", (void*)THETAZ_CreateThreadpoolIo);
    register_api("kernel32.dll", "SetThreadpoolThreadMaximum", (void*)THETAZ_SetThreadpoolThreadMaximum);
    register_api("KERNEL32.dll", "SetThreadpoolThreadMaximum", (void*)THETAZ_SetThreadpoolThreadMaximum);
    register_api("kernelbase.dll", "SetThreadpoolThreadMaximum", (void*)THETAZ_SetThreadpoolThreadMaximum);
    register_api("kernel32.dll", "SetThreadpoolThreadMinimum", (void*)THETAZ_SetThreadpoolThreadMinimum);
    register_api("KERNEL32.dll", "SetThreadpoolThreadMinimum", (void*)THETAZ_SetThreadpoolThreadMinimum);
    register_api("kernelbase.dll", "SetThreadpoolThreadMinimum", (void*)THETAZ_SetThreadpoolThreadMinimum);
    register_api("kernel32.dll", "CloseThreadpoolIo", (void*)THETAZ_CloseThreadpoolIo);
    register_api("KERNEL32.dll", "CloseThreadpoolIo", (void*)THETAZ_CloseThreadpoolIo);
    register_api("kernelbase.dll", "CloseThreadpoolIo", (void*)THETAZ_CloseThreadpoolIo);
    register_api("kernel32.dll", "StartThreadpoolIo", (void*)THETAZ_StartThreadpoolIo);
    register_api("KERNEL32.dll", "StartThreadpoolIo", (void*)THETAZ_StartThreadpoolIo);
    register_api("kernelbase.dll", "StartThreadpoolIo", (void*)THETAZ_StartThreadpoolIo);
    register_api("kernel32.dll", "CancelThreadpoolIo", (void*)THETAZ_CancelThreadpoolIo);
    register_api("KERNEL32.dll", "CancelThreadpoolIo", (void*)THETAZ_CancelThreadpoolIo);
    register_api("kernelbase.dll", "CancelThreadpoolIo", (void*)THETAZ_CancelThreadpoolIo);
    register_api("kernel32.dll", "FreeLibraryWhenCallbackReturns", (void*)THETAZ_FreeLibraryWhenCallbackReturns);
    register_api("KERNEL32.dll", "FreeLibraryWhenCallbackReturns", (void*)THETAZ_FreeLibraryWhenCallbackReturns);
    register_api("kernelbase.dll", "FreeLibraryWhenCallbackReturns", (void*)THETAZ_FreeLibraryWhenCallbackReturns);
    register_api("kernel32.dll", "TrySubmitThreadpoolCallback", (void*)THETAZ_TrySubmitThreadpoolCallback);
    register_api("KERNEL32.dll", "TrySubmitThreadpoolCallback", (void*)THETAZ_TrySubmitThreadpoolCallback);
    register_api("kernelbase.dll", "TrySubmitThreadpoolCallback", (void*)THETAZ_TrySubmitThreadpoolCallback);
    register_api("kernel32.dll", "WaitForThreadpoolWorkCallbacks", (void*)THETAZ_WaitForThreadpoolWorkCallbacks);
    register_api("KERNEL32.dll", "WaitForThreadpoolWorkCallbacks", (void*)THETAZ_WaitForThreadpoolWorkCallbacks);
    register_api("kernelbase.dll", "WaitForThreadpoolWorkCallbacks", (void*)THETAZ_WaitForThreadpoolWorkCallbacks);
    register_api("kernel32.dll", "InitializeProcThreadAttributeList", (void*)THETAZ_InitializeProcThreadAttributeList);
    register_api("KERNEL32.dll", "InitializeProcThreadAttributeList", (void*)THETAZ_InitializeProcThreadAttributeList);
    register_api("kernelbase.dll", "InitializeProcThreadAttributeList", (void*)THETAZ_InitializeProcThreadAttributeList);
    register_api("kernel32.dll", "UpdateProcThreadAttribute", (void*)(BOOL(*)(void*,DWORD,DWORD,void*,SIZE_T,void*,void*))THETAZ_UpdateProcThreadAttribute);
    register_api("KERNEL32.dll", "UpdateProcThreadAttribute", (void*)(BOOL(*)(void*,DWORD,DWORD,void*,SIZE_T,void*,void*))THETAZ_UpdateProcThreadAttribute);
    register_api("kernelbase.dll", "UpdateProcThreadAttribute", (void*)(BOOL(*)(void*,DWORD,DWORD,void*,SIZE_T,void*,void*))THETAZ_UpdateProcThreadAttribute);
    register_api("kernel32.dll", "GetEnabledXStateFeatures", (void*)THETAZ_GetEnabledXStateFeatures);
    register_api("KERNEL32.dll", "GetEnabledXStateFeatures", (void*)THETAZ_GetEnabledXStateFeatures);
    register_api("kernel32.dll", "BindIoCompletionCallback", (void*)THETAZ_BindIoCompletionCallback);
    register_api("KERNEL32.dll", "BindIoCompletionCallback", (void*)THETAZ_BindIoCompletionCallback);
    register_api("kernel32.dll", "RaiseFailFastException", (void*)THETAZ_RaiseFailFastException);
    register_api("KERNEL32.dll", "RaiseFailFastException", (void*)THETAZ_RaiseFailFastException);
    register_api("kernel32.dll", "InterlockedFlushSList", (void*)THETAZ_InterlockedFlushSList);
    register_api("KERNEL32.dll", "InterlockedFlushSList", (void*)THETAZ_InterlockedFlushSList);
    register_api("kernel32.dll", "ApplicationRecoveryFinished", (void*)THETAZ_ApplicationRecoveryFinished);
    register_api("KERNEL32.dll", "ApplicationRecoveryFinished", (void*)THETAZ_ApplicationRecoveryFinished);
    register_api("kernelbase.dll", "ApplicationRecoveryFinished", (void*)THETAZ_ApplicationRecoveryFinished);
    register_api("kernel32.dll", "ApplicationRecoveryInProgress", (void*)THETAZ_ApplicationRecoveryInProgress);
    register_api("KERNEL32.dll", "ApplicationRecoveryInProgress", (void*)THETAZ_ApplicationRecoveryInProgress);
    register_api("kernelbase.dll", "ApplicationRecoveryInProgress", (void*)THETAZ_ApplicationRecoveryInProgress);
    register_api("kernel32.dll", "RegisterApplicationRecoveryCallback", (void*)THETAZ_RegisterApplicationRecoveryCallback);
    register_api("KERNEL32.dll", "RegisterApplicationRecoveryCallback", (void*)THETAZ_RegisterApplicationRecoveryCallback);
    register_api("kernelbase.dll", "RegisterApplicationRecoveryCallback", (void*)THETAZ_RegisterApplicationRecoveryCallback);
    register_api("kernel32.dll", "RegisterApplicationRestart", (void*)THETAZ_RegisterApplicationRestart);
    register_api("KERNEL32.dll", "RegisterApplicationRestart", (void*)THETAZ_RegisterApplicationRestart);
    register_api("kernelbase.dll", "RegisterApplicationRestart", (void*)THETAZ_RegisterApplicationRestart);
    register_api("kernel32.dll", "GetCurrentPackageFullName", (void*)THETAZ_GetCurrentPackageFullName);
    register_api("KERNEL32.dll", "GetCurrentPackageFullName", (void*)THETAZ_GetCurrentPackageFullName);
    register_api("kernelbase.dll", "GetCurrentPackageFullName", (void*)THETAZ_GetCurrentPackageFullName);
    register_api("kernel32.dll", "GetCurrentPackagePath", (void*)THETAZ_GetCurrentPackagePath);
    register_api("KERNEL32.dll", "GetCurrentPackagePath", (void*)THETAZ_GetCurrentPackagePath);
    register_api("kernelbase.dll", "GetCurrentPackagePath", (void*)THETAZ_GetCurrentPackagePath);
    register_api("kernel32.dll", "GetPackageFamilyName", (void*)THETAZ_GetPackageFamilyName);
    register_api("KERNEL32.dll", "GetPackageFamilyName", (void*)THETAZ_GetPackageFamilyName);
    register_api("kernelbase.dll", "GetPackageFamilyName", (void*)THETAZ_GetPackageFamilyName);
    register_api("kernel32.dll", "GetPackagePathByFullName", (void*)THETAZ_GetPackagePathByFullName);
    register_api("KERNEL32.dll", "GetPackagePathByFullName", (void*)THETAZ_GetPackagePathByFullName);
    register_api("kernelbase.dll", "GetPackagePathByFullName", (void*)THETAZ_GetPackagePathByFullName);
    register_api("kernel32.dll", "FindPackagesByPackageFamily", (void*)THETAZ_FindPackagesByPackageFamily);
    register_api("KERNEL32.dll", "FindPackagesByPackageFamily", (void*)THETAZ_FindPackagesByPackageFamily);
    register_api("kernelbase.dll", "FindPackagesByPackageFamily", (void*)THETAZ_FindPackagesByPackageFamily);
    register_api("kernel32.dll", "WerRegisterFile", (void*)THETAZ_WerRegisterFile);
    register_api("KERNEL32.dll", "WerRegisterFile", (void*)THETAZ_WerRegisterFile);
    register_api("kernelbase.dll", "WerRegisterFile", (void*)THETAZ_WerRegisterFile);
    register_api("kernel32.dll", "WerRegisterRuntimeExceptionModule", (void*)THETAZ_WerRegisterRuntimeExceptionModule);
    register_api("KERNEL32.dll", "WerRegisterRuntimeExceptionModule", (void*)THETAZ_WerRegisterRuntimeExceptionModule);
    register_api("kernelbase.dll", "WerRegisterRuntimeExceptionModule", (void*)THETAZ_WerRegisterRuntimeExceptionModule);
    register_api("kernel32.dll", "WerUnregisterRuntimeExceptionModule", (void*)THETAZ_WerUnregisterRuntimeExceptionModule);
    register_api("KERNEL32.dll", "WerUnregisterRuntimeExceptionModule", (void*)THETAZ_WerUnregisterRuntimeExceptionModule);
    register_api("kernelbase.dll", "WerUnregisterRuntimeExceptionModule", (void*)THETAZ_WerUnregisterRuntimeExceptionModule);
    register_api("kernel32.dll", "Wow64DisableWow64FsRedirection", (void*)THETAZ_Wow64DisableWow64FsRedirection);
    register_api("KERNEL32.dll", "Wow64DisableWow64FsRedirection", (void*)THETAZ_Wow64DisableWow64FsRedirection);
    register_api("kernelbase.dll", "Wow64DisableWow64FsRedirection", (void*)THETAZ_Wow64DisableWow64FsRedirection);
    register_api("kernel32.dll", "Wow64RevertWow64FsRedirection", (void*)THETAZ_Wow64RevertWow64FsRedirection);
    register_api("KERNEL32.dll", "Wow64RevertWow64FsRedirection", (void*)THETAZ_Wow64RevertWow64FsRedirection);
    register_api("kernelbase.dll", "Wow64RevertWow64FsRedirection", (void*)THETAZ_Wow64RevertWow64FsRedirection);
    register_api("kernel32.dll", "IsWow64Process2", (void*)THETAZ_IsWow64Process2);
    register_api("KERNEL32.dll", "IsWow64Process2", (void*)THETAZ_IsWow64Process2);
    register_api("kernelbase.dll", "IsWow64Process2", (void*)THETAZ_IsWow64Process2);
    register_api("kernel32.dll", "K32EnumProcessModules", (void*)THETAZ_K32EnumProcessModules);
    register_api("KERNEL32.dll", "K32EnumProcessModules", (void*)THETAZ_K32EnumProcessModules);
    register_api("kernelbase.dll", "K32EnumProcessModules", (void*)THETAZ_K32EnumProcessModules);
    register_api("kernel32.dll", "K32GetModuleBaseNameA", (void*)THETAZ_K32GetModuleBaseNameA);
    register_api("KERNEL32.dll", "K32GetModuleBaseNameA", (void*)THETAZ_K32GetModuleBaseNameA);
    register_api("kernelbase.dll", "K32GetModuleBaseNameA", (void*)THETAZ_K32GetModuleBaseNameA);
    register_api("kernel32.dll", "K32GetModuleBaseNameW", (void*)THETAZ_K32GetModuleBaseNameW);
    register_api("KERNEL32.dll", "K32GetModuleBaseNameW", (void*)THETAZ_K32GetModuleBaseNameW);
    register_api("kernelbase.dll", "K32GetModuleBaseNameW", (void*)THETAZ_K32GetModuleBaseNameW);
    register_api("kernel32.dll", "K32GetModuleInformation", (void*)THETAZ_K32GetModuleInformation);
    register_api("KERNEL32.dll", "K32GetModuleInformation", (void*)THETAZ_K32GetModuleInformation);
    register_api("kernelbase.dll", "K32GetModuleInformation", (void*)THETAZ_K32GetModuleInformation);
    register_api("kernel32.dll", "RtlAddFunctionTable", (void*)THETAZ_RtlAddFunctionTable);
    register_api("KERNEL32.dll", "RtlAddFunctionTable", (void*)THETAZ_RtlAddFunctionTable);
    register_api("kernelbase.dll", "RtlAddFunctionTable", (void*)THETAZ_RtlAddFunctionTable);
    register_api("kernel32.dll", "RtlDeleteFunctionTable", (void*)THETAZ_RtlDeleteFunctionTable);
    register_api("KERNEL32.dll", "RtlDeleteFunctionTable", (void*)THETAZ_RtlDeleteFunctionTable);
    register_api("kernelbase.dll", "RtlDeleteFunctionTable", (void*)THETAZ_RtlDeleteFunctionTable);
    register_api("kernel32.dll", "RtlRestoreContext", (void*)THETAZ_RtlRestoreContext);
    register_api("KERNEL32.dll", "RtlRestoreContext", (void*)THETAZ_RtlRestoreContext);
    register_api("kernel32.dll", "CreateActCtxW", (void*)THETAZ_CreateActCtxW);
    register_api("KERNEL32.dll", "CreateActCtxW", (void*)THETAZ_CreateActCtxW);
    register_api("kernelbase.dll", "CreateActCtxW", (void*)THETAZ_CreateActCtxW);
    register_api("kernel32.dll", "ActivateActCtx", (void*)THETAZ_ActivateActCtx);
    register_api("KERNEL32.dll", "ActivateActCtx", (void*)THETAZ_ActivateActCtx);
    register_api("kernelbase.dll", "ActivateActCtx", (void*)THETAZ_ActivateActCtx);
    register_api("kernel32.dll", "DeactivateActCtx", (void*)THETAZ_DeactivateActCtx);
    register_api("KERNEL32.dll", "DeactivateActCtx", (void*)THETAZ_DeactivateActCtx);
    register_api("kernelbase.dll", "DeactivateActCtx", (void*)THETAZ_DeactivateActCtx);
    register_api("kernel32.dll", "ReleaseActCtx", (void*)THETAZ_ReleaseActCtx);
    register_api("KERNEL32.dll", "ReleaseActCtx", (void*)THETAZ_ReleaseActCtx);
    register_api("kernelbase.dll", "ReleaseActCtx", (void*)THETAZ_ReleaseActCtx);
    register_api("kernel32.dll", "GetCurrentActCtx", (void*)THETAZ_GetCurrentActCtx);
    register_api("KERNEL32.dll", "GetCurrentActCtx", (void*)THETAZ_GetCurrentActCtx);
    register_api("kernelbase.dll", "GetCurrentActCtx", (void*)THETAZ_GetCurrentActCtx);
    register_api("kernel32.dll", "FindActCtxSectionStringW", (void*)THETAZ_FindActCtxSectionStringW);
    register_api("KERNEL32.dll", "FindActCtxSectionStringW", (void*)THETAZ_FindActCtxSectionStringW);
    register_api("kernelbase.dll", "FindActCtxSectionStringW", (void*)THETAZ_FindActCtxSectionStringW);
    register_api("kernel32.dll", "QueryActCtxW", (void*)THETAZ_QueryActCtxW);
    register_api("KERNEL32.dll", "QueryActCtxW", (void*)THETAZ_QueryActCtxW);
    register_api("kernelbase.dll", "QueryActCtxW", (void*)THETAZ_QueryActCtxW);
    register_api("kernelbase.dll", "CharLowerA", (void*)THETAZ_CharLowerA);
    register_api("kernelbase.dll", "CharLowerW", (void*)THETAZ_CharLowerW);
    register_api("kernelbase.dll", "ChrCmpIA", (void*)THETAZ_ChrCmpIA);
    register_api("kernelbase.dll", "ChrCmpIW", (void*)THETAZ_ChrCmpIW);
    register_api("kernelbase.dll", "IsCharBlankW", (void*)THETAZ_IsCharBlankW);
    register_api("kernelbase.dll", "IsCharCntrlW", (void*)THETAZ_IsCharCntrlW);
    register_api("kernelbase.dll", "IsCharDigitW", (void*)THETAZ_IsCharDigitW);
    register_api("kernelbase.dll", "IsCharPunctW", (void*)THETAZ_IsCharPunctW);
    register_api("kernelbase.dll", "IsCharSpaceA", (void*)THETAZ_IsCharSpaceA);
    register_api("kernelbase.dll", "IsCharSpaceW", (void*)THETAZ_IsCharSpaceW);
    register_api("kernelbase.dll", "IsCharXDigitW", (void*)THETAZ_IsCharXDigitW);
    register_api("kernelbase.dll", "IsInternetESCEnabled", (void*)THETAZ_IsInternetESCEnabled);
    register_api("kernelbase.dll", "GetAce", (void*)THETAZ_GetAce);
    register_api("kernelbase.dll", "GetSecurityDescriptorControl", (void*)THETAZ_GetSecurityDescriptorControl);
    register_api("kernelbase.dll", "GetSecurityDescriptorGroup", (void*)THETAZ_GetSecurityDescriptorGroup);
    register_api("kernelbase.dll", "GetSecurityDescriptorOwner", (void*)THETAZ_GetSecurityDescriptorOwner);
    register_api("kernelbase.dll", "GetSecurityDescriptorSacl", (void*)THETAZ_GetSecurityDescriptorSacl);
    register_api("kernelbase.dll", "InitializeSecurityDescriptor", (void*)THETAZ_InitializeSecurityDescriptor);
    register_api("kernelbase.dll", "IsValidAcl", (void*)THETAZ_IsValidAcl);
    register_api("kernelbase.dll", "HashData", (void*)THETAZ_HashData);
    register_api("kernelbase.dll", "GetFileVersionInfoSizeW", (void*)THETAZ_GetFileVersionInfoSizeW);
    register_api("kernelbase.dll", "GetFileVersionInfoW", (void*)THETAZ_GetFileVersionInfoW);
    register_api("kernelbase.dll", "GetAcceptLanguagesA", (void*)THETAZ_GetAcceptLanguagesA);
    register_api("kernelbase.dll", "GetAcceptLanguagesW", (void*)THETAZ_GetAcceptLanguagesW);
    register_api("kernelbase.dll", "ParseURLA", (void*)THETAZ_ParseURLA);
    register_api("kernelbase.dll", "ParseURLW", (void*)THETAZ_ParseURLW);
    register_api("kernelbase.dll", "QISearch", (void*)THETAZ_QISearch);
    register_api("kernelbase.dll", "SHLoadIndirectString", (void*)THETAZ_SHLoadIndirectString);
    register_api("kernelbase.dll", "SHRegCreateUSKeyA", (void*)THETAZ_SHRegCreateUSKeyA);
    register_api("kernelbase.dll", "SHRegCreateUSKeyW", (void*)THETAZ_SHRegCreateUSKeyW);
    register_api("kernelbase.dll", "SHRegOpenUSKeyA", (void*)THETAZ_SHRegOpenUSKeyA);
    register_api("kernelbase.dll", "SHRegOpenUSKeyW", (void*)THETAZ_SHRegOpenUSKeyW);
    register_api("kernelbase.dll", "SHRegQueryUSValueA", (void*)THETAZ_SHRegQueryUSValueA);
    register_api("kernelbase.dll", "SHRegQueryUSValueW", (void*)THETAZ_SHRegQueryUSValueW);
    register_api("kernelbase.dll", "SHRegSetUSValueA", (void*)THETAZ_SHRegSetUSValueA);
    register_api("kernelbase.dll", "SHRegSetUSValueW", (void*)THETAZ_SHRegSetUSValueW);
    register_api("kernelbase.dll", "SHRegWriteUSValueA", (void*)THETAZ_SHRegWriteUSValueA);
    register_api("kernelbase.dll", "SHRegWriteUSValueW", (void*)THETAZ_SHRegWriteUSValueW);
    register_api("kernelbase.dll", "SHRegDeleteUSValueA", (void*)THETAZ_SHRegDeleteUSValueA);
    register_api("kernelbase.dll", "SHRegDeleteUSValueW", (void*)THETAZ_SHRegDeleteUSValueW);
    register_api("kernelbase.dll", "SHRegDeleteEmptyUSKeyA", (void*)THETAZ_SHRegDeleteEmptyUSKeyA);
    register_api("kernelbase.dll", "SHRegDeleteEmptyUSKeyW", (void*)THETAZ_SHRegDeleteEmptyUSKeyW);
    register_api("kernelbase.dll", "SHRegEnumUSKeyA", (void*)THETAZ_SHRegEnumUSKeyA);
    register_api("kernelbase.dll", "SHRegEnumUSKeyW", (void*)THETAZ_SHRegEnumUSKeyW);
    register_api("kernelbase.dll", "SHRegEnumUSValueA", (void*)THETAZ_SHRegEnumUSValueA);
    register_api("kernelbase.dll", "SHRegEnumUSValueW", (void*)THETAZ_SHRegEnumUSValueW);
    register_api("kernelbase.dll", "SHRegGetBoolUSValueA", (void*)THETAZ_SHRegGetBoolUSValueA);
    register_api("kernelbase.dll", "SHRegGetBoolUSValueW", (void*)THETAZ_SHRegGetBoolUSValueW);
    register_api("kernelbase.dll", "SHRegGetUSValueA", (void*)THETAZ_SHRegGetUSValueA);
    register_api("kernelbase.dll", "SHRegGetUSValueW", (void*)THETAZ_SHRegGetUSValueW);
    register_api("kernelbase.dll", "SHRegQueryInfoUSKeyA", (void*)THETAZ_SHRegQueryInfoUSKeyA);
    register_api("kernelbase.dll", "SHRegQueryInfoUSKeyW", (void*)THETAZ_SHRegQueryInfoUSKeyW);
    register_api("kernelbase.dll", "SHRegCloseUSKey", (void*)THETAZ_SHRegCloseUSKey);
    register_api("kernelbase.dll", "SHTruncateString", (void*)THETAZ_SHTruncateString);
    register_api("kernelbase.dll", "StrChrA", (void*)THETAZ_StrChrA);
    register_api("kernelbase.dll", "StrChrW", (void*)THETAZ_StrChrW);
    register_api("kernelbase.dll", "StrChrNW", (void*)THETAZ_StrChrNW);
    register_api("kernelbase.dll", "StrChrIA", (void*)THETAZ_StrChrIA);
    register_api("kernelbase.dll", "StrChrIW", (void*)THETAZ_StrChrIW);
    register_api("kernelbase.dll", "StrCmpA", (void*)THETAZ_StrCmpA);
    register_api("kernelbase.dll", "StrCmpW", (void*)THETAZ_StrCmpW);
    register_api("kernelbase.dll", "StrCmpCA", (void*)THETAZ_StrCmpCA);
    register_api("kernelbase.dll", "StrCmpCW", (void*)THETAZ_StrCmpCW);
    register_api("kernelbase.dll", "StrCmpICA", (void*)THETAZ_StrCmpICA);
    register_api("kernelbase.dll", "StrCmpICW", (void*)THETAZ_StrCmpICW);
    register_api("kernelbase.dll", "StrCmpNA", (void*)THETAZ_StrCmpNA);
    register_api("kernelbase.dll", "StrCmpNW", (void*)THETAZ_StrCmpNW);
    register_api("kernelbase.dll", "StrCmpNCA", (void*)THETAZ_StrCmpNCA);
    register_api("kernelbase.dll", "StrCmpNCW", (void*)THETAZ_StrCmpNCW);
    register_api("kernelbase.dll", "StrCmpNIA", (void*)THETAZ_StrCmpNIA);
    register_api("kernelbase.dll", "StrCmpNICW", (void*)THETAZ_StrCmpNICW);
    register_api("kernelbase.dll", "StrCmpLogicalW", (void*)THETAZ_StrCmpLogicalW);
    register_api("kernelbase.dll", "StrCpyNW", (void*)THETAZ_StrCpyNW);
    register_api("kernelbase.dll", "StrCSpnA", (void*)THETAZ_StrCSpnA);
    register_api("kernelbase.dll", "StrCSpnW", (void*)THETAZ_StrCSpnW);
    register_api("kernelbase.dll", "StrCSpnIA", (void*)THETAZ_StrCSpnIA);
    register_api("kernelbase.dll", "StrCSpnIW", (void*)THETAZ_StrCSpnIW);
    register_api("kernelbase.dll", "StrDupA", (void*)THETAZ_StrDupA);
    register_api("kernelbase.dll", "StrDupW", (void*)THETAZ_StrDupW);
    register_api("kernelbase.dll", "StrIsIntlEqualA", (void*)THETAZ_StrIsIntlEqualA);
    register_api("kernelbase.dll", "StrIsIntlEqualW", (void*)THETAZ_StrIsIntlEqualW);
    register_api("kernelbase.dll", "StrPBrkA", (void*)THETAZ_StrPBrkA);
    register_api("kernelbase.dll", "StrPBrkW", (void*)THETAZ_StrPBrkW);
    register_api("kernelbase.dll", "StrRChrA", (void*)THETAZ_StrRChrA);
    register_api("kernelbase.dll", "StrRChrW", (void*)THETAZ_StrRChrW);
    register_api("kernelbase.dll", "StrRChrIA", (void*)THETAZ_StrRChrIA);
    register_api("kernelbase.dll", "StrRChrIW", (void*)THETAZ_StrRChrIW);
    register_api("kernelbase.dll", "StrRStrIA", (void*)THETAZ_StrRStrIA);
    register_api("kernelbase.dll", "StrRStrIW", (void*)THETAZ_StrRStrIW);
    register_api("kernelbase.dll", "StrSpnA", (void*)THETAZ_StrSpnA);
    register_api("kernelbase.dll", "StrSpnW", (void*)THETAZ_StrSpnW);
    register_api("kernelbase.dll", "StrStrA", (void*)THETAZ_StrStrA);
    register_api("kernelbase.dll", "StrStrW", (void*)THETAZ_StrStrW);
    register_api("kernelbase.dll", "StrStrIA", (void*)THETAZ_StrStrIA);
    register_api("kernelbase.dll", "StrStrIW", (void*)THETAZ_StrStrIW);
    register_api("kernelbase.dll", "StrStrNW", (void*)THETAZ_StrStrNW);
    register_api("kernelbase.dll", "StrToIntA", (void*)THETAZ_StrToIntA);
    register_api("kernelbase.dll", "StrToIntW", (void*)THETAZ_StrToIntW);
    register_api("kernelbase.dll", "StrToInt64ExA", (void*)THETAZ_StrToInt64ExA);
    register_api("kernelbase.dll", "StrToInt64ExW", (void*)THETAZ_StrToInt64ExW);
    register_api("kernelbase.dll", "StrTrimA", (void*)THETAZ_StrTrimA);
    register_api("kernelbase.dll", "StrTrimW", (void*)THETAZ_StrTrimW);
    register_api("kernelbase.dll", "StrCatBuffA", (void*)THETAZ_StrCatBuffA);
    register_api("kernelbase.dll", "StrCatBuffW", (void*)THETAZ_StrCatBuffW);
    register_api("kernelbase.dll", "StrCatChainW", (void*)THETAZ_StrCatChainW);
    register_api("kernelbase.dll", "StrCpyNXA", (void*)THETAZ_StrCpyNXA);
    register_api("kernelbase.dll", "StrCpyNXW", (void*)THETAZ_StrCpyNXW);
    register_api("kernelbase.dll", "GetNumberFormatW", (void*)THETAZ_GetNumberFormatEx);
    register_api("shlwapi.dll", "PathAddBackslashA", (void*)THETAZ_PathAddBackslashA);
    register_api("shlwapi.dll", "PathAddBackslashW", (void*)THETAZ_PathAddBackslashW);
    register_api("shlwapi.dll", "PathAddExtensionA", (void*)THETAZ_PathAddExtensionA);
    register_api("shlwapi.dll", "PathAddExtensionW", (void*)THETAZ_PathAddExtensionW);
    register_api("shlwapi.dll", "PathAppendA", (void*)THETAZ_PathAppendA);
    register_api("shlwapi.dll", "PathAppendW", (void*)THETAZ_PathAppendW);
    register_api("shlwapi.dll", "PathCanonicalizeA", (void*)THETAZ_PathCanonicalizeA);
    register_api("shlwapi.dll", "PathCanonicalizeW", (void*)THETAZ_PathCanonicalizeW);
    register_api("shlwapi.dll", "PathCchAppend", (void*)THETAZ_PathCchAppend);
    register_api("shlwapi.dll", "PathCchFindExtension", (void*)THETAZ_PathCchFindExtension);
    register_api("shlwapi.dll", "PathCombineA", (void*)THETAZ_PathCombineA);
    register_api("shlwapi.dll", "PathCombineW", (void*)THETAZ_PathCombineW);
    register_api("shlwapi.dll", "PathCommonPrefixA", (void*)THETAZ_PathCommonPrefixA);
    register_api("shlwapi.dll", "PathCommonPrefixW", (void*)THETAZ_PathCommonPrefixW);
    register_api("shlwapi.dll", "PathCreateFromUrlA", (void*)THETAZ_PathCreateFromUrlA);
    register_api("shlwapi.dll", "PathCreateFromUrlW", (void*)THETAZ_PathCreateFromUrlW);
    register_api("shlwapi.dll", "PathCreateFromUrlAlloc", (void*)THETAZ_PathCreateFromUrlAlloc);
    register_api("shlwapi.dll", "PathFileExistsA", reinterpret_cast<void*>(THETAZ_PathFileExistsA));
    register_api("shlwapi.dll", "PathFileExistsW", reinterpret_cast<void*>(THETAZ_PathFileExistsW));
    register_api("shlwapi.dll", "PathFindExtensionA", reinterpret_cast<void*>(THETAZ_PathFindExtensionA));
    register_api("shlwapi.dll", "PathFindExtensionW", (void*)(wchar_t*(*)(const wchar_t*))THETAZ_PathFindExtensionW);
    register_api("shlwapi.dll", "PathFindFileNameA", reinterpret_cast<void*>(THETAZ_PathFindFileNameA));
    register_api("shlwapi.dll", "PathFindFileNameW", (void*)(wchar_t*(*)(const wchar_t*))THETAZ_PathFindFileNameW);
    register_api("shlwapi.dll", "PathFindNextComponentW", (void*)(wchar_t*(*)(const wchar_t*))THETAZ_PathFindNextComponentW);
    register_api("shlwapi.dll", "PathGetArgsA", reinterpret_cast<void*>(THETAZ_PathGetArgsA));
    register_api("shlwapi.dll", "PathGetArgsW", (void*)(wchar_t*(*)(const wchar_t*))THETAZ_PathGetArgsW);
    register_api("shlwapi.dll", "PathGetCharTypeA", reinterpret_cast<void*>(THETAZ_PathGetCharTypeA));
    register_api("shlwapi.dll", "PathGetCharTypeW", (void*)(int(*)(const wchar_t*,DWORD*))THETAZ_PathGetCharTypeW);
    register_api("shlwapi.dll", "PathGetDriveNumberA", reinterpret_cast<void*>(THETAZ_PathGetDriveNumberA));
    register_api("shlwapi.dll", "PathGetDriveNumberW", (void*)(int(*)(const wchar_t*))THETAZ_PathGetDriveNumberW);
    register_api("shlwapi.dll", "PathIsFileSpecA", reinterpret_cast<void*>(THETAZ_PathIsFileSpecA));
    register_api("shlwapi.dll", "PathIsFileSpecW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsFileSpecW);
    register_api("shlwapi.dll", "PathIsLFNFileSpecA", reinterpret_cast<void*>(THETAZ_PathIsLFNFileSpecA));
    register_api("shlwapi.dll", "PathIsLFNFileSpecW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsLFNFileSpecW);
    register_api("shlwapi.dll", "PathIsPrefixA", reinterpret_cast<void*>(THETAZ_PathIsPrefixA));
    register_api("shlwapi.dll", "PathIsPrefixW", (void*)(BOOL(*)(const wchar_t*,const wchar_t*))THETAZ_PathIsPrefixW);
    register_api("shlwapi.dll", "PathIsRelativeA", reinterpret_cast<void*>(THETAZ_PathIsRelativeA));
    register_api("shlwapi.dll", "PathIsRelativeW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsRelativeW);
    register_api("shlwapi.dll", "PathIsRootA", reinterpret_cast<void*>(THETAZ_PathIsRootA));
    register_api("shlwapi.dll", "PathIsRootW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsRootW);
    register_api("shlwapi.dll", "PathIsSameRootA", reinterpret_cast<void*>(THETAZ_PathIsSameRootA));
    register_api("shlwapi.dll", "PathIsSameRootW", (void*)(BOOL(*)(const wchar_t*,const wchar_t*))THETAZ_PathIsSameRootW);
    register_api("shlwapi.dll", "PathIsUNCA", reinterpret_cast<void*>(THETAZ_PathIsUNCA));
    register_api("shlwapi.dll", "PathIsUNCW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsUNCW);
    register_api("shlwapi.dll", "PathIsUNCServerA", (void*)(BOOL(*)(const char*))THETAZ_PathIsUNCServerA);
    register_api("shlwapi.dll", "PathIsUNCServerW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsUNCServerW);
    register_api("shlwapi.dll", "PathIsUNCServerShareA", (void*)(BOOL(*)(const char*))THETAZ_PathIsUNCServerShareA);
    register_api("shlwapi.dll", "PathIsUNCServerShareW", (void*)(BOOL(*)(const wchar_t*))THETAZ_PathIsUNCServerShareW);
    register_api("shlwapi.dll", "PathIsURLA", (void*)(BOOL(*)(LPCSTR))THETAZ_PathIsURLA);
    register_api("shlwapi.dll", "PathIsURLW", (void*)(BOOL(*)(LPCWSTR))THETAZ_PathIsURLW);
    register_api("shlwapi.dll", "PathIsValidCharA", (void*)(BOOL(*)(LPCSTR,int))THETAZ_PathIsValidCharA);
    register_api("shlwapi.dll", "PathIsValidCharW", (void*)(BOOL(*)(LPCWSTR,int))THETAZ_PathIsValidCharW);
    register_api("shlwapi.dll", "PathMatchSpecA", (void*)(BOOL(*)(LPCSTR,LPCSTR))THETAZ_PathMatchSpecA);
    register_api("shlwapi.dll", "PathMatchSpecW", (void*)(BOOL(*)(LPCWSTR,LPCWSTR))THETAZ_PathMatchSpecW);
    register_api("shlwapi.dll", "PathMatchSpecExA", (void*)(BOOL(*)(LPCSTR,LPCSTR,DWORD,void*))THETAZ_PathMatchSpecExA);
    register_api("shlwapi.dll", "PathMatchSpecExW", (void*)(HRESULT(*)(LPCWSTR,LPCWSTR,DWORD,void*))THETAZ_PathMatchSpecExW);
    register_api("shlwapi.dll", "PathParseIconLocationA", (void*)(int(*)(LPSTR))THETAZ_PathParseIconLocationA);
    register_api("shlwapi.dll", "PathParseIconLocationW", (void*)(int(*)(LPWSTR))THETAZ_PathParseIconLocationW);
    register_api("shlwapi.dll", "PathQuoteSpacesA", (void*)(void(*)(LPSTR))THETAZ_PathQuoteSpacesA);
    register_api("shlwapi.dll", "PathQuoteSpacesW", (void*)(void(*)(LPWSTR))THETAZ_PathQuoteSpacesW);
    register_api("shlwapi.dll", "PathRelativePathToA", (void*)(BOOL(*)(LPSTR,LPCSTR,DWORD,LPCSTR,DWORD))THETAZ_PathRelativePathToA);
    register_api("shlwapi.dll", "PathRelativePathToW", (void*)(BOOL(*)(LPWSTR,LPCWSTR,DWORD,LPCWSTR,DWORD))THETAZ_PathRelativePathToW);
    register_api("shlwapi.dll", "PathRemoveBackslashA", (void*)(void(*)(LPSTR))THETAZ_PathRemoveBackslashA);
    register_api("shlwapi.dll", "PathRemoveBackslashW", (void*)(void(*)(LPWSTR))THETAZ_PathRemoveBackslashW);
    register_api("shlwapi.dll", "PathRemoveBlanksA", (void*)(void(*)(LPSTR))THETAZ_PathRemoveBlanksA);
    register_api("shlwapi.dll", "PathRemoveBlanksW", (void*)(void(*)(LPWSTR))THETAZ_PathRemoveBlanksW);
    register_api("shlwapi.dll", "PathRemoveExtensionA", (void*)(void(*)(LPSTR))THETAZ_PathRemoveExtensionA);
    register_api("shlwapi.dll", "PathRemoveExtensionW", (void*)(void(*)(LPWSTR))THETAZ_PathRemoveExtensionW);
    register_api("shlwapi.dll", "PathRemoveFileSpecA", (void*)(void(*)(LPSTR))THETAZ_PathRemoveFileSpecA);
    register_api("shlwapi.dll", "PathRemoveFileSpecW", (void*)(void(*)(LPCWSTR))THETAZ_PathRemoveFileSpecW);
    register_api("shlwapi.dll", "PathRenameExtensionA", (void*)(BOOL(*)(LPSTR,LPCSTR))THETAZ_PathRenameExtensionA);
    register_api("shlwapi.dll", "PathRenameExtensionW", (void*)(BOOL(*)(LPWSTR,LPCWSTR))THETAZ_PathRenameExtensionW);
    register_api("shlwapi.dll", "PathSearchAndQualifyA", (void*)(BOOL(*)(LPCSTR,LPSTR,UINT*))THETAZ_PathSearchAndQualifyA);
    register_api("shlwapi.dll", "PathSearchAndQualifyW", (void*)(BOOL(*)(LPCWSTR,LPWSTR,UINT*))THETAZ_PathSearchAndQualifyW);
    register_api("shlwapi.dll", "PathSkipRootA", (void*)(BOOL(*)(LPSTR))THETAZ_PathSkipRootA);
    register_api("shlwapi.dll", "PathSkipRootW", (void*)(BOOL(*)(LPWSTR))THETAZ_PathSkipRootW);
    register_api("shlwapi.dll", "PathStripPathA", (void*)(void(*)(LPSTR))THETAZ_PathStripPathA);
    register_api("shlwapi.dll", "PathStripPathW", (void*)(void(*)(LPWSTR))THETAZ_PathStripPathW);
    register_api("shlwapi.dll", "PathStripToRootA", (void*)(BOOL(*)(LPSTR))THETAZ_PathStripToRootA);
    register_api("shlwapi.dll", "PathStripToRootW", (void*)(BOOL(*)(LPWSTR))THETAZ_PathStripToRootW);
    register_api("shlwapi.dll", "PathUnExpandEnvStringsA", (void*)(BOOL(*)(LPCSTR,LPSTR,UINT*))THETAZ_PathUnExpandEnvStringsA);
    register_api("shlwapi.dll", "PathUnExpandEnvStringsW", (void*)(BOOL(*)(LPCWSTR,LPWSTR,UINT*))THETAZ_PathUnExpandEnvStringsW);
    register_api("shlwapi.dll", "PathUnquoteSpacesA", (void*)(void(*)(LPSTR))THETAZ_PathUnquoteSpacesA);
    register_api("shlwapi.dll", "PathUnquoteSpacesW", (void*)(void(*)(LPWSTR))THETAZ_PathUnquoteSpacesW);
    register_api("shlwapi.dll", "UrlApplySchemeA", (void*)THETAZ_UrlApplySchemeA);
    register_api("shlwapi.dll", "UrlApplySchemeW", (void*)THETAZ_UrlApplySchemeW);
    register_api("shlwapi.dll", "UrlCombineA", (void*)THETAZ_UrlCombineA);
    register_api("shlwapi.dll", "UrlCombineW", (void*)THETAZ_UrlCombineW);
    register_api("shlwapi.dll", "UrlCreateFromPathA", (void*)THETAZ_UrlCreateFromPathA);
    register_api("shlwapi.dll", "UrlCreateFromPathW", (void*)THETAZ_UrlCreateFromPathW);
    register_api("shlwapi.dll", "UrlEscapeA", (void*)THETAZ_UrlEscapeA);
    register_api("shlwapi.dll", "UrlEscapeW", (void*)THETAZ_UrlEscapeW);
    register_api("shlwapi.dll", "UrlFixupA", (void*)THETAZ_UrlFixupA);
    register_api("shlwapi.dll", "UrlFixupW", (void*)THETAZ_UrlFixupW);
    register_api("shlwapi.dll", "UrlGetPartA", (void*)THETAZ_UrlGetPartA);
    register_api("shlwapi.dll", "UrlGetPartW", (void*)THETAZ_UrlGetPartW);
    register_api("shlwapi.dll", "UrlGetSchemeA", (void*)THETAZ_UrlGetSchemeA);
    register_api("shlwapi.dll", "UrlGetSchemeW", (void*)THETAZ_UrlGetSchemeW);
    register_api("shlwapi.dll", "UrlHashA", (void*)THETAZ_UrlHashA);
    register_api("shlwapi.dll", "UrlHashW", (void*)THETAZ_UrlHashW);
    register_api("shlwapi.dll", "UrlIsA", (void*)THETAZ_UrlIsA);
    register_api("shlwapi.dll", "UrlIsW", (void*)THETAZ_UrlIsW);
    register_api("shlwapi.dll", "UrlIsNoHistoryA", (void*)THETAZ_UrlIsNoHistoryA);
    register_api("shlwapi.dll", "UrlIsNoHistoryW", (void*)THETAZ_UrlIsNoHistoryW);
    register_api("shlwapi.dll", "UrlIsOpaqueA", (void*)THETAZ_UrlIsOpaqueA);
    register_api("shlwapi.dll", "UrlIsOpaqueW", (void*)THETAZ_UrlIsOpaqueW);
    register_api("shlwapi.dll", "UrlUnescapeA", (void*)THETAZ_UrlUnescapeA);
    register_api("shlwapi.dll", "UrlUnescapeW", (void*)THETAZ_UrlUnescapeW);
    register_api("shlwapi.dll", "UrlUnescapeInPlaceA", (void*)THETAZ_UrlUnescapeInPlaceA);
    register_api("shlwapi.dll", "UrlUnescapeInPlaceW", (void*)THETAZ_UrlUnescapeInPlaceW);
    register_api("shlwapi.dll", "PathFindNextComponentA", (void*)THETAZ_PathFindNextComponentA);
    register_api("shlwapi.dll", "UrlCanonicalizeA", (void*)THETAZ_UrlCanonicalizeA);
    register_api("shlwapi.dll", "UrlCanonicalizeW", (void*)THETAZ_UrlCanonicalizeW);
    register_api("shlwapi.dll", "UrlCompareA", (void*)THETAZ_UrlCompareA);
    register_api("shlwapi.dll", "UrlCompareW", (void*)THETAZ_UrlCompareW);
    register_api("shlwapi.dll", "UrlGetLocationA", (void*)THETAZ_UrlGetLocationA);
    register_api("shlwapi.dll", "UrlGetLocationW", (void*)THETAZ_UrlGetLocationW);
    register_api("shlwapi.dll", "StrCmpNICA", (void*)THETAZ_StrCmpNICA);
    register_api("shlwapi.dll", "StrStrNIA", (void*)THETAZ_StrStrNIA);
    register_api("shlwapi.dll", "StrStrNIW", (void*)THETAZ_StrStrNIW);
    register_api("shlwapi.dll", "StrToIntExA", (void*)THETAZ_StrToIntExA);
    register_api("shlwapi.dll", "StrToIntExW", (void*)THETAZ_StrToIntExW);
    // kernel32
    register_api("kernel32.dll", "EnumResourceNamesW", (void*)THETAZ_EnumResourceNamesW);
    register_api("KERNEL32.dll", "EnumResourceNamesW", (void*)THETAZ_EnumResourceNamesW);
    register_api("kernel32.dll", "GetSystemWindowsDirectoryW", (void*)THETAZ_GetSystemWindowsDirectoryW);
    register_api("KERNEL32.dll", "GetSystemWindowsDirectoryW", (void*)THETAZ_GetSystemWindowsDirectoryW);
    register_api("kernel32.dll", "LoadStringW", (void*)THETAZ_LoadStringW);
    register_api("KERNEL32.dll", "LoadStringW", (void*)THETAZ_LoadStringW);
    auto queue_user_apc_fn = static_cast<BOOL (WINAPI_CALL*)(void*, void*, ULONG_PTR)>(THETAZ_QueueUserAPC);
    register_api("kernel32.dll", "QueueUserAPC", reinterpret_cast<void*>(queue_user_apc_fn));
    auto queue_user_apc_fn2 = static_cast<BOOL (WINAPI_CALL*)(void*, void*, ULONG_PTR)>(THETAZ_QueueUserAPC);
    register_api("KERNEL32.dll", "QueueUserAPC", reinterpret_cast<void*>(queue_user_apc_fn2));
    auto queue_user_work_item_fn_1 = static_cast<BOOL (WINAPI_CALL*)(void*, void*, ULONG)>(THETAZ_QueueUserWorkItem);
    register_api("kernel32.dll", "QueueUserWorkItem", reinterpret_cast<void*>(queue_user_work_item_fn_1));
    
    // kernelbase
    auto set_thread_description_fn = static_cast<HRESULT (WINAPI_CALL*)(HANDLE, PCWSTR)>(THETAZ_SetThreadDescription);
    register_api("kernelbase.dll", "SetThreadDescription", reinterpret_cast<void*>(set_thread_description_fn));
    auto load_string_a_fn = static_cast<int (WINAPI_CALL*)(HINSTANCE, UINT, LPSTR, int)>(THETAZ_LoadStringA);
    register_api("kernel32.dll", "LoadStringA", reinterpret_cast<void*>(load_string_a_fn));
    
    auto find_resource_ex_w_fn = static_cast<void* (WINAPI_CALL*)(void*, const wchar_t*, const wchar_t*, unsigned short)>(THETAZ_FindResourceExW);
    register_api("kernel32.dll", "FindResourceExW", reinterpret_cast<void*>(find_resource_ex_w_fn));
    
    auto i64tow_s_fn = static_cast<errno_t (WINAPI_CALL*)(long long, wchar_t*, size_t, int)>(THETAZ__i64tow_s);
    register_api("api-ms-win-crt-convert-l1-1-0.dll", "_i64tow_s", reinterpret_cast<void*>(i64tow_s_fn));
    auto get_computer_name_ex_a_fn = static_cast<BOOL (WINAPI_CALL*)(int, char*, LPDWORD)>(THETAZ_GetComputerNameExA);
    register_api("kernel32.dll", "GetComputerNameExA", reinterpret_cast<void*>(get_computer_name_ex_a_fn));
    auto get_computer_name_ex_a_fn2 = static_cast<BOOL (WINAPI_CALL*)(int, char*, LPDWORD)>(THETAZ_GetComputerNameExA);
    register_api("KERNEL32.dll", "GetComputerNameExA", reinterpret_cast<void*>(get_computer_name_ex_a_fn2));
    auto atof_fn = static_cast<double (WINAPI_CALL*)(const char*)>(THETAZ_atof);
    register_api("api-ms-win-crt-convert-l1-1-0.dll", "atof", reinterpret_cast<void*>(atof_fn));

    // user32 missing
    register_api("user32.dll", "MsgWaitForMultipleObjects", (void*)THETAZ_MsgWaitForMultipleObjects);
    register_api("user32.dll", "DestroyMenu", (void*)THETAZ_DestroyMenu);
    register_api("user32.dll", "EnumChildWindows", (void*)THETAZ_EnumChildWindows);
    register_api("user32.dll", "LoadCursorA", (void*)THETAZ_LoadCursorA);
    register_api("user32.dll", "UnregisterClassA", (void*)THETAZ_UnregisterClassA);
    register_api("user32.dll", "CharLowerW", (void*)THETAZ_CharLowerW);

    // ws2_32 missing
    register_api("ws2_32.dll", "FreeAddrInfoW", (void*)THETAZ_FreeAddrInfoW);
    register_api("ws2_32.dll", "freeaddrinfo", (void*)THETAZ_freeaddrinfo);
    register_api("ws2_32.dll", "getaddrinfo", (void*)THETAZ_getaddrinfo);

    // kernel32 additional
    register_api("kernel32.dll", "FindNextChangeNotification", (void*)THETAZ_FindNextChangeNotification);
    register_api("kernel32.dll", "fputs", (void*)THETAZ_fputs);
    register_api("kernel32.dll", "fgets", (void*)THETAZ_fgets);

    // ucrt math (registered in ucrt.cpp but also needed here for kernel32/msvcrt)
    register_api("ws2_32.dll", "getaddrinfo", (void*)THETAZ_getaddrinfo);
    register_api("ws2_32.dll", "FreeAddrInfoW", (void*)THETAZ_FreeAddrInfoW);
    register_api("ws2_32.dll", "freeaddrinfo", (void*)THETAZ_freeaddrinfo);
}

} // namespace ThetaZ
