#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00
#define WINVER       0x0A00

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <restartmanager.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include <cmath>

static const wchar_t* kAppName        = L"PepperCut";
static const wchar_t* kAppVersion     = L"1.1";
static const wchar_t* kWndClassName   = L"PepperCutHiddenTrayWindow";

// {E8F4B9A1-7C3D-4E2A-9B1E-6D5F8A3C2E10}
static const GUID CLSID_PepperCutShellExt = {
    0xe8f4b9a1, 0x7c3d, 0x4e2a, { 0x9b, 0x1e, 0x6d, 0x5f, 0x8a, 0x3c, 0x2e, 0x10 }
};
static const wchar_t* kShellExtClsidStr = L"{E8F4B9A1-7C3D-4E2A-9B1E-6D5F8A3C2E10}";

inline float DistToSegmentCommon(float px, float py, float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float l2 = dx * dx + dy * dy;
    if (l2 == 0.0f) return hypotf(px - x1, py - y1);
    float t = ((px - x1) * dx + (py - y1) * dy) / l2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return hypotf(px - (x1 + t * dx), py - (y1 + t * dy));
}

// Renders a 32-bit ARGB HBITMAP for IContextMenu / Menu items
// Draws White Scissors on a Crimson Red (#DC2626) Background (no cable)
inline HBITMAP CreatePepperCutMenuBitmap(int size = 16) {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = -size; // top-down DIB
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pPixels = nullptr;
    HDC hdc = GetDC(NULL);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pPixels), NULL, 0);
    ReleaseDC(NULL, hdc);

    if (hBmp && pPixels) {
        float scale = size / 32.0f;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float nx = (x + 0.5f) / scale;
                float ny = (y + 0.5f) / scale;

                float cx = (nx < 5.5f) ? 5.5f : ((nx > 26.5f) ? 26.5f : nx);
                float cy = (ny < 5.5f) ? 5.5f : ((ny > 26.5f) ? 26.5f : ny);
                float cornerDist = hypotf(nx - cx, ny - cy);
                if (cornerDist > 5.8f) {
                    pPixels[y * size + x] = 0x00000000;
                    continue;
                }

                // Base red (#DC2626)
                BYTE r = 220, g = 38, b = 38, a = 255;

                // White Scissors (no cable)
                float dRing1  = fabsf(hypotf(nx - 8.5f, ny - 11.0f) - 3.6f);
                float dRing2  = fabsf(hypotf(nx - 8.5f, ny - 21.0f) - 3.6f);
                float dBlade1 = DistToSegmentCommon(nx, ny, 11.5f, 13.2f, 26.0f, 22.5f);
                float dBlade2 = DistToSegmentCommon(nx, ny, 11.5f, 18.8f, 26.0f, 9.5f);

                if (dRing1 < 1.65f || dRing2 < 1.65f || dBlade1 < 1.75f || dBlade2 < 1.75f) {
                    float dPivot = hypotf(nx - 16.2f, ny - 16.0f);
                    if (dPivot >= 1.1f) {
                        r = 255;
                        g = 255;
                        b = 255;
                    }
                }

                pPixels[y * size + x] = (DWORD(a) << 24) | (DWORD(r) << 16) | (DWORD(g) << 8) | DWORD(b);
            }
        }
    }
    return hBmp;
}

// ----------------------------------------------------------------------------
// %APPDATA%\PepperCut Storage Directory & INI Paths
// Saves settings ("Ignore important windows file and folder", ExePath, Version)
// and "Delete on next boot" queue into %APPDATA%\PepperCut
// ----------------------------------------------------------------------------
inline bool GetPepperCutAppDataDir(wchar_t* outDir, DWORD cchMax) {
    wchar_t szBase[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, szBase)) || szBase[0] == L'\0') {
        if (GetEnvironmentVariableW(L"APPDATA", szBase, MAX_PATH) == 0) {
            GetTempPathW(MAX_PATH, szBase);
        }
    }
    StringCchPrintfW(outDir, cchMax, L"%s\\PepperCut", szBase);
    CreateDirectoryW(outDir, NULL);
    return true;
}

inline void GetPepperCutSettingsIniPath(wchar_t* outPath, DWORD cchMax) {
    wchar_t szDir[MAX_PATH] = {};
    GetPepperCutAppDataDir(szDir, MAX_PATH);
    StringCchPrintfW(outPath, cchMax, L"%s\\settings.ini", szDir);
}

inline void GetPepperCutBootDeleteIniPath(wchar_t* outPath, DWORD cchMax) {
    wchar_t szDir[MAX_PATH] = {};
    GetPepperCutAppDataDir(szDir, MAX_PATH);
    StringCchPrintfW(outPath, cchMax, L"%s\\boot_delete.ini", szDir);
}

inline void NormalizePathCommon(const wchar_t* szPath, wchar_t* outNorm, size_t cchMax) {
    if (cchMax == 0) return;
    outNorm[0] = L'\0';
    if (!szPath || !*szPath) return;

    const wchar_t* p = szPath;
    if (wcsncmp(p, L"\\\\?\\", 4) == 0) {
        p += 4;
    }
    StringCchCopyW(outNorm, cchMax, p);

    size_t len = wcslen(outNorm);
    while (len > 3 && (outNorm[len - 1] == L'\\' || outNorm[len - 1] == L'/')) {
        outNorm[--len] = L'\0';
    }
    if (len > 0) {
        CharLowerBuffW(outNorm, static_cast<DWORD>(len));
    }
}

// ----------------------------------------------------------------------------
// Processes PepperCut must never touch. Closing a handle inside the session
// manager, the shell or an antivirus service can hang or take down the whole
// session, so these are refused even when the handle belongs to the target.
// ----------------------------------------------------------------------------
inline bool IsProtectedProcessName(const wchar_t* szImageName) {
    if (!szImageName || !*szImageName) return true;
    const wchar_t* pLeaf = PathFindFileNameW(szImageName);
    const wchar_t* name = (pLeaf && *pLeaf) ? pLeaf : szImageName;
    if (!*name) return true;

    wchar_t nameLower[MAX_PATH] = {};
    StringCchCopyW(nameLower, ARRAYSIZE(nameLower), name);
    CharLowerBuffW(nameLower, static_cast<DWORD>(wcslen(nameLower)));

    static const wchar_t* kProtected[] = {
        L"system", L"registry", L"smss.exe", L"csrss.exe", L"wininit.exe",
        L"winlogon.exe", L"services.exe", L"lsass.exe", L"lsaiso.exe",
        L"svchost.exe", L"dwm.exe", L"fontdrvhost.exe", L"sihost.exe",
        L"explorer.exe", L"searchhost.exe", L"shellexperiencehost.exe",
        L"startmenuexperiencehost.exe", L"taskhostw.exe", L"dllhost.exe",
        L"securityhealthservice.exe", L"msmpeng.exe", L"nissrv.exe",
        L"trustedinstaller.exe", L"tiworker.exe", L"audiodg.exe", L"wudfhost.exe"
    };
    for (const wchar_t* p : kProtected) {
        if (wcscmp(nameLower, p) == 0) return true;
    }
    return false;
}

// True when the handle owner is off limits. A process we cannot even open for
// PROCESS_QUERY_LIMITED_INFORMATION is unreachable for DUP_HANDLE too, so it
// counts as protected.
inline bool IsProtectedProcess(DWORD pid) {
    if (pid <= 4) return true;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return true;
    wchar_t szImage[MAX_PATH] = {};
    DWORD cch = ARRAYSIZE(szImage);
    bool isProtected = true;
    if (QueryFullProcessImageNameW(hProc, 0, szImage, &cch)) {
        isProtected = IsProtectedProcessName(szImage);
    }
    CloseHandle(hProc);
    return isProtected;
}

// Converts a normalized path into a safe INI key name
inline void MakeIniKeyFromPath(const wchar_t* szPath, wchar_t* outKey, size_t cchMax) {
    NormalizePathCommon(szPath, outKey, cchMax);
    for (wchar_t* p = outKey; *p; ++p) {
        if (*p == L'\\' || *p == L'/' || *p == L':' || *p == L'=' || *p == L'[' || *p == L']') {
            *p = L'_';
        }
    }
}

// ----------------------------------------------------------------------------
// "Start on boot" setting (ON by default = 1)
// Saved in %APPDATA%\PepperCut\settings.ini and synced with HKCU\...\Run
// ----------------------------------------------------------------------------
inline bool IsStartOnBootEnabled() {
    wchar_t szIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szIni, MAX_PATH);
    UINT val = GetPrivateProfileIntW(L"PepperCut", L"StartOnBoot", 1 /* ON by default */, szIni);
    return (val != 0);
}

inline void SyncStartOnBootRegistry(bool enabled, const wchar_t* szExePath) {
    static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    HKEY hKey = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (enabled && szExePath && *szExePath) {
            wchar_t szQuoted[MAX_PATH + 8] = {};
            StringCchPrintfW(szQuoted, ARRAYSIZE(szQuoted), L"\"%s\"", szExePath);
            RegSetValueExW(
                hKey,
                kAppName,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(szQuoted),
                static_cast<DWORD>((wcslen(szQuoted) + 1) * sizeof(wchar_t))
            );
        } else {
            RegDeleteValueW(hKey, kAppName);
        }
        RegCloseKey(hKey);
    }
}

inline void SetStartOnBootEnabled(bool enabled, const wchar_t* szExePath) {
    wchar_t szIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szIni, MAX_PATH);
    WritePrivateProfileStringW(L"PepperCut", L"StartOnBoot", enabled ? L"1" : L"0", szIni);
    SyncStartOnBootRegistry(enabled, szExePath);
}

// ----------------------------------------------------------------------------
// "Ignore important windows file and folder" setting (ON by default = 1)
// Saved in %APPDATA%\PepperCut\settings.ini
// ----------------------------------------------------------------------------
inline bool IsIgnoreImportantWindowsEnabled() {
    wchar_t szIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szIni, MAX_PATH);
    UINT val = GetPrivateProfileIntW(L"PepperCut", L"IgnoreImportantWindows", 1 /* ON by default */, szIni);
    return (val != 0);
}

inline void SetIgnoreImportantWindowsEnabled(bool enabled) {
    wchar_t szIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szIni, MAX_PATH);
    WritePrivateProfileStringW(L"PepperCut", L"IgnoreImportantWindows", enabled ? L"1" : L"0", szIni);
}

// ----------------------------------------------------------------------------
// "Delete on next boot" state stored in %APPDATA%\PepperCut\boot_delete.ini
// ----------------------------------------------------------------------------
inline bool IsScheduledForBootDeleteCommon(const wchar_t* szPath) {
    wchar_t szKey[MAX_PATH] = {};
    MakeIniKeyFromPath(szPath, szKey, ARRAYSIZE(szKey));
    if (szKey[0] == L'\0') return false;

    wchar_t szBootIni[MAX_PATH] = {};
    GetPepperCutBootDeleteIniPath(szBootIni, MAX_PATH);

    wchar_t szStored[MAX_PATH] = {};
    DWORD cch = GetPrivateProfileStringW(L"BootDelete", szKey, L"", szStored, MAX_PATH, szBootIni);
    return (cch > 0 && szStored[0] != L'\0');
}

inline bool PathStartsWithDirPrefix(const wchar_t* pathNorm, const wchar_t* prefixNorm) {
    if (!pathNorm || !prefixNorm || !*pathNorm || !*prefixNorm) return false;
    size_t prefixLen = wcslen(prefixNorm);
    if (wcscmp(pathNorm, prefixNorm) == 0) return true;
    if (wcsncmp(pathNorm, prefixNorm, prefixLen) == 0 && pathNorm[prefixLen] == L'\\') {
        return true;
    }
    return false;
}

inline bool IsImportantWindowsPath(const wchar_t* szPath) {
    wchar_t norm[MAX_PATH] = {};
    NormalizePathCommon(szPath, norm, ARRAYSIZE(norm));
    size_t normLen = wcslen(norm);
    if (normLen == 0) return false;

    // 1. Protect drive roots (e.g. "c:", "c:\")
    if (normLen <= 3 && normLen >= 2 && norm[1] == L':') {
        return true;
    }

    // 2. Get Windows directory (e.g. "c:\windows") and system drive (e.g. "c:")
    wchar_t szWinDir[MAX_PATH] = {};
    GetWindowsDirectoryW(szWinDir, MAX_PATH);
    wchar_t winDirNorm[MAX_PATH] = {};
    NormalizePathCommon(szWinDir, winDirNorm, ARRAYSIZE(winDirNorm));

    wchar_t sysDrive[4] = L"c:";
    if (wcslen(winDirNorm) >= 2 && winDirNorm[1] == L':') {
        sysDrive[0] = winDirNorm[0];
        sysDrive[1] = L':';
        sysDrive[2] = L'\0';
    }

    // Protect C:\Windows and everything inside it
    if (PathStartsWithDirPrefix(norm, winDirNorm)) {
        return true;
    }

    // 3. Protect critical system directories on the OS drive
    static const wchar_t* kProtectedPrefixes[] = {
        L"\\system volume information",
        L"\\recovery",
        L"\\$recycle.bin",
        L"\\boot",
        L"\\efi",
        L"\\perflogs",
        L"\\msocache",
        L"\\programdata\\microsoft",
        L"\\program files\\windows defender",
        L"\\program files\\windows nt",
        L"\\program files\\windows security",
        L"\\program files\\windowsapps",
        L"\\program files\\common files\\microsoft shared",
        L"\\program files (x86)\\windows defender",
        L"\\program files (x86)\\windows nt",
        L"\\program files (x86)\\microsoft"
    };
    for (const wchar_t* rel : kProtectedPrefixes) {
        wchar_t full[MAX_PATH] = {};
        StringCchPrintfW(full, ARRAYSIZE(full), L"%s%s", sysDrive, rel);
        if (PathStartsWithDirPrefix(norm, full)) {
            return true;
        }
    }

    static const wchar_t* kProtectedRoots[] = {
        L"\\program files", L"\\program files (x86)", L"\\programdata", L"\\users"
    };
    for (const wchar_t* rel : kProtectedRoots) {
        wchar_t full[MAX_PATH] = {};
        StringCchPrintfW(full, ARRAYSIZE(full), L"%s%s", sysDrive, rel);
        if (wcscmp(norm, full) == 0) {
            return true;
        }
    }

    // 4. Protect critical system & registry files
    const wchar_t* pLeaf = PathFindFileNameW(norm);
    if (pLeaf && *pLeaf) {
        static const wchar_t* kProtectedFiles[] = {
            L"pagefile.sys", L"hiberfil.sys", L"swapfile.sys", L"bootmgr",
            L"bootnxt", L"ntldr", L"ntuser.dat", L"ntuser.dat.log1",
            L"ntuser.dat.log2", L"usrclass.dat", L"sam", L"security",
            L"software", L"system"
        };
        for (const wchar_t* name : kProtectedFiles) {
            if (wcscmp(pLeaf, name) == 0) {
                return true;
            }
        }
    }

    // 5. Protect %LOCALAPPDATA%\Microsoft\Windows shell database/caches
    wchar_t szLocalAppData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szLocalAppData)) && szLocalAppData[0] != L'\0') {
        wchar_t localWinNorm[MAX_PATH] = {};
        NormalizePathCommon(szLocalAppData, localWinNorm, ARRAYSIZE(localWinNorm));
        StringCchCatW(localWinNorm, ARRAYSIZE(localWinNorm), L"\\microsoft\\windows");
        if (PathStartsWithDirPrefix(norm, localWinNorm)) {
            return true;
        }
    }

    return false;
}

// Check if any visible non-Explorer window has the file/folder name open in its title bar
struct WindowTitleMatchCtx {
    wchar_t leafNameLower[MAX_PATH];
    wchar_t leafNoExtLower[MAX_PATH];
    wchar_t fullPathLower[MAX_PATH];
    bool    foundMatch;
    DWORD   matchingPid;
    HWND    matchingHwnd;
};

// True when `token` occurs in `title` (both already lower-cased) as a whole
// word: bounded by whitespace, a separator, or the ends of the title. Guards
// against "a.txt" matching "data.txt".
inline bool TitleHasDistinctToken(const wchar_t* title, const wchar_t* token) {
    size_t tokenLen = wcslen(token);
    if (tokenLen == 0) return false;

    const wchar_t* p = title;
    while ((p = wcsstr(p, token)) != NULL) {
        bool leftOk = (p == title);
        if (!leftOk) {
            wchar_t prev = p[-1];
            leftOk = (prev == L' ' || prev == L'*' || prev == L'\\' || prev == L'/' ||
                      prev == L'[' || prev == L'(' || prev == L'"' || prev == L'\'' || prev == L'-');
        }
        bool rightOk = (p[tokenLen] == L'\0');
        if (!rightOk) {
            wchar_t next = p[tokenLen];
            rightOk = (next == L' ' || next == L'-' || next == L']' || next == L')' ||
                       next == L'"' || next == L'\'' || next == L':' || next == L'*');
        }
        if (leftOk && rightOk && wcscmp(title, token) != 0) {
            return true;
        }
        p += tokenLen;
    }
    return false;
}

inline BOOL CALLBACK EnumWindowsTitleCheckProc(HWND hWnd, LPARAM lParam) {
    if (!IsWindowVisible(hWnd)) return TRUE;

    wchar_t szClass[64] = {};
    if (GetClassNameW(hWnd, szClass, ARRAYSIZE(szClass)) > 0) {
        if (wcscmp(szClass, L"CabinetWClass") == 0 ||
            wcscmp(szClass, L"ExploreWClass") == 0 ||
            wcscmp(szClass, L"Progman") == 0 ||
            wcscmp(szClass, L"WorkerW") == 0 ||
            wcscmp(szClass, L"Shell_TrayWnd") == 0 ||
            wcscmp(szClass, kWndClassName) == 0) {
            return TRUE;
        }
    }

    wchar_t szTitle[512] = {};
    int len = GetWindowTextW(hWnd, szTitle, ARRAYSIZE(szTitle));
    if (len <= 0) return TRUE;

    CharLowerBuffW(szTitle, static_cast<DWORD>(len));

    WindowTitleMatchCtx* ctx = reinterpret_cast<WindowTitleMatchCtx*>(lParam);
    if (ctx->leafNameLower[0] == L'\0') return TRUE;

    bool matched = false;
    if (ctx->fullPathLower[0] != L'\0' && wcsstr(szTitle, ctx->fullPathLower) != NULL) {
        matched = true;
    } else if (TitleHasDistinctToken(szTitle, ctx->leafNameLower)) {
        matched = true;
    } else if (wcslen(ctx->leafNoExtLower) >= 2 &&
               (wcsstr(szTitle, L"notepad") != NULL ||
                wcsstr(szTitle, L"paint") != NULL ||
                wcsstr(szTitle, L"wordpad") != NULL ||
                wcsstr(szTitle, L"photos") != NULL) &&
               TitleHasDistinctToken(szTitle, ctx->leafNoExtLower)) {
        matched = true;
    }

    if (matched) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid != 0 && pid != GetCurrentProcessId()) {
            ctx->foundMatch   = true;
            ctx->matchingPid  = pid;
            ctx->matchingHwnd = hWnd;
            return FALSE;
        }
    }
    return TRUE;
}

inline bool HasWindowWithOpenItemTitle(const wchar_t* szPath, DWORD* pOutPid = nullptr, HWND* pOutHwnd = nullptr) {
    if (!szPath || !*szPath) return false;
    const wchar_t* pFileName = PathFindFileNameW(szPath);
    if (!pFileName || !*pFileName) return false;

    WindowTitleMatchCtx ctx = {};
    StringCchCopyW(ctx.leafNameLower, ARRAYSIZE(ctx.leafNameLower), pFileName);
    CharLowerBuffW(ctx.leafNameLower, static_cast<DWORD>(wcslen(ctx.leafNameLower)));

    StringCchCopyW(ctx.leafNoExtLower, ARRAYSIZE(ctx.leafNoExtLower), ctx.leafNameLower);
    wchar_t* dot = wcsrchr(ctx.leafNoExtLower, L'.');
    if (dot && dot != ctx.leafNoExtLower) {
        *dot = L'\0';
    }

    NormalizePathCommon(szPath, ctx.fullPathLower, ARRAYSIZE(ctx.fullPathLower));
    ctx.foundMatch = false;

    EnumWindows(EnumWindowsTitleCheckProc, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.foundMatch) {
        if (pOutPid) *pOutPid = ctx.matchingPid;
        if (pOutHwnd) *pOutHwnd = ctx.matchingHwnd;
        return true;
    }
    return false;
}

inline bool IsSingleFileLockedCommon(const wchar_t* szFilePath) {
    HANDLE hProbe = CreateFileW(
        szFilePath,
        GENERIC_READ | GENERIC_WRITE | DELETE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hProbe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
            return true;
        }
    } else {
        CloseHandle(hProbe);
    }

    hProbe = CreateFileW(
        szFilePath,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hProbe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
            return true;
        }
    } else {
        CloseHandle(hProbe);
    }

    DWORD dwSession = 0;
    wchar_t szSessionKey[CCH_RM_SESSION_KEY + 1] = {};
    bool rmLocked = false;
    if (RmStartSession(&dwSession, 0, szSessionKey) == ERROR_SUCCESS) {
        PCWSTR pszRes[] = { szFilePath };
        if (RmRegisterResources(dwSession, 1, pszRes, 0, NULL, 0, NULL) == ERROR_SUCCESS) {
            UINT nNeeded = 0, nCount = 8;
            DWORD dwReason = RmRebootReasonNone;
            RM_PROCESS_INFO rgpi[8] = {};
            DWORD res = RmGetList(dwSession, &nNeeded, &nCount, rgpi, &dwReason);
            if (res == ERROR_MORE_DATA || (res == ERROR_SUCCESS && nCount > 0)) {
                DWORD selfPid = GetCurrentProcessId();
                DWORD explorerPid = 0;
                if (GetShellWindow()) GetWindowThreadProcessId(GetShellWindow(), &explorerPid);
                for (UINT i = 0; i < nCount; ++i) {
                    DWORD pid = rgpi[i].Process.dwProcessId;
                    if (pid != selfPid && pid != explorerPid) {
                        rmLocked = true;
                        break;
                    }
                }
            }
        }
        RmEndSession(dwSession);
    }
    if (rmLocked) return true;

    return HasWindowWithOpenItemTitle(szFilePath);
}

inline bool IsSingleFolderLockedCommon(const wchar_t* szFolderPath) {
    HANDLE hDir = CreateFileW(
        szFolderPath,
        DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    if (hDir == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
            return true;
        }
    } else {
        CloseHandle(hDir);
    }

    return HasWindowWithOpenItemTitle(szFolderPath);
}

// Walks a folder looking for anything locked, without collecting the tree
// first. `budget` bounds the work so a huge folder cannot stall Explorer's
// context menu; it is shared across the recursion.
inline bool FolderTreeHasLockedItem(const wchar_t* dirPath, int depth, int* budget) {
    if (depth > 2 || *budget <= 0) return false;

    wchar_t searchPattern[MAX_PATH] = {};
    StringCchPrintfW(searchPattern, ARRAYSIZE(searchPattern), L"%s\\*", dirPath);

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    bool locked = false;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (--(*budget) < 0) break;

        wchar_t full[MAX_PATH] = {};
        StringCchPrintfW(full, ARRAYSIZE(full), L"%s\\%s", dirPath, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (IsSingleFolderLockedCommon(full) ||
                FolderTreeHasLockedItem(full, depth + 1, budget)) {
                locked = true;
                break;
            }
        } else if (IsSingleFileLockedCommon(full)) {
            locked = true;
            break;
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return locked;
}

inline bool IsPathUsedByOtherAppCommon(const wchar_t* szPath) {
    if (!szPath || !*szPath) return false;
    DWORD attrs = GetFileAttributesW(szPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return IsSingleFileLockedCommon(szPath);
    }
    if (IsSingleFolderLockedCommon(szPath)) return true;

    int budget = 64; // same ceiling the old collection used
    return FolderTreeHasLockedItem(szPath, 0, &budget);
}
