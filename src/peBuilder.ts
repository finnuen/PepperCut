/**
 * Inspector & Metadata for the real MinGW-w64 GCC-compiled /public/PepperCut.exe
 * (289,280 bytes, pei-x86-64, statically linked C++ & Win32 GUI executable).
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
  fileName: 'PepperCut.exe',
  fileSizeBytes: 513536,
  compiler: 'x86_64-w64-mingw32-g++ 12.2.0 (-O2 -s -static -mwindows -municode)',
  architecture: 'x86-64 (PE32+ AMD64, HAS_RELOC)',
  subsystem: 'IMAGE_SUBSYSTEM_WINDOWS_GUI (Windows 10)',
  imageBase: '0x0000000140000000',
  entryPointRva: '0x000014D0 (wWinMainCRTStartup)',
  sections: [
    {
      name: '.text',
      virtualAddress: '0x00001000',
      virtualSize: 0x27218,
      rawOffset: '0x00000400',
      rawSize: 0x27400,
      characteristics: 'CODE | EXECUTE | READ',
    },
    {
      name: '.data',
      virtualAddress: '0x00029000',
      virtualSize: 0x00200,
      rawOffset: '0x00027800',
      rawSize: 0x00200,
      characteristics: 'INITIALIZED_DATA | READ | WRITE',
    },
    {
      name: '.rdata',
      virtualAddress: '0x0002A000',
      virtualSize: 0x0ec80,
      rawOffset: '0x00027A00',
      rawSize: 0x0ee00,
      characteristics: 'INITIALIZED_DATA | READ',
    },
    {
      name: '.pdata',
      virtualAddress: '0x00039000',
      virtualSize: 0x02f40,
      rawOffset: '0x00036800',
      rawSize: 0x03000,
      characteristics: 'INITIALIZED_DATA | READ (x64 Exception Table)',
    },
    {
      name: '.xdata',
      virtualAddress: '0x0003C000',
      virtualSize: 0x02cc0,
      rawOffset: '0x00039800',
      rawSize: 0x02e00,
      characteristics: 'INITIALIZED_DATA | READ (x64 Unwind Info)',
    },
    {
      name: '.idata',
      virtualAddress: '0x00041000',
      virtualSize: 0x016dc,
      rawOffset: '0x0003C600',
      rawSize: 0x01800,
      characteristics: 'INITIALIZED_DATA | READ | WRITE (Import Table)',
    },
    {
      name: '.rsrc',
      virtualAddress: '0x00045000',
      virtualSize: 0x08138,
      rawOffset: '0x0003E200',
      rawSize: 0x08200,
      characteristics: 'INITIALIZED_DATA | READ (Multi-Size RT_ICON + VERSIONINFO)',
    },
    {
      name: '.reloc',
      virtualAddress: '0x0004E000',
      virtualSize: 0x00410,
      rawOffset: '0x00046400',
      rawSize: 0x00600,
      characteristics: 'INITIALIZED_DATA | READ | DISCARDABLE (ASLR Base Relocations)',
    },
  ],
  imports: [
    {
      dll: 'KERNEL32.dll',
      functions: [
        'CreateFileW',
        'DuplicateHandle',
        'GetFinalPathNameByHandleW',
        'GetFileType',
        'MoveFileExW',
        'OpenProcess',
        'TerminateProcess',
        'FindFirstFileW',
        'FindNextFileW',
        'DeleteFileW',
        'RemoveDirectoryW',
      ],
    },
    {
      dll: 'USER32.dll',
      functions: [
        'RegisterClassExW',
        'CreateWindowExW',
        'CreatePopupMenu',
        'AppendMenuW',
        'TrackPopupMenu',
        'SetWindowsHookExW',
        'SetTimer',
        'PostMessageW',
        'FindWindowW',
        'CharLowerBuffW',
      ],
    },
    {
      dll: 'SHELL32.dll',
      functions: ['Shell_NotifyIconW', 'CommandLineToArgvW', 'DragQueryFileW'],
    },
    {
      dll: 'ADVAPI32.dll',
      functions: [
        'RegCreateKeyExW',
        'RegSetValueExW',
        'RegQueryValueExW',
        'RegEnumValueW',
        'RegDeleteValueW',
        'RegDeleteTreeW',
        'OpenProcessToken',
        'AdjustTokenPrivileges',
      ],
    },
    {
      dll: 'RstrtMgr.DLL',
      functions: ['RmStartSession', 'RmRegisterResources', 'RmGetList', 'RmEndSession'],
    },
    {
      dll: 'ole32.dll',
      functions: ['CoInitializeEx', 'CoCreateInstance', 'ReleaseStgMedium', 'CoUninitialize'],
    },
  ],
};
