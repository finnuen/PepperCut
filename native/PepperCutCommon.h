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
#include <vector>
#include <string>
#include <cmath>

static const wchar_t* kAppName        = L"PepperCut";
static const wchar_t* kAppVersion     = L"1.0";
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

inline std::wstring NormalizePathCommon(const wchar_t* szPath) {
    if (!szPath || !*szPath) return L"";
    std::wstring s(szPath);
    if (s.compare(0, 4, L"\\\\?\\") == 0) {
        s.erase(0, 4);
    }
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) {
        s.pop_back();
    }
    if (!s.empty()) {
        CharLowerBuffW(&s[0], static_cast<DWORD>(s.size()));
    }
    return s;
}

// Converts a normalized path into a safe INI key name
inline std::wstring MakeIniKeyFromPath(const wchar_t* szPath) {
    std::wstring norm = NormalizePathCommon(szPath);
    for (wchar_t& ch : norm) {
        if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'=' || ch == L'[' || ch == L']') {
            ch = L'_';
        }
    }
    return norm;
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
    std::wstring key = MakeIniKeyFromPath(szPath);
    if (key.empty()) return false;

    wchar_t szBootIni[MAX_PATH] = {};
    GetPepperCutBootDeleteIniPath(szBootIni, MAX_PATH);

    wchar_t szStored[MAX_PATH] = {};
    DWORD cch = GetPrivateProfileStringW(L"BootDelete", key.c_str(), L"", szStored, MAX_PATH, szBootIni);
    return (cch > 0 && szStored[0] != L'\0');
}

inline bool PathStartsWithDirPrefix(const std::wstring& pathNorm, const std::wstring& prefixNorm) {
    if (prefixNorm.empty() || pathNorm.empty()) return false;
    if (pathNorm == prefixNorm) return true;
    if (pathNorm.size() > prefixNorm.size() &&
        pathNorm.compare(0, prefixNorm.size(), prefixNorm) == 0 &&
        pathNorm[prefixNorm.size()] == L'\\') {
        return true;
    }
    return false;
}

inline bool IsImportantWindowsPath(const wchar_t* szPath) {
    std::wstring norm = NormalizePathCommon(szPath);
    if (norm.empty()) return false;

    // 1. Protect drive roots (e.g. "c:", "c:\")
    if (norm.size() <= 3 && norm.size() >= 2 && norm[1] == L':') {
        return true;
    }

    // 2. Get Windows directory (e.g. "c:\windows") and system drive (e.g. "c:")
    wchar_t szWinDir[MAX_PATH] = {};
    GetWindowsDirectoryW(szWinDir, MAX_PATH);
    std::wstring winDirNorm = NormalizePathCommon(szWinDir);
    std::wstring sysDrive = (winDirNorm.size() >= 2 && winDirNorm[1] == L':')
        ? winDirNorm.substr(0, 2)
        : L"c:";

    // Protect C:\Windows and everything inside it
    if (PathStartsWithDirPrefix(norm, winDirNorm)) {
        return true;
    }

    // 3. Protect critical system directories on the OS drive
    const std::wstring protectedPrefixes[] = {
        sysDrive + L"\\system volume information",
        sysDrive + L"\\recovery",
        sysDrive + L"\\$recycle.bin",
        sysDrive + L"\\boot",
        sysDrive + L"\\efi",
        sysDrive + L"\\perflogs",
        sysDrive + L"\\msocache",
        sysDrive + L"\\programdata\\microsoft",
        sysDrive + L"\\program files\\windows defender",
        sysDrive + L"\\program files\\windows nt",
        sysDrive + L"\\program files\\windows security",
        sysDrive + L"\\program files\\windowsapps",
        sysDrive + L"\\program files\\common files\\microsoft shared",
        sysDrive + L"\\program files (x86)\\windows defender",
        sysDrive + L"\\program files (x86)\\windows nt",
        sysDrive + L"\\program files (x86)\\microsoft"
    };
    for (const auto& prefix : protectedPrefixes) {
        if (PathStartsWithDirPrefix(norm, prefix)) {
            return true;
        }
    }

    if (norm == sysDrive + L"\\program files" ||
        norm == sysDrive + L"\\program files (x86)" ||
        norm == sysDrive + L"\\programdata" ||
        norm == sysDrive + L"\\users") {
        return true;
    }

    // 4. Protect critical system & registry files
    const wchar_t* pLeaf = PathFindFileNameW(norm.c_str());
    if (pLeaf && *pLeaf) {
        std::wstring leaf(pLeaf);
        if (leaf == L"pagefile.sys" ||
            leaf == L"hiberfil.sys" ||
            leaf == L"swapfile.sys" ||
            leaf == L"bootmgr" ||
            leaf == L"bootnxt" ||
            leaf == L"ntldr" ||
            leaf == L"ntuser.dat" ||
            leaf == L"ntuser.dat.log1" ||
            leaf == L"ntuser.dat.log2" ||
            leaf == L"usrclass.dat" ||
            leaf == L"sam" ||
            leaf == L"security" ||
            leaf == L"software" ||
            leaf == L"system") {
            return true;
        }
    }

    // 5. Protect %LOCALAPPDATA%\Microsoft\Windows shell database/caches
    wchar_t szLocalAppData[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szLocalAppData)) && szLocalAppData[0] != L'\0') {
        std::wstring localWinNorm = NormalizePathCommon(szLocalAppData) + L"\\microsoft\\windows";
        if (PathStartsWithDirPrefix(norm, localWinNorm)) {
            return true;
        }
    }

    return false;
}

// Check if any visible non-Explorer window has the file/folder name open in its title bar
struct WindowTitleMatchCtx {
    std::wstring leafNameLower;
    std::wstring leafNoExtLower;
    std::wstring fullPathLower;
    bool         foundMatch;
    DWORD        matchingPid;
    HWND         matchingHwnd;
};

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
    std::wstring title(szTitle);

    WindowTitleMatchCtx* ctx = reinterpret_cast<WindowTitleMatchCtx*>(lParam);
    if (ctx->leafNameLower.empty()) return TRUE;

    bool matched = false;
    if (!ctx->fullPathLower.empty() && title.find(ctx->fullPathLower) != std::wstring::npos) {
        matched = true;
    } else {
        auto hasDistinctToken = [&](const std::wstring& token) -> bool {
            if (token.empty()) return false;
            size_t pos = 0;
            while ((pos = title.find(token, pos)) != std::wstring::npos) {
                bool leftOk = (pos == 0);
                if (!leftOk) {
                    wchar_t prev = title[pos - 1];
                    leftOk = (prev == L' ' || prev == L'*' || prev == L'\\' || prev == L'/' ||
                              prev == L'[' || prev == L'(' || prev == L'"' || prev == L'\'' || prev == L'-');
                }
                size_t endPos = pos + token.size();
                bool rightOk = (endPos == title.size());
                if (!rightOk) {
                    wchar_t next = title[endPos];
                    rightOk = (next == L' ' || next == L'-' || next == L']' || next == L')' ||
                               next == L'"' || next == L'\'' || next == L':' || next == L'*');
                }
                if (leftOk && rightOk && title != token) {
                    return true;
                }
                pos += token.size();
            }
            return false;
        };

        if (hasDistinctToken(ctx->leafNameLower)) {
            matched = true;
        } else if (!ctx->leafNoExtLower.empty() && ctx->leafNoExtLower.size() >= 2 &&
                   (title.find(L"notepad") != std::wstring::npos ||
                    title.find(L"paint") != std::wstring::npos ||
                    title.find(L"wordpad") != std::wstring::npos ||
                    title.find(L"photos") != std::wstring::npos) &&
                   hasDistinctToken(ctx->leafNoExtLower)) {
            matched = true;
        }
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
    ctx.leafNameLower = pFileName;
    CharLowerBuffW(&ctx.leafNameLower[0], static_cast<DWORD>(ctx.leafNameLower.size()));

    ctx.leafNoExtLower = ctx.leafNameLower;
    size_t dotPos = ctx.leafNoExtLower.find_last_of(L'.');
    if (dotPos != std::wstring::npos && dotPos > 0) {
        ctx.leafNoExtLower.erase(dotPos);
    }

    ctx.fullPathLower = NormalizePathCommon(szPath);
    ctx.foundMatch    = false;

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

inline void CollectItemsInFolderCommon(
    const std::wstring& dirPath,
    std::vector<std::wstring>& outFiles,
    std::vector<std::wstring>& outDirs,
    int depth = 0
) {
    if (depth > 2 || outFiles.size() >= 64) return;
    std::wstring searchPattern = dirPath + L"\\*";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring full = dirPath + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            outDirs.push_back(full);
            CollectItemsInFolderCommon(full, outFiles, outDirs, depth + 1);
        } else {
            outFiles.push_back(full);
            if (outFiles.size() >= 64) break;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

inline bool IsPathUsedByOtherAppCommon(const wchar_t* szPath) {
    if (!szPath || !*szPath) return false;
    DWORD attrs = GetFileAttributesW(szPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    bool isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;

    if (!isDirectory) {
        return IsSingleFileLockedCommon(szPath);
    } else {
        if (IsSingleFolderLockedCommon(szPath)) return true;

        std::vector<std::wstring> childFiles;
        std::vector<std::wstring> childDirs;
        CollectItemsInFolderCommon(szPath, childFiles, childDirs, 0);

        for (const auto& f : childFiles) {
            if (IsSingleFileLockedCommon(f.c_str())) return true;
        }
        for (const auto& d : childDirs) {
            if (IsSingleFolderLockedCommon(d.c_str())) return true;
        }
        return false;
    }
}
