// ============================================================================
// PepperCut.cpp — Pure 100% C++ & Win32 Standalone Background File/Folder Unlocker
// Target OS: Windows 10 x64 (Subsystem: WINDOWS / wWinMain)
// ============================================================================

#include "PepperCutCommon.h"
#include "EmbeddedShellDll.h"
#include <winternl.h>

#define WM_TRAYICON             (WM_APP + 1)
#define WM_PEPPERCUT_NOTIFY     (WM_APP + 2)
#define ID_TRAY_ICON            1001
#define IDM_TRAY_EXIT           2001
#define IDM_TRAY_IGNORE_WIN     2002
#define IDM_TRAY_START_BOOT     2003
#define IDI_PEPPERCUT_ICON      101

#define COPYDATA_NOTIFY_START        1
#define COPYDATA_NOTIFY_FILE_DISC    2
#define COPYDATA_NOTIFY_FOLDER_DISC  3
#define COPYDATA_NOTIFY_BOOT_DEL     4
#define COPYDATA_NOTIFY_BOOT_CANCEL  5

static const wchar_t* kRunRegKey        = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
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

bool PathMatchesOrIsChild(const std::wstring& handlePathNorm, const std::wstring& targetNorm, bool isDirectory) {
    if (handlePathNorm.empty() || targetNorm.empty()) return false;
    if (handlePathNorm == targetNorm) return true;
    if (isDirectory && handlePathNorm.size() > targetNorm.size()) {
        if (handlePathNorm.compare(0, targetNorm.size(), targetNorm) == 0 &&
            handlePathNorm[targetNorm.size()] == L'\\') {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Persistent Worker Thread for SafeGetHandleDiskPath
// ----------------------------------------------------------------------------
static HANDLE  g_hWorkerReqEvent  = NULL;
static HANDLE  g_hWorkerDoneEvent = NULL;
static HANDLE  g_hWorkerThread    = NULL;
static HANDLE  g_hQueryTarget     = NULL;
static wchar_t g_szQueryResult[MAX_PATH] = {};
static DWORD   g_dwQueryLen       = 0;

DWORD WINAPI HandlePathWorkerProc(LPVOID) {
    while (WaitForSingleObject(g_hWorkerReqEvent, INFINITE) == WAIT_OBJECT_0) {
        g_dwQueryLen = 0;
        if (g_hQueryTarget && GetFileType(g_hQueryTarget) == FILE_TYPE_DISK) {
            g_dwQueryLen = GetFinalPathNameByHandleW(g_hQueryTarget, g_szQueryResult, MAX_PATH, VOLUME_NAME_DOS);
        }
        SetEvent(g_hWorkerDoneEvent);
    }
    return 0;
}

bool SafeGetHandleDiskPath(HANDLE hLocal, std::wstring& outNormPath) {
    if (!g_hWorkerReqEvent) {
        g_hWorkerReqEvent  = CreateEventW(NULL, FALSE, FALSE, NULL);
        g_hWorkerDoneEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    }
    if (!g_hWorkerThread) {
        g_hWorkerThread = CreateThread(NULL, 0, HandlePathWorkerProc, NULL, 0, NULL);
    }
    if (!g_hWorkerThread) return false;

    g_hQueryTarget = hLocal;
    SetEvent(g_hWorkerReqEvent);

    if (WaitForSingleObject(g_hWorkerDoneEvent, 4) == WAIT_TIMEOUT) {
        TerminateThread(g_hWorkerThread, 1);
        CloseHandle(g_hWorkerThread);
        g_hWorkerThread = NULL;
        return false;
    }

    if (g_dwQueryLen == 0 || g_dwQueryLen >= MAX_PATH) return false;
    outNormPath = NormalizePathCommon(g_szQueryResult);
    return !outNormPath.empty();
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
    std::vector<BYTE> buffer(bufSize);
    ULONG returnLen = 0;
    NTSTATUS status = 0;

    while ((status = NtQuerySysInfo(64, buffer.data(), bufSize, &returnLen)) == (NTSTATUS)0xC0000004L) {
        bufSize = returnLen ? (returnLen + 65536) : (bufSize * 2);
        if (bufSize > 64 * 1024 * 1024) {
            if (hProbeSelf != INVALID_HANDLE_VALUE) CloseHandle(hProbeSelf);
            return false;
        }
        buffer.resize(bufSize);
    }
    if (status < 0) {
        if (hProbeSelf != INVALID_HANDLE_VALUE) CloseHandle(hProbeSelf);
        return false;
    }

    DWORD selfPid = GetCurrentProcessId();
    PSYSTEM_HANDLE_INFORMATION_EX pInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION_EX>(buffer.data());

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

    std::wstring targetNorm = NormalizePathCommon(szTargetPath);
    if (targetNorm.empty()) return false;

    bool closedAny = false;
    ULONG_PTR lastPid = 0;
    HANDLE hProc = NULL;
    ULONG timedOutAccessMask = 0xFFFFFFFF;

    for (ULONG_PTR i = 0; i < pInfo->NumberOfHandles; ++i) {
        const auto& entry = pInfo->Handles[i];
        if (fileTypeIndex != 0xFFFF && entry.ObjectTypeIndex != fileTypeIndex) {
            continue;
        }
        if (entry.GrantedAccess == 0x0012019F ||
            entry.GrantedAccess == 0x00120189 ||
            entry.GrantedAccess == 0x00120089 ||
            entry.GrantedAccess == timedOutAccessMask) {
            continue;
        }

        ULONG_PTR pid = entry.UniqueProcessId;
        if (pid <= 4 || pid == selfPid) continue;

        if (pid != lastPid) {
            if (hProc) CloseHandle(hProc);
            hProc = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            lastPid = pid;
            timedOutAccessMask = 0xFFFFFFFF;
        }
        if (!hProc) continue;

        HANDLE hLocal = NULL;
        if (DuplicateHandle(hProc, (HANDLE)entry.HandleValue, GetCurrentProcess(), &hLocal, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            std::wstring handleNorm;
            if (SafeGetHandleDiskPath(hLocal, handleNorm)) {
                if (PathMatchesOrIsChild(handleNorm, targetNorm, isDirectory)) {
                    CloseHandle(hLocal);
                    hLocal = NULL;
                    if (DuplicateHandle(hProc, (HANDLE)entry.HandleValue, NULL, NULL, 0, FALSE, DUPLICATE_CLOSE_SOURCE)) {
                        closedAny = true;
                    }
                }
            } else if (!g_hWorkerThread) {
                timedOutAccessMask = entry.GrantedAccess;
            }
            if (hLocal) CloseHandle(hLocal);
        }
    }
    if (hProc) CloseHandle(hProc);
    return closedAny;
}

// ----------------------------------------------------------------------------
// Disconnect a File or Folder from whatever application is using it
// ----------------------------------------------------------------------------
bool DisconnectPathFromProcesses(const wchar_t* szPath, bool* pOutIsDirectory) {
    if (IsIgnoreImportantWindowsEnabled() && IsImportantWindowsPath(szPath)) {
        return false;
    }
    DWORD attrs = GetFileAttributesW(szPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    bool isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (pOutIsDirectory) *pOutIsDirectory = isDirectory;

    // Step 1: Close all open file/folder handles in remote processes non-destructively
    CloseMatchingSystemHandles(szPath, isDirectory);

    // Step 2: Close any application windows (e.g. Notepad, Paint, Photos) holding the item open
    DWORD winPid = 0;
    HWND  winHwnd = NULL;
    while (HasWindowWithOpenItemTitle(szPath, &winPid, &winHwnd)) {
        DWORD_PTR dwRes = 0;
        SendMessageTimeoutW(winHwnd, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG, 300, &dwRes);
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, winPid);
        if (hProc) {
            TerminateProcess(hProc, 0);
            CloseHandle(hProc);
        }
        break;
    }

    // Step 3: Use Windows Restart Manager to release any remaining locks
    std::vector<std::wstring> resources;
    std::vector<std::wstring> childDirs;
    if (isDirectory) {
        CollectItemsInFolderCommon(szPath, resources, childDirs, 0);
    } else {
        resources.push_back(szPath);
    }

    if (!resources.empty()) {
        std::vector<PCWSTR> ptrs;
        for (const auto& r : resources) ptrs.push_back(r.c_str());

        DWORD dwSession = 0;
        wchar_t szSessionKey[CCH_RM_SESSION_KEY + 1] = {};
        if (RmStartSession(&dwSession, 0, szSessionKey) == ERROR_SUCCESS) {
            if (RmRegisterResources(dwSession, (UINT)ptrs.size(), ptrs.data(), 0, NULL, 0, NULL) == ERROR_SUCCESS) {
                UINT nNeeded = 0, nCount = 16;
                DWORD dwReason = RmRebootReasonNone;
                std::vector<RM_PROCESS_INFO> rgpi(nCount);
                DWORD res = RmGetList(dwSession, &nNeeded, &nCount, rgpi.data(), &dwReason);
                if (res == ERROR_MORE_DATA) {
                    nCount = nNeeded;
                    rgpi.resize(nCount);
                    res = RmGetList(dwSession, &nNeeded, &nCount, rgpi.data(), &dwReason);
                }
                if (res == ERROR_SUCCESS && nCount > 0) {
                    DWORD selfPid = GetCurrentProcessId();
                    DWORD explorerPid = 0;
                    if (GetShellWindow()) GetWindowThreadProcessId(GetShellWindow(), &explorerPid);

                    for (UINT i = 0; i < nCount; ++i) {
                        DWORD pid = rgpi[i].Process.dwProcessId;
                        if (pid != selfPid && pid != explorerPid) {
                            if (IsPathUsedByOtherAppCommon(szPath)) {
                                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                                if (hProc) {
                                    TerminateProcess(hProc, 0);
                                    CloseHandle(hProc);
                                }
                            }
                        }
                    }
                }
            }
            RmEndSession(dwSession);
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// Boot-Time Deletion Management ("Delete on next boot" / "Cancel delete on next boot")
// Saved in %APPDATA%\PepperCut\boot_delete.ini
// ----------------------------------------------------------------------------
bool ToggleBootDelete(const wchar_t* szPath, bool* pOutScheduled) {
    if (IsIgnoreImportantWindowsEnabled() && IsImportantWindowsPath(szPath) && !IsScheduledForBootDeleteCommon(szPath)) {
        return false;
    }
    std::wstring key = MakeIniKeyFromPath(szPath);
    if (key.empty()) return false;

    wchar_t szBootIni[MAX_PATH] = {};
    GetPepperCutBootDeleteIniPath(szBootIni, MAX_PATH);

    if (IsScheduledForBootDeleteCommon(szPath)) {
        WritePrivateProfileStringW(L"BootDelete", key.c_str(), NULL, szBootIni);
        if (pOutScheduled) *pOutScheduled = false;
        return true;
    } else {
        MoveFileExW(szPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        WritePrivateProfileStringW(L"BootDelete", key.c_str(), szPath, szBootIni);

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
}

void RecursiveDeleteDirectory(const std::wstring& dirPath) {
    std::wstring pattern = dirPath + L"\\*";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring child = dirPath + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                RecursiveDeleteDirectory(child);
            } else {
                SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    SetFileAttributesW(dirPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(dirPath.c_str());
}

void ProcessPendingBootDeletesOnStartup() {
    wchar_t szBootIni[MAX_PATH] = {};
    GetPepperCutBootDeleteIniPath(szBootIni, MAX_PATH);
    if (GetFileAttributesW(szBootIni) == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    std::vector<wchar_t> keyBuf(32768, L'\0');
    DWORD cchKeys = GetPrivateProfileStringW(L"BootDelete", NULL, L"", keyBuf.data(), static_cast<DWORD>(keyBuf.size()), szBootIni);
    if (cchKeys == 0) return;

    const wchar_t* pKey = keyBuf.data();
    while (*pKey) {
        wchar_t szTargetPath[MAX_PATH] = {};
        if (GetPrivateProfileStringW(L"BootDelete", pKey, L"", szTargetPath, MAX_PATH, szBootIni) > 0 && szTargetPath[0] != L'\0') {
            DWORD attrs = GetFileAttributesW(szTargetPath);
            if (attrs != INVALID_FILE_ATTRIBUTES) {
                bool isDir = false;
                DisconnectPathFromProcesses(szTargetPath, &isDir);
                if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                    RecursiveDeleteDirectory(szTargetPath);
                } else {
                    SetFileAttributesW(szTargetPath, FILE_ATTRIBUTE_NORMAL);
                    DeleteFileW(szTargetPath);
                }
            }
            WritePrivateProfileStringW(L"BootDelete", pKey, NULL, szBootIni);
        }
        pKey += wcslen(pKey) + 1;
    }
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
    wchar_t szDllPath[MAX_PATH] = {};
    StringCchPrintfW(szDllPath, ARRAYSIZE(szDllPath), L"%s\\PepperCutShell.dll", szDir);

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
        WriteFile(hFile, kEmbeddedShellDll, kEmbeddedShellDllSize, &dwWritten, NULL);
        CloseHandle(hFile);
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
                DisconnectPathFromProcesses(targetPath, &isFolder);
                NotifyRunningTrayOrFallback(
                    isFolder ? COPYDATA_NOTIFY_FOLDER_DISC : COPYDATA_NOTIFY_FILE_DISC,
                    isFolder ? L"Folder has been disconnected" : L"File has been disconnected"
                );
                LocalFree(argv);
                return 0;
            }

            if (wcscmp(mode, L"--boot-delete") == 0) {
                bool nowScheduled = false;
                ToggleBootDelete(targetPath, &nowScheduled);
                if (nowScheduled) {
                    NotifyRunningTrayOrFallback(COPYDATA_NOTIFY_BOOT_DEL, L"File will be deleted on next boot");
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

    // Process any pending boot deletions queued by PepperCut
    ProcessPendingBootDeletesOnStartup();

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
