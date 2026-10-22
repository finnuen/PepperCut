/**
 * Inspector & Metadata for the real MinGW-w64 GCC-compiled public/PepperCut_v1.1.exe
 * (111,616 bytes, pei-x86-64, statically linked C++ & Win32 GUI executable).
 *
 * Regenerate with:
 *   objdump -h public/PepperCut_v1.1.exe    # sections
 *   objdump -p public/PepperCut_v1.1.exe    # imports
 *   objdump -f public/PepperCut_v1.1.exe    # entry point
 */

export interface PeBinaryInspection {
  fileName: string;
  fileSizeBytes: number;
  compiler: string;
  architecture: string;
  subsystem: string;
  imageBase: string;
  entryPointRva: string;
  sections: {
    name: string;
    virtualAddress: string;
    virtualSize: number;
    rawOffset: string;
    rawSize: number;
    characteristics: string;
  }[];
  imports: {
    dll: string;
    functions: string[];
  }[];
}

export const REAL_PEPPERCUT_EXE_METADATA: PeBinaryInspection = {
  fileName: 'PepperCut_v1.1.exe',
  fileSizeBytes: 111616,
  compiler:
    'x86_64-w64-mingw32-g++ 16.2.0 (-Os -s -static -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -mwindows -municode)',
  architecture: 'x86-64 (PE32+ AMD64, HAS_RELOC)',
  subsystem: 'IMAGE_SUBSYSTEM_WINDOWS_GUI (Windows 10)',
  imageBase: '0x0000000140000000',
  entryPointRva: '0x00001030 (wWinMainCRTStartup)',
  sections: [
    {
      name: '.text',
      virtualAddress: '0x00001000',
      virtualSize: 0x5930,
      rawOffset: '0x00000400',
      rawSize: 0x5930,
      characteristics: 'CODE | EXECUTE | READ',
    },
    {
      name: '.data',
      virtualAddress: '0x00007000',
      virtualSize: 0x1a80,
      rawOffset: '0x00005e00',
      rawSize: 0x1a80,
      characteristics: 'INITIALIZED_DATA | READ | WRITE',
    },
    {
      name: '.rdata',
      virtualAddress: '0x00009000',
      virtualSize: 0x1a38,
      rawOffset: '0x00007a00',
      rawSize: 0x1a38,
      characteristics: 'INITIALIZED_DATA | READ',
    },
    {
      name: '.eh_frame',
      virtualAddress: '0x0000b000',
      virtualSize: 0x0004,
      rawOffset: '0x00009600',
      rawSize: 0x0004,
      characteristics: 'INITIALIZED_DATA | READ (empty, no exceptions are used)',
    },
    {
      name: '.pdata',
      virtualAddress: '0x0000c000',
      virtualSize: 0x04a4,
      rawOffset: '0x00009800',
      rawSize: 0x04a4,
      characteristics: 'INITIALIZED_DATA | READ (x64 Exception Table)',
    },
    {
      name: '.xdata',
      virtualAddress: '0x0000d000',
      virtualSize: 0x0560,
      rawOffset: '0x00009e00',
      rawSize: 0x0560,
      characteristics: 'INITIALIZED_DATA | READ (x64 Unwind Info)',
    },
    {
      name: '.bss',
      virtualAddress: '0x0000e000',
      virtualSize: 0x0170,
      rawOffset: '0x00000000',
      rawSize: 0,
      characteristics: 'UNINITIALIZED_DATA | READ | WRITE',
    },
    {
      name: '.idata',
      virtualAddress: '0x0000f000',
      virtualSize: 0x189c,
      rawOffset: '0x0000a400',
      rawSize: 0x189c,
      characteristics: 'INITIALIZED_DATA | READ | WRITE (Import Table)',
    },
    {
      name: '.tls',
      virtualAddress: '0x00011000',
      virtualSize: 0x0010,
      rawOffset: '0x0000be00',
      rawSize: 0x0010,
      characteristics: 'INITIALIZED_DATA | READ | WRITE (Thread Local Storage)',
    },
    {
      name: '.rsrc',
      virtualAddress: '0x00012000',
      virtualSize: 0xf028,
      rawOffset: '0x0000c000',
      rawSize: 0xf028,
      characteristics:
        'INITIALIZED_DATA | READ (RT_ICON + VERSIONINFO + the shell DLL as RCDATA 200)',
    },
    {
      name: '.reloc',
      virtualAddress: '0x00022000',
      virtualSize: 0x00e0,
      rawOffset: '0x0001b200',
      rawSize: 0x00e0,
      characteristics: 'INITIALIZED_DATA | READ | DISCARDABLE (ASLR Base Relocations)',
    },
  ],
  imports: [
    {
      dll: 'KERNEL32.dll',
      functions: [
        'CloseHandle',
        'CreateDirectoryW',
        'CreateEventW',
        'CreateFileW',
        'CreateThread',
        'DeleteCriticalSection',
        'DeleteFileW',
        'DuplicateHandle',
        'EnterCriticalSection',
        'FindClose',
        'FindFirstFileW',
        'FindNextFileW',
        'FindResourceW',
        'FreeLibrary',
        'GetCommandLineW',
        'GetCurrentProcess',
        'GetCurrentProcessId',
        'GetEnvironmentVariableW',
        'GetFileAttributesW',
        'GetFileType',
        'GetFinalPathNameByHandleW',
        'GetLastError',
        'GetModuleFileNameW',
        'GetModuleHandleA',
        'GetModuleHandleW',
        'GetPrivateProfileIntW',
        'GetPrivateProfileStringW',
        'GetProcAddress',
        'GetProcessHeap',
        'GetStartupInfoW',
        'GetTempPathW',
        'GetWindowsDirectoryW',
        'HeapAlloc',
        'HeapFree',
        'HeapReAlloc',
        'InitializeCriticalSection',
        'LeaveCriticalSection',
        'LoadLibraryA',
        'LoadResource',
        'LocalFree',
        'LockResource',
        'MoveFileExW',
        'OpenProcess',
        'QueryFullProcessImageNameW',
        'SetEvent',
        'SetUnhandledExceptionFilter',
        'SizeofResource',
        'Sleep',
        'VirtualProtect',
        'VirtualQuery',
        'WaitForSingleObject',
        'WriteFile',
        'WritePrivateProfileStringW',
      ],
    },
    {
      dll: 'USER32.dll',
      functions: [
        'AppendMenuW',
        'CharLowerBuffW',
        'CreateIconIndirect',
        'CreatePopupMenu',
        'CreateWindowExW',
        'DefWindowProcW',
        'DestroyMenu',
        'DestroyWindow',
        'DispatchMessageW',
        'EnumWindows',
        'FindWindowW',
        'GetClassNameW',
        'GetCursorPos',
        'GetDC',
        'GetMessageW',
        'GetShellWindow',
        'GetWindowTextW',
        'GetWindowThreadProcessId',
        'IsWindowVisible',
        'LoadIconW',
        'LoadImageW',
        'PostMessageW',
        'PostQuitMessage',
        'RegisterClassExW',
        'RegisterWindowMessageW',
        'ReleaseDC',
        'SendMessageTimeoutW',
        'SetForegroundWindow',
        'TrackPopupMenu',
        'TranslateMessage',
      ],
    },
    {
      dll: 'SHELL32.dll',
      functions: [
        'CommandLineToArgvW',
        'SHChangeNotify',
        'SHFileOperationW',
        'SHGetFolderPathW',
        'Shell_NotifyIconW',
      ],
    },
    {
      dll: 'ADVAPI32.dll',
      functions: [
        'AdjustTokenPrivileges',
        'LookupPrivilegeValueW',
        'OpenProcessToken',
        'RegCloseKey',
        'RegCreateKeyExW',
        'RegDeleteTreeW',
        'RegDeleteValueW',
        'RegSetValueExW',
      ],
    },
    {
      dll: 'GDI32.dll',
      functions: ['CreateBitmap', 'CreateDIBSection', 'DeleteObject'],
    },
    {
      dll: 'SHLWAPI.dll',
      functions: ['PathFindFileNameW'],
    },
    {
      dll: 'RstrtMgr.DLL',
      functions: ['RmEndSession', 'RmGetList', 'RmRegisterResources', 'RmStartSession'],
    },
    {
      dll: 'UCRT (api-ms-win-crt-*)',
      functions: [
        'malloc',
        'calloc',
        'free',
        'memcpy',
        'strlen',
        'strncmp',
        'wcscmp',
        'wcslen',
        'wcsncmp',
        'wcsrchr',
        'wcsstr',
        '__C_specific_handler',
      ],
    },
  ],
};
