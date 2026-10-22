// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include <sys/socket.h>

namespace ThetaZ {

// Windows drive type constants
constexpr UINT DRIVE_FIXED = 3;

// Windows socket constants
constexpr SOCKET INVALID_SOCKET = static_cast<SOCKET>(~0);

// WINWORD unresolved function implementations
HRESULT WINAPI_CALL THETAZ_ResolveDelayLoadedAPI(void* pDescriptor) { return 0; }

ATOM WINAPI_CALL THETAZ_RegisterClassExW(const void* lpwcx) { return 1; }

DWORD WINAPI_CALL THETAZ_MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD dwFlags) { return 0; }

int WINAPI_CALL THETAZ_GetAddrInfoW(const wchar_t* pNodeName, const wchar_t* pServiceName, const void* pHints, void** ppResult) { return 0; }

BOOL WINAPI_CALL THETAZ_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) { return TRUE; }

wchar_t* WINAPI_CALL THETAZ__wgetenv(const wchar_t* varname) { return nullptr; }


HRESULT WINAPI_CALL THETAZ_SHGetSpecialFolderLocation(HWND hwnd, int csidl, void** ppidl) { return 0; }

HRESULT WINAPI_CALL THETAZ_SHGetFileInfoW(LPCWSTR pszPath, DWORD dwFileAttributes, void* psfi, UINT cbFileInfo, UINT uFlags) { return 0; }

int WINAPI_CALL THETAZ_SHFileOperationW(void* lpFileOp) { return 0; }

HRESULT WINAPI_CALL THETAZ_SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription) { return 0; }

BOOL WINAPI_CALL THETAZ_SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString) { return TRUE; }

LRESULT WINAPI_CALL THETAZ_SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult) { return 0; }

LRESULT WINAPI_CALL THETAZ_SendMessageTimeoutA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult) { return 0; }

HANDLE WINAPI_CALL THETAZ_RemovePropA(HWND hWnd, LPCSTR lpString) { return nullptr; }

UINT WINAPI_CALL THETAZ_RegisterWindowMessageA(LPCSTR lpString) { return 1; }

UINT WINAPI_CALL THETAZ_RegisterClipboardFormatW(LPCWSTR lpszFormat) { return 1; }

UINT WINAPI_CALL THETAZ_RegisterClipboardFormatA(LPCSTR lpszFormat) { return 1; }

HMONITOR WINAPI_CALL THETAZ_MonitorFromWindow(HWND hwnd, DWORD dwFlags) { return nullptr; }

HMONITOR WINAPI_CALL THETAZ_MonitorFromPoint(POINT pt, DWORD dwFlags) { return nullptr; }


BOOL WINAPI_CALL THETAZ_InitializeSRWLock(PSRWLOCK SRWLock) { return TRUE; }

HANDLE WINAPI_CALL THETAZ_GetPropA(HWND hWnd, LPCSTR lpString) { return nullptr; }

int WINAPI_CALL THETAZ_GetNameInfoW(const SOCKADDR* pSockaddr, socklen_t SockaddrLength, PWCHAR pNodeBuffer, DWORD NodeBufferSize, PWCHAR pServiceBuffer, DWORD ServiceBufferSize, INT Flags) { return 0; }

BOOL WINAPI_CALL THETAZ_GetMenuItemInfoW(HWND hMenu, UINT item, BOOL fByPosition, void* lpmii) { return FALSE; }

UINT WINAPI_CALL THETAZ_GetDriveTypeA(LPCSTR lpRootPathName) { return DRIVE_FIXED; }

BOOL WINAPI_CALL THETAZ_GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize) { return TRUE; }

DWORD WINAPI_CALL THETAZ_GetClassLongPtrW(HWND hWnd, int nIndex) { return 0; }

BOOL WINAPI_CALL THETAZ_GetClassInfoW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSW lpWndClass) { return FALSE; }

BOOL WINAPI_CALL THETAZ_EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable) { return TRUE; }

int WINAPI_CALL THETAZ__dclass(int c) { return 0; }

LPCSTR WINAPI_CALL THETAZ_CharNextA(LPCSTR lpsz) { return lpsz; }

int WINAPI_CALL THETAZ__wsopen_s(int* pfh, const wchar_t* filename, int oflag, int shflag, int pmode) { return 0; }

SOCKET WINAPI_CALL THETAZ_WSASocketW(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo, GROUP g, DWORD dwFlags) { return INVALID_SOCKET; }

int WINAPI_CALL THETAZ_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) { return 0; }

BOOL WINAPI_CALL THETAZ___WSAFDIsSet(SOCKET fd, void* pfdset) { return FALSE; }

int WINAPI_CALL THETAZ__wrmdir(const wchar_t* dirname) { return 0; }

HRESULT WINAPI_CALL THETAZ_WriteFmtUserTypeStg(void* pstg, void* pfmtetc, DWORD dwUserType, LPWSTR lpszUserType) { return 0; }

HRESULT WINAPI_CALL THETAZ_WriteClassStm(void* pstg, REFCLSID rclsid) { return 0; }

HRESULT WINAPI_CALL THETAZ_WriteClassStg(void* pstg, REFCLSID rclsid) { return 0; }

} // namespace ThetaZ