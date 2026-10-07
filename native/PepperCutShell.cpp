// ============================================================================
// PepperCutShell.cpp — Win32 COM IContextMenu & IShellExtInit Shell Extension
// Dynamically evaluates right-clicked files/folders in Windows 10 Explorer:
//   - Only adds '[icon] Disconnect file' / '[icon] Disconnect folder' and
//     '[icon] Delete on next boot' (or '[icon] Cancel delete on next boot')
//     IF the selected file or folder is currently used by another app!
// ============================================================================

#include "PepperCutCommon.h"
#include <shobjidl.h>
#include <new>

static LONG g_cDllRef = 0;

// Helper to read PepperCut.exe path stored in %APPDATA%\PepperCut\settings.ini
static bool GetPepperCutExePath(wchar_t* outExePath, DWORD cchMax) {
    wchar_t szIni[MAX_PATH] = {};
    GetPepperCutSettingsIniPath(szIni, MAX_PATH);
    DWORD cch = GetPrivateProfileStringW(L"PepperCut", L"ExePath", L"", outExePath, cchMax, szIni);
    return (cch > 0 && outExePath[0] != L'\0');
}

class PepperCutContextMenu : public IShellExtInit, public IContextMenu {
private:
    LONG    m_cRef;
    wchar_t m_szTargetPath[MAX_PATH];
    bool    m_isDirectory;
    bool    m_hasDisconnectCmd;
    bool    m_hasBootDeleteCmd;
    HBITMAP m_hMenuBmp;

public:
    PepperCutContextMenu()
        : m_cRef(1),
          m_isDirectory(false),
          m_hasDisconnectCmd(false),
          m_hasBootDeleteCmd(false),
          m_hMenuBmp(NULL) {
        m_szTargetPath[0] = L'\0';
        InterlockedIncrement(&g_cDllRef);
        m_hMenuBmp = CreatePepperCutMenuBitmap(16);
    }

    ~PepperCutContextMenu() {
        if (m_hMenuBmp) {
            DeleteObject(m_hMenuBmp);
            m_hMenuBmp = NULL;
        }
        InterlockedDecrement(&g_cDllRef);
    }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IContextMenu)) {
            *ppv = static_cast<IContextMenu*>(this);
        } else if (IsEqualIID(riid, IID_IShellExtInit)) {
            *ppv = static_cast<IShellExtInit*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    // IShellExtInit
    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, IDataObject* pDataObj, HKEY) override {
        m_szTargetPath[0] = L'\0';

        if (pDataObj) {
            FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM stg = {};
            if (SUCCEEDED(pDataObj->GetData(&fmt, &stg))) {
                HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
                if (hDrop) {
                    if (DragQueryFileW(hDrop, 0, m_szTargetPath, MAX_PATH) > 0) {
                        // Successfully got selected file/folder path
                    }
                    GlobalUnlock(stg.hGlobal);
                }
                ReleaseStgMedium(&stg);
            }
        }

        if (m_szTargetPath[0] == L'\0' && pidlFolder) {
            SHGetPathFromIDListW(pidlFolder, m_szTargetPath);
        }

        if (m_szTargetPath[0] == L'\0') {
            return E_FAIL;
        }

        DWORD attrs = GetFileAttributesW(m_szTargetPath);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return E_FAIL;
        }
        m_isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
        return S_OK;
    }

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(
        HMENU hMenu,
        UINT indexMenu,
        UINT idCmdFirst,
        UINT,
        UINT uFlags
    ) override {
        if (uFlags & CMF_DEFAULTONLY) {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        // Only show options while PepperCut is running in the background tray
        if (FindWindowW(kWndClassName, NULL) == NULL) {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        if (m_szTargetPath[0] == L'\0') {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        // If "Ignore important windows file and folder" is enabled in tray, hide options on system paths
        if (IsIgnoreImportantWindowsEnabled() && IsImportantWindowsPath(m_szTargetPath)) {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        bool isUsedByOtherApp = IsPathUsedByOtherAppCommon(m_szTargetPath);
        bool isBootScheduled  = IsScheduledForBootDeleteCommon(m_szTargetPath);

        // CRITICAL REQUIREMENT:
        // Only show the options IF the file or folder is used by another app
        // (or if already set to be deleted on next boot by PepperCut so user can cancel it)
        if (!isUsedByOtherApp && !isBootScheduled) {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }

        UINT cmdOffset = 0;

        // Option 1: '[icon] Disconnect file' or '[icon] Disconnect folder'
        if (isUsedByOtherApp) {
            const wchar_t* szDiscText = m_isDirectory ? L"Disconnect folder" : L"Disconnect file";
            MENUITEMINFOW miiDisc = {};
            miiDisc.cbSize     = sizeof(MENUITEMINFOW);
            miiDisc.fMask      = MIIM_ID | MIIM_STRING | MIIM_FTYPE | MIIM_STATE | (m_hMenuBmp ? MIIM_BITMAP : 0);
            miiDisc.fType      = MFT_STRING;
            miiDisc.fState     = MFS_ENABLED;
            miiDisc.wID        = idCmdFirst + cmdOffset;
            miiDisc.dwTypeData = const_cast<LPWSTR>(szDiscText);
            miiDisc.hbmpItem   = m_hMenuBmp;
            InsertMenuItemW(hMenu, indexMenu++, TRUE, &miiDisc);
            m_hasDisconnectCmd = true;
        }
        cmdOffset = 1;

        // Option 2: '[icon] Delete on next boot' or '[icon] Cancel delete on next boot'
        const wchar_t* szBootText = isBootScheduled
            ? L"Cancel delete on next boot"
            : L"Delete on next boot";
        MENUITEMINFOW miiBoot = {};
        miiBoot.cbSize     = sizeof(MENUITEMINFOW);
        miiBoot.fMask      = MIIM_ID | MIIM_STRING | MIIM_FTYPE | MIIM_STATE | (m_hMenuBmp ? MIIM_BITMAP : 0);
        miiBoot.fType      = MFT_STRING;
        miiBoot.fState     = MFS_ENABLED;
        miiBoot.wID        = idCmdFirst + cmdOffset;
        miiBoot.dwTypeData = const_cast<LPWSTR>(szBootText);
        miiBoot.hbmpItem   = m_hMenuBmp;
        InsertMenuItemW(hMenu, indexMenu++, TRUE, &miiBoot);
        m_hasBootDeleteCmd = true;

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 2);
    }

    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici) override {
        if (!pici) return E_POINTER;
        if (HIWORD(pici->lpVerb) != 0) {
            return E_FAIL;
        }

        UINT cmdOffset = LOWORD(pici->lpVerb);
        wchar_t szExePath[MAX_PATH] = {};
        if (!GetPepperCutExePath(szExePath, MAX_PATH)) {
            return E_FAIL;
        }

        const wchar_t* szFlag = nullptr;
        if (cmdOffset == 0 && m_hasDisconnectCmd) {
            szFlag = L"--disconnect";
        } else if (cmdOffset == 1 && m_hasBootDeleteCmd) {
            szFlag = L"--boot-delete";
        } else {
            return E_FAIL;
        }

        wchar_t szCmdLine[MAX_PATH * 2 + 64] = {};
        StringCchPrintfW(szCmdLine, ARRAYSIZE(szCmdLine), L"\"%s\" %s \"%s\"", szExePath, szFlag, m_szTargetPath);

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(NULL, szCmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return S_OK;
        }
        return E_FAIL;
    }

    IFACEMETHODIMP GetCommandString(UINT_PTR, UINT, UINT*, CHAR*, UINT) override {
        return E_NOTIMPL;
    }
};

class PepperCutClassFactory : public IClassFactory {
private:
    LONG m_cRef;

public:
    PepperCutClassFactory() : m_cRef(1) {
        InterlockedIncrement(&g_cDllRef);
    }

    ~PepperCutClassFactory() {
        InterlockedDecrement(&g_cDllRef);
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_cRef);
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) {
            delete this;
        }
        return cRef;
    }

    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        PepperCutContextMenu* pExt = new (std::nothrow) PepperCutContextMenu();
        if (!pExt) return E_OUTOFMEMORY;
        HRESULT hr = pExt->QueryInterface(riid, ppv);
        pExt->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) InterlockedIncrement(&g_cDllRef);
        else InterlockedDecrement(&g_cDllRef);
        return S_OK;
    }
};

extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_PepperCutShellExt)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    PepperCutClassFactory* pFactory = new (std::nothrow) PepperCutClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;
    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow(void) {
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
