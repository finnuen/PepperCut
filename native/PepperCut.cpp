// ============================================================================
// PepperCut.cpp — Pure 100% C++ & Win32 Standalone Background File/Folder Unlocker
// Target OS: Windows 10 x64 (Subsystem: WINDOWS / wWinMain)
// ============================================================================

#include "PepperCutCommon.h"
#include <winternl.h>

#define WM_TRAYICON             (WM_APP + 1)
#define WM_PEPPERCUT_NOTIFY     (WM_APP + 2)
#define ID_TRAY_ICON            1001
#define IDM_TRAY_EXIT           2001
#define IDM_TRAY_IGNORE_WIN     2002
#define IDM_TRAY_START_BOOT     2003
#define IDI_PEPPERCUT_ICON      101
#define IDR_SHELL_DLL           200

#define COPYDATA_NOTIFY_START        1
#define COPYDATA_NOTIFY_FILE_DISC    2
#define COPYDATA_NOTIFY_FOLDER_DISC  3
#define COPYDATA_NOTIFY_BOOT_DEL     4
#define COPYDATA_NOTIFY_BOOT_CANCEL  5
#define COPYDATA_NOTIFY_FAIL         6

static const wchar_t* kRunOnceRegKey    = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";

NOTIFYICONDATAW g_nid                 = {};
HICON           g_hAppIcon            = NULL;
HWND            g_hMainWnd            = NULL;
wchar_t         g_szExePath[MAX_PATH] = {};

// Undocumented NT structures for SystemExtendedHandleInformation (64)
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
    PVOID     Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG     GrantedAccess;
    USHORT    CreatorBackTraceIndex;
    USHORT    ObjectTypeIndex;
    ULONG     HandleAttributes;
    ULONG     Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
    ULONG_PTR                         NumberOfHandles;
    ULONG_PTR                         Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

typedef NTSTATUS(NTAPI* PFN_NtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

// ----------------------------------------------------------------------------
// Enable Debug Privilege (best-effort) so handle closure covers all processes
// ----------------------------------------------------------------------------
void EnableDebugPrivilege() {
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp = {};
        if (LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        }
        CloseHandle(hToken);
    }
}

// ----------------------------------------------------------------------------
// Load Embedded PepperCut Icon (with guaranteed GDI DIB fallback)
// Draws White Scissors cutting a White Cable on a Red (#DC2626) Background
// ----------------------------------------------------------------------------
HICON LoadOrCreatePepperCutIcon() {
    HMODULE hMod = GetModuleHandleW(NULL);
    HICON hRes = (HICON)LoadImageW(
        hMod,
        MAKEINTRESOURCEW(IDI_PEPPERCUT_ICON),
        IMAGE_ICON,
        32,
        32,
        LR_DEFAULTCOLOR
    );
    if (hRes) return hRes;

    hRes = LoadIconW(hMod, MAKEINTRESOURCEW(IDI_PEPPERCUT_ICON));
    if (hRes) return hRes;

    HBITMAP hColor = CreatePepperCutMenuBitmap(32);
    HBITMAP hMask  = CreateBitmap(32, 32, 1, 1, NULL);
    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.hbmMask  = hMask;
    ii.hbmColor = hColor;
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hColor);
    DeleteObject(hMask);
    return hIcon ? hIcon : LoadIconW(NULL, IDI_APPLICATION);
}

bool PathMatchesOrIsChild(const wchar_t* handlePathNorm, const wchar_t* targetNorm, bool isDirectory) {
    if (!handlePathNorm || !targetNorm || !*handlePathNorm || !*targetNorm) return false;
    if (wcscmp(handlePathNorm, targetNorm) == 0) return true;
    if (isDirectory) {
        size_t targetLen = wcslen(targetNorm);
        if (wcslen(handlePathNorm) > targetLen &&
            wcsncmp(handlePathNorm, targetNorm, targetLen) == 0 &&
            handlePathNorm[targetLen] == L'\\') {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Resolving a duplicated handle to a disk path.
//
// GetFinalPathNameByHandleW blocks in the kernel for as long as the underlying
// device takes to answer, which on a dead network share or an ejected drive is
// forever. The query therefore runs on a worker thread with a timeout.
//
// A worker that overstays its timeout is *abandoned, never terminated*:
// TerminateThread can leave the heap lock or a loader lock held inside this
// process and crash the next allocation. The abandoned thread keeps its handle
// and finishes whenever the kernel returns; it is simply never reused.
// ----------------------------------------------------------------------------
namespace {

constexpr DWORD  kPathQueryTimeoutMs = 150;
constexpr size_t kMaxPathWorkers     = 8;

struct PathWorker {
    HANDLE  hReq      = NULL;
    HANDLE  hDone     = NULL;
    HANDLE  hThread   = NULL;
    HANDLE  hTarget   = NULL;      // owned by the worker once hReq is signalled
    wchar_t szResult[MAX_PATH] = {};
    DWORD   dwLen     = 0;
    bool    abandoned = false;
};

// Fixed pool: no allocation, so <new> and the C++ runtime stay out of the exe.
PathWorker g_pathWorkers[kMaxPathWorkers];
size_t     g_pathWorkerCount = 0;

DWORD WINAPI HandlePathWorkerProc(LPVOID param) {
    PathWorker* w = static_cast<PathWorker*>(param);
    while (WaitForSingleObject(w->hReq, INFINITE) == WAIT_OBJECT_0) {
        HANDLE hTarget = w->hTarget;
        w->hTarget = NULL;
        w->dwLen = 0;
        w->szResult[0] = L'\0';
        if (hTarget) {
            if (GetFileType(hTarget) == FILE_TYPE_DISK) {
                DWORD cch = GetFinalPathNameByHandleW(hTarget, w->szResult, MAX_PATH, VOLUME_NAME_DOS);
                w->dwLen = (cch > 0 && cch < MAX_PATH) ? cch : 0;
            }
            CloseHandle(hTarget);
        }
        SetEvent(w->hDone);
    }
    return 0;
}

PathWorker* CreatePathWorker() {
    if (g_pathWorkerCount >= kMaxPathWorkers) return nullptr;

    PathWorker* w = &g_pathWorkers[g_pathWorkerCount];
    w->hReq  = CreateEventW(NULL, FALSE, FALSE, NULL);
    w->hDone = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (w->hReq && w->hDone) {
        w->hThread = CreateThread(NULL, 0, HandlePathWorkerProc, w, 0, NULL);
    }
    if (!w->hThread) {
        if (w->hReq)  CloseHandle(w->hReq);
        if (w->hDone) CloseHandle(w->hDone);
        return nullptr;
    }
    ++g_pathWorkerCount;
    return w;
}

PathWorker* AcquirePathWorker() {
    for (size_t i = 0; i < g_pathWorkerCount; ++i) {
        if (!g_pathWorkers[i].abandoned) return &g_pathWorkers[i];
    }
    return CreatePathWorker();
}

} // namespace

// Takes ownership of hLocal: the worker closes it, or keeps it open for ever
// when the query is abandoned. Callers must not close it afterwards.
bool SafeGetHandleDiskPath(HANDLE hLocal, wchar_t* outNormPath, size_t cchMax) {
    if (cchMax) outNormPath[0] = L'\0';

    PathWorker* w = AcquirePathWorker();
    if (!w) {
        // No worker left to hand the handle to: nobody else will close it.
        CloseHandle(hLocal);
        return false;
    }

    w->hTarget = hLocal;
    SetEvent(w->hReq);

    if (WaitForSingleObject(w->hDone, kPathQueryTimeoutMs) != WAIT_OBJECT_0) {
        // Stuck in device I/O. Retire the worker without killing it.
        w->abandoned = true;
        return false;
    }

    if (w->dwLen == 0) return false;
    NormalizePathCommon(w->szResult, outNormPath, cchMax);
    return outNormPath[0] != L'\0';
}

// ----------------------------------------------------------------------------
// Close remote handles matching target file/folder
// ----------------------------------------------------------------------------
bool CloseMatchingSystemHandles(const wchar_t* szTargetPath, bool isDirectory) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return false;
    PFN_NtQuerySystemInformation NtQuerySysInfo =
        reinterpret_cast<PFN_NtQuerySystemInformation>(GetProcAddress(hNtdll, "NtQuerySystemInformation"));
    if (!NtQuerySysInfo) return false;

    HANDLE hProbeSelf = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    ULONG bufSize = 1024 * 1024;
    BYTE* buffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, bufSize));
    if (!buffer) {
        if (hProbeSelf != INVALID_HANDLE_VALUE) CloseHandle(hProbeSelf);
        return false;
    }
    ULONG returnLen = 0;
    NTSTATUS status = 0;

    while ((status = NtQuerySysInfo(64, buffer, bufSize, &returnLen)) == (NTSTATUS)0xC0000004L) {
        ULONG newSize = returnLen ? (returnLen + 65536) : (bufSize * 2);
        if (newSize > 64 * 1024 * 1024) {
            HeapFree(GetProcessHeap(), 0, buffer);
            if (hProbeSelf != INVALID_HANDLE_VALUE) CloseHandle(hProbeSelf);
            return false;
        }
        BYTE* bigger = static_cast<BYTE*>(HeapReAlloc(GetProcessHeap(), 0, buffer, newSize));
        if (!bigger) {
            HeapFree(GetProcessHeap(), 0, buffer);
            if (hProbeSelf != INVALID_HANDLE_VALUE) CloseHandle(hProbeSelf);
            return false;
        }
        buffer = bigger;
        bufSize = newSize;
    }
    if (status < 0) {
        HeapFree(GetProcessHeap(), 0, buffer);
        if (hProbeSelf != INVALID_HANDLE_VALUE) CloseHandle(hProbeSelf);
        return false;
    }

    DWORD selfPid = GetCurrentProcessId();
    PSYSTEM_HANDLE_INFORMATION_EX pInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION_EX>(buffer);

    USHORT fileTypeIndex = 0xFFFF;
    if (hProbeSelf != INVALID_HANDLE_VALUE) {
        for (ULONG_PTR i = 0; i < pInfo->NumberOfHandles; ++i) {
            if (pInfo->Handles[i].UniqueProcessId == selfPid &&
                pInfo->Handles[i].HandleValue == (ULONG_PTR)hProbeSelf) {
                fileTypeIndex = pInfo->Handles[i].ObjectTypeIndex;
                break;
            }
        }
        CloseHandle(hProbeSelf);
    }

    wchar_t targetNorm[MAX_PATH] = {};
    NormalizePathCommon(szTargetPath, targetNorm, ARRAYSIZE(targetNorm));
    if (targetNorm[0] == L'\0') return false;

    bool closedAny = false;
    ULONG_PTR lastPid = 0;
    bool lastPidProtected = true;
    HANDLE hProc = NULL;

    for (ULONG_PTR i = 0; i < pInfo->NumberOfHandles; ++i) {
        const auto& entry = pInfo->Handles[i];
        if (fileTypeIndex != 0xFFFF && entry.ObjectTypeIndex != fileTypeIndex) {
            continue;
        }
        // Only handles that grant data or delete access can be holding the
        // item open. The old filter skipped 0x0012019F as well, which is
        // exactly GENERIC_READ|GENERIC_WRITE - the access mask of the process
        // that actually locks the file - so nothing was ever released unless
        // the owning process was killed.
        constexpr ULONG kDataOrDelete =
            FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE;
        if ((entry.GrantedAccess & kDataOrDelete) == 0) {
            continue;
        }

        ULONG_PTR pid = entry.UniqueProcessId;
        if (pid <= 4 || pid == selfPid) continue;

        if (pid != lastPid) {
            if (hProc) {
                CloseHandle(hProc);
                hProc = NULL;
            }
            lastPid = pid;
            lastPidProtected = IsProtectedProcess(static_cast<DWORD>(pid));
            if (!lastPidProtected) {
                hProc = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            }
        }
        if (lastPidProtected || !hProc) continue;

        HANDLE hLocal = NULL;
        if (DuplicateHandle(hProc, (HANDLE)entry.HandleValue, GetCurrentProcess(), &hLocal, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            wchar_t handleNorm[MAX_PATH] = {};
            if (SafeGetHandleDiskPath(hLocal, handleNorm, ARRAYSIZE(handleNorm))) {
                if (PathMatchesOrIsChild(handleNorm, targetNorm, isDirectory)) {
                    if (DuplicateHandle(hProc, (HANDLE)entry.HandleValue, NULL, NULL, 0, FALSE, DUPLICATE_CLOSE_SOURCE)) {
                        closedAny = true;
                    }
                }
            }
        }
    }
    if (hProc) CloseHandle(hProc);
    HeapFree(GetProcessHeap(), 0, buffer);
    return closedAny;
}

// ----------------------------------------------------------------------------
// Disconnect a File or Folder from whatever application is using it.
//
// This only *releases handles*; it never terminates a process. Killing the app
// that holds a file throws away whatever that app had not saved yet, which is
// not what "disconnect" promises. If a window still owns the item afterwards
// it is asked to close politely, and if the lock survives that the call reports
// failure so the user is told the truth.
// ----------------------------------------------------------------------------
bool DisconnectPathFromProcesses(const wchar_t* szPath, bool* pOutIsDirectory) {
    if (IsIgnoreImportantWindowsEnabled() && IsImportantWindowsPath(szPath)) {
        return false;
    }
    DWORD attrs = GetFileAttributesW(szPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    bool isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (pOutIsDirectory) *pOutIsDirectory = isDirectory;

    // Step 1: close the file/folder handles held by other processes.
    CloseMatchingSystemHandles(szPath, isDirectory);

    // Step 2: a visible window whose title carries the item (Notepad, Paint,
    // Photos) still owns it through its own handle. Ask it to close and let it
    // decide - an unsaved document will show its own save prompt.
    DWORD winPid = 0;
    HWND  winHwnd = NULL;
    if (HasWindowWithOpenItemTitle(szPath, &winPid, &winHwnd) && !IsProtectedProcess(winPid)) {
        SendMessageTimeoutW(winHwnd, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 1000, NULL);
    }

    // Step 3: report whether the item is actually free now.
    return !IsPathUsedByOtherAppCommon(szPath);
}

// ----------------------------------------------------------------------------
// Boot-Time Deletion Management ("Delete on next boot" / "Cancel delete on next boot")
// Saved in %APPDATA%\PepperCut\boot_delete.ini
// ----------------------------------------------------------------------------

// Walks a tree children-first and hands every path to `visit`, so a directory is
// never scheduled ahead of the files it still holds. Junctions and symlinks are
// visited themselves, never walked into: following one would queue the contents
// of an entirely different tree for deletion.
typedef void (*BootDeleteVisitor)(const wchar_t* path, void* ctx);

void WalkTreeChildrenFirst(const wchar_t* path, BootDeleteVisitor visit, void* ctx, int depth = 0) {
    if (!path || !*path || depth > 64) return;

    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return;

    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        visit(path, ctx);
        return;
    }

    wchar_t pattern[MAX_PATH] = {};
    StringCchPrintfW(pattern, ARRAYSIZE(pattern), L"%s\\*", path);

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

            wchar_t child[MAX_PATH] = {};
            StringCchPrintfW(child, ARRAYSIZE(child), L"%s\\%s", path, fd.cFileName);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                visit(child, ctx);
            } else {
                WalkTreeChildrenFirst(child, visit, ctx, depth + 1);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    visit(path, ctx); // the directory itself goes last
}

void ScheduleBootDeleteVisit(const wchar_t* path, void*) {
    MoveFileExW(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
}

bool ToggleBootDelete(const wchar_t* szPath, bool* pOutScheduled) {
    if (IsIgnoreImportantWindowsEnabled() && IsImportantWindowsPath(szPath) && !IsScheduledForBootDeleteCommon(szPath)) {
        return false;
    }
    wchar_t szKey[MAX_PATH] = {};
    MakeIniKeyFromPath(szPath, szKey, ARRAYSIZE(szKey));
    if (szKey[0] == L'\0') return false;

    wchar_t szBootIni[MAX_PATH] = {};
    GetPepperCutBootDeleteIniPath(szBootIni, MAX_PATH);

    if (IsScheduledForBootDeleteCommon(szPath)) {
        WritePrivateProfileStringW(L"BootDelete", szKey, NULL, szBootIni);
        if (pOutScheduled) *pOutScheduled = false;
        return true;
    }

    // Hand the whole tree to the kernel for the next restart. This is the real
    // "delete on next boot": it runs before any process can lock the files
    // again. The call needs administrator rights, so it is expected to fail for
    // a normal user - the logon sweep registered below is the fallback.
    WalkTreeChildrenFirst(szPath, ScheduleBootDeleteVisit, NULL);

    WritePrivateProfileStringW(L"BootDelete", szKey, szPath, szBootIni);

    HKEY hRunOnce = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunOnceRegKey, 0, NULL, 0, KEY_WRITE, NULL, &hRunOnce, NULL) == ERROR_SUCCESS) {
        wchar_t szCmd[MAX_PATH + 32] = {};
        StringCchPrintfW(szCmd, ARRAYSIZE(szCmd), L"\"%s\" --run-boot-deletes", g_szExePath);
        RegSetValueExW(
            hRunOnce,
            L"PepperCutBootCleanup",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(szCmd),
            static_cast<DWORD>((wcslen(szCmd) + 1) * sizeof(wchar_t))
        );
        RegCloseKey(hRunOnce);
    }
    if (pOutScheduled) *pOutScheduled = true;
    return true;
}

// Removes a path through the shell so it lands in the Recycle Bin and can be
// restored. Used by the logon sweep only.
void RecyclePath(const wchar_t* szPath) {
    wchar_t list[MAX_PATH + 1] = {}; // SHFileOperation wants a double-terminated list
    StringCchCopyW(list, MAX_PATH, szPath);

    SHFILEOPSTRUCTW op = {};
    op.wFunc  = FO_DELETE;
    op.pFrom  = list;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;
    SHFileOperationW(&op);
}

// Fallback sweep for queued deletions that the kernel could not schedule
// (no administrator rights). Runs once per logon through HKCU\...\RunOnce.
// It only deletes what is still there: no process is touched, nothing is
// disconnected and nothing is force-closed, because at logon the lock that
// originally blocked the item is normally gone on its own.
void ProcessPendingBootDeletesOnStartup() {
    wchar_t szBootIni[MAX_PATH] = {};
    GetPepperCutBootDeleteIniPath(szBootIni, MAX_PATH);
    if (GetFileAttributesW(szBootIni) == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    // The key list can be large, so it is a transient heap block rather than a
    // static buffer: a zero-filled static one would be written into the image.
    constexpr DWORD kKeyBufChars = 32768;
    wchar_t* pKeys = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), 0, kKeyBufChars * sizeof(wchar_t)));
    if (!pKeys) return;

    DWORD cchKeys = GetPrivateProfileStringW(L"BootDelete", NULL, L"", pKeys, kKeyBufChars, szBootIni);
    if (cchKeys == 0) {
        HeapFree(GetProcessHeap(), 0, pKeys);
        return;
    }

    const wchar_t* pKey = pKeys;
    while (*pKey) {
        wchar_t szTargetPath[MAX_PATH] = {};
        if (GetPrivateProfileStringW(L"BootDelete", pKey, L"", szTargetPath, MAX_PATH, szBootIni) > 0 && szTargetPath[0] != L'\0') {
            if (GetFileAttributesW(szTargetPath) != INVALID_FILE_ATTRIBUTES) {
                RecyclePath(szTargetPath);
            }
            WritePrivateProfileStringW(L"BootDelete", pKey, NULL, szBootIni);
        }
        pKey += wcslen(pKey) + 1;
    }
    HeapFree(GetProcessHeap(), 0, pKeys);
}

// ----------------------------------------------------------------------------
// Extract & Register Embedded COM IContextMenu Shell Extension (PepperCutShell.dll)
// Enables synchronous right-click evaluation inside Windows 10 Explorer!
// ----------------------------------------------------------------------------
static void SetRegistryDefaultString(HKEY hRoot, const wchar_t* szSubKey, const wchar_t* szVal) {
    HKEY hKey = NULL;
    if (RegCreateKeyExW(hRoot, szSubKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(
            hKey,
            NULL,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(szVal),
            static_cast<DWORD>((wcslen(szVal) + 1) * sizeof(wchar_t))
        );
        RegCloseKey(hKey);
    }
}

void InstallAndRegisterShellExtension() {
    // 1. Clean up any legacy static verb keys from earlier versions
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\PepperCut.1.Disconnect");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\PepperCut.2.BootDelete");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\PepperCut.1.Disconnect");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\PepperCut.2.BootDelete");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\PepperCutDisconnect");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\PepperCutBootDelete");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\PepperCutDisconnect");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\PepperCutBootDelete");

    // 2. Save settings into %APPDATA%\PepperCut\settings.ini
    //    Ensure "Start on boot" and "Ignore important windows file and folder" are ON by default (1)
    wchar_t szDir[MAX_PATH] = {};
    GetPepperCutAppDataDir(szDir, MAX_PATH);

    wchar_t szSettingsIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szSettingsIni, MAX_PATH);
    WritePrivateProfileStringW(L"PepperCut", L"Version", kAppVersion, szSettingsIni);
    WritePrivateProfileStringW(L"PepperCut", L"ExePath", g_szExePath, szSettingsIni);

    wchar_t szExistingStartBoot[16] = {};
    if (GetPrivateProfileStringW(L"PepperCut", L"StartOnBoot", L"", szExistingStartBoot, ARRAYSIZE(szExistingStartBoot), szSettingsIni) == 0) {
        WritePrivateProfileStringW(L"PepperCut", L"StartOnBoot", L"1", szSettingsIni);
    }

    wchar_t szExistingIgnore[16] = {};
    if (GetPrivateProfileStringW(L"PepperCut", L"IgnoreImportantWindows", L"", szExistingIgnore, ARRAYSIZE(szExistingIgnore), szSettingsIni) == 0) {
        WritePrivateProfileStringW(L"PepperCut", L"IgnoreImportantWindows", L"1", szSettingsIni);
    }

    // 3. Extract PepperCutShell.dll into %APPDATA%\PepperCut\PepperCutShell.dll
    //    The DLL travels as an RCDATA resource, so the bytes come straight out
    //    of the image instead of out of a compiled hex array.
    wchar_t szDllPath[MAX_PATH] = {};
    StringCchPrintfW(szDllPath, ARRAYSIZE(szDllPath), L"%s\\PepperCutShell.dll", szDir);

    HMODULE hSelf = GetModuleHandleW(NULL);
    HRSRC hRes = FindResourceW(hSelf, MAKEINTRESOURCEW(IDR_SHELL_DLL), RT_RCDATA);
    HGLOBAL hResData = hRes ? LoadResource(hSelf, hRes) : NULL;
    const void* pShellDll = hResData ? LockResource(hResData) : NULL;
    DWORD cbShellDll = hRes ? SizeofResource(hSelf, hRes) : 0;

    if (pShellDll && cbShellDll > 0) {
        // If an old copy is loaded in explorer.exe, rename it to .old first so we can write the new one
        HANDLE hFile = CreateFileW(szDllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            wchar_t szOldDll[MAX_PATH] = {};
            StringCchPrintfW(szOldDll, ARRAYSIZE(szOldDll), L"%s\\PepperCutShell.old.dll", szDir);
            DeleteFileW(szOldDll);
            MoveFileExW(szDllPath, szOldDll, MOVEFILE_REPLACE_EXISTING);
            hFile = CreateFileW(szDllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD dwWritten = 0;
            WriteFile(hFile, pShellDll, cbShellDll, &dwWritten, NULL);
            CloseHandle(hFile);
        }
    }

    // 4. Register COM CLSID under HKCU\Software\Classes\CLSID\{E8F4B9A1-7C3D-4E2A-9B1E-6D5F8A3C2E10}
    wchar_t szClsidKey[256] = {};
    StringCchPrintfW(szClsidKey, ARRAYSIZE(szClsidKey), L"Software\\Classes\\CLSID\\%s", kShellExtClsidStr);
    SetRegistryDefaultString(HKEY_CURRENT_USER, szClsidKey, L"PepperCut Shell Context Menu");

    wchar_t szInprocKey[256] = {};
    StringCchPrintfW(szInprocKey, ARRAYSIZE(szInprocKey), L"Software\\Classes\\CLSID\\%s\\InprocServer32", kShellExtClsidStr);
    HKEY hInproc = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, szInprocKey, 0, NULL, 0, KEY_WRITE, NULL, &hInproc, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(
            hInproc,
            NULL,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(szDllPath),
            static_cast<DWORD>((wcslen(szDllPath) + 1) * sizeof(wchar_t))
        );
        const wchar_t* szModel = L"Apartment";
        RegSetValueExW(
            hInproc,
            L"ThreadingModel",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(szModel),
            static_cast<DWORD>((wcslen(szModel) + 1) * sizeof(wchar_t))
        );
        RegCloseKey(hInproc);
    }

    // 5. Register ContextMenuHandlers for All Files (*), Folders (Directory), and Folder Background
    SetRegistryDefaultString(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\PepperCut",
        kShellExtClsidStr
    );
    SetRegistryDefaultString(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\PepperCut",
        kShellExtClsidStr
    );

    // 6. Add to Approved Shell Extensions in HKCU
    HKEY hApproved = NULL;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
            0, NULL, 0, KEY_WRITE, NULL, &hApproved, NULL) == ERROR_SUCCESS) {
        const wchar_t* szDesc = L"PepperCut Context Menu Extension";
        RegSetValueExW(
            hApproved,
            kShellExtClsidStr,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(szDesc),
            static_cast<DWORD>((wcslen(szDesc) + 1) * sizeof(wchar_t))
        );
        RegCloseKey(hApproved);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

// ----------------------------------------------------------------------------
// Send Windows 10 Tray Notification
// ----------------------------------------------------------------------------
void ShowTrayNotification(const wchar_t* szMessage) {
    if (!g_hMainWnd) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize       = sizeof(NOTIFYICONDATAW);
    nid.hWnd         = g_hMainWnd;
    nid.uID          = ID_TRAY_ICON;
    nid.uFlags       = NIF_INFO | NIF_ICON | NIF_TIP;
    nid.hIcon        = g_hAppIcon;
    nid.hBalloonIcon = g_hAppIcon;
    nid.dwInfoFlags  = NIIF_USER | NIIF_LARGE_ICON;
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"PepperCut");
    StringCchCopyW(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), L"PepperCut");
    StringCchCopyW(nid.szInfo, ARRAYSIZE(nid.szInfo), szMessage);
    if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) {
        nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
}

void NotifyRunningTrayOrFallback(DWORD notifyCode, const wchar_t* szFallbackMessage) {
    HWND hExisting = FindWindowW(kWndClassName, NULL);
    if (hExisting) {
        PostMessageW(hExisting, WM_PEPPERCUT_NOTIFY, (WPARAM)notifyCode, 0);
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"PepperCutTempNotifyWnd";
    RegisterClassExW(&wc);
    HWND hTemp = CreateWindowExW(0, wc.lpszClassName, kAppName, WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);

    NOTIFYICONDATAW nid = {};
    nid.cbSize       = sizeof(NOTIFYICONDATAW);
    nid.hWnd         = hTemp;
    nid.uID          = ID_TRAY_ICON;
    nid.uFlags       = NIF_ICON | NIF_TIP | NIF_INFO;
    nid.hIcon        = g_hAppIcon;
    nid.hBalloonIcon = g_hAppIcon;
    nid.dwInfoFlags  = NIIF_INFO;
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"PepperCut");
    StringCchCopyW(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), L"PepperCut");
    StringCchCopyW(nid.szInfo, ARRAYSIZE(nid.szInfo), szFallbackMessage);
    Shell_NotifyIconW(NIM_ADD, &nid);
    Sleep(3500);
    Shell_NotifyIconW(NIM_DELETE, &nid);
    DestroyWindow(hTemp);
}

// ----------------------------------------------------------------------------
// Start on Boot by Default (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)
// Synced with %APPDATA%\PepperCut\settings.ini (ON by default)
// ----------------------------------------------------------------------------
void EnsureStartOnBootEnabled() {
    wchar_t szSettingsIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szSettingsIni, MAX_PATH);
    wchar_t szExistingStartBoot[16] = {};
    if (GetPrivateProfileStringW(L"PepperCut", L"StartOnBoot", L"", szExistingStartBoot, ARRAYSIZE(szExistingStartBoot), szSettingsIni) == 0) {
        WritePrivateProfileStringW(L"PepperCut", L"StartOnBoot", L"1", szSettingsIni);
    }
    bool startOnBoot = IsStartOnBootEnabled();
    SyncStartOnBootRegistry(startOnBoot, g_szExePath);
}

// ----------------------------------------------------------------------------
// System Tray Hidden Window Procedure
// ----------------------------------------------------------------------------
LRESULT CALLBACK HiddenTrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static UINT s_uTaskbarRestart = 0;
    if (msg == WM_CREATE) {
        s_uTaskbarRestart = RegisterWindowMessageW(L"TaskbarCreated");
        return 0;
    }
    if (s_uTaskbarRestart != 0 && msg == s_uTaskbarRestart) {
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        return 0;
    }

    switch (msg) {
    case WM_PEPPERCUT_NOTIFY:
        switch (wParam) {
        case COPYDATA_NOTIFY_START:
            ShowTrayNotification(L"PepperCut is running");
            break;
        case COPYDATA_NOTIFY_FILE_DISC:
            ShowTrayNotification(L"File has been disconnected");
            break;
        case COPYDATA_NOTIFY_FOLDER_DISC:
            ShowTrayNotification(L"Folder has been disconnected");
            break;
        case COPYDATA_NOTIFY_BOOT_DEL:
            ShowTrayNotification(L"File will be deleted on next boot");
            break;
        case COPYDATA_NOTIFY_BOOT_CANCEL:
            ShowTrayNotification(L"Delete on next boot cancelled");
            break;
        case COPYDATA_NOTIFY_FAIL:
            ShowTrayNotification(L"Still in use - nothing was disconnected");
            break;
        }
        return 0;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            POINT pt = {};
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd);

            bool startOnBoot = IsStartOnBootEnabled();
            bool ignoreWin   = IsIgnoreImportantWindowsEnabled();
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(
                hMenu,
                MF_STRING | (startOnBoot ? MF_CHECKED : MF_UNCHECKED),
                IDM_TRAY_START_BOOT,
                L"Start on boot"
            );
            AppendMenuW(
                hMenu,
                MF_STRING | (ignoreWin ? MF_CHECKED : MF_UNCHECKED),
                IDM_TRAY_IGNORE_WIN,
                L"Ignore important windows file and folder"
            );
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Exit");

            UINT cmd = TrackPopupMenu(
                hMenu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                pt.x,
                pt.y,
                0,
                hWnd,
                NULL
            );
            DestroyMenu(hMenu);

            if (cmd == IDM_TRAY_START_BOOT) {
                SetStartOnBootEnabled(!startOnBoot, g_szExePath);
            } else if (cmd == IDM_TRAY_IGNORE_WIN) {
                SetIgnoreImportantWindowsEnabled(!ignoreWin);
            } else if (cmd == IDM_TRAY_EXIT) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                PostQuitMessage(0);
            }
        }
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// Entry Point (wWinMain)
// ----------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    EnableDebugPrivilege();
    GetModuleFileNameW(NULL, g_szExePath, MAX_PATH);
    g_hAppIcon = LoadOrCreatePepperCutIcon();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv && argc >= 2) {
        if (wcscmp(argv[1], L"--run-boot-deletes") == 0) {
            ProcessPendingBootDeletesOnStartup();
            LocalFree(argv);
            return 0;
        }

        if (argc >= 3) {
            const wchar_t* mode = argv[1];
            const wchar_t* targetPath = argv[2];

            if (wcscmp(mode, L"--disconnect") == 0) {
                bool isFolder = false;
                bool released = DisconnectPathFromProcesses(targetPath, &isFolder);
                if (released) {
                    NotifyRunningTrayOrFallback(
                        isFolder ? COPYDATA_NOTIFY_FOLDER_DISC : COPYDATA_NOTIFY_FILE_DISC,
                        isFolder ? L"Folder has been disconnected" : L"File has been disconnected"
                    );
                } else {
                    NotifyRunningTrayOrFallback(COPYDATA_NOTIFY_FAIL, L"Still in use - nothing was disconnected");
                }
                LocalFree(argv);
                return 0;
            }

            if (wcscmp(mode, L"--boot-delete") == 0) {
                bool nowScheduled = false;
                if (!ToggleBootDelete(targetPath, &nowScheduled)) {
                    NotifyRunningTrayOrFallback(COPYDATA_NOTIFY_FAIL, L"This location is protected");
                } else if (nowScheduled) {
                    NotifyRunningTrayOrFallback(COPYDATA_NOTIFY_BOOT_DEL, L"File will be deleted on next boot");
                } else {
                    NotifyRunningTrayOrFallback(COPYDATA_NOTIFY_BOOT_CANCEL, L"Delete on next boot cancelled");
                }
                LocalFree(argv);
                return 0;
            }
        }
    }
    if (argv) LocalFree(argv);

    // Ensure Start on Boot is enabled by default
    EnsureStartOnBootEnabled();

    // Extract & register our COM IContextMenu Shell Extension DLL
    InstallAndRegisterShellExtension();

    // NOTE: the boot-delete queue is drained only by the --run-boot-deletes
    // invocation registered in RunOnce. It must never run here: doing so would
    // delete "delete on next boot" items the moment the user re-launches
    // PepperCut, which is not what was promised.

    // Single-instance check: if already running in tray, trigger startup notification and exit
    HWND hExisting = FindWindowW(kWndClassName, NULL);
    if (hExisting) {
        PostMessageW(hExisting, WM_PEPPERCUT_NOTIFY, COPYDATA_NOTIFY_START, 0);
        return 0;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = HiddenTrayWndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = g_hAppIcon;
    wc.hIconSm       = g_hAppIcon;
    wc.lpszClassName = kWndClassName;
    RegisterClassExW(&wc);

    g_hMainWnd = CreateWindowExW(
        0,
        kWndClassName,
        kAppName,
        WS_OVERLAPPED,
        0, 0, 0, 0,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // Add icon to Windows 10 System Tray
    g_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd             = g_hMainWnd;
    g_nid.uID              = ID_TRAY_ICON;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_hAppIcon;
    g_nid.hBalloonIcon     = g_hAppIcon;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"PepperCut");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Display startup notification: "PepperCut is running"
    ShowTrayNotification(L"PepperCut is running");

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
