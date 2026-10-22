# Builds PepperCut.exe, with the shell extension embedded as an RCDATA resource.
# Requires MinGW-w64 (g++ and windres) on PATH.
#
#   .\build.ps1                 # release build, published as ..\public\PepperCut_v<version>.exe
#   .\build.ps1 -SkipCopy       # leave the exe in native\
param(
    [string]$OutDir = (Join-Path $PSScriptRoot '..\public'),
    [switch]$SkipCopy
)

$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    # The published filename carries the version, and the version is read from
    # the one place the app records it rather than repeated here.
    $header = Get-Content (Join-Path $PSScriptRoot 'PepperCutCommon.h') -Raw
    if ($header -notmatch 'kAppVersion\s*=\s*L"([^"]+)"') {
        throw 'could not read kAppVersion from PepperCutCommon.h'
    }
    $version   = $Matches[1]
    $shippedAs = "PepperCut_v$version.exe"

    # -Os: size over speed, this is a tray utility.
    # -s:  strip symbols; the DLL alone loses ~480 KB of them.
    # -static: the DLL is loaded by Explorer out of %APPDATA% with nothing next
    #          to it, so libstdc++/libgcc must be inside the binary.
    # --gc-sections: the shared header pulls in more than either target uses.
    $flags = @(
        '-std=c++17', '-Os', '-s', '-static',
        '-ffunction-sections', '-fdata-sections', '-DNDEBUG',
        '-DUNICODE', '-D_UNICODE',
        # Nothing here throws or uses typeid. Without this, GCC emits cleanup
        # landing pads that pull libstdc++'s entire exception runtime, the
        # pthread TLS behind it and the 80 KB C++ demangler behind that.
        '-fno-exceptions', '-fno-rtti'
    )
    $link = @('-Wl,--gc-sections')
    $libs = @('-lgdi32', '-luuid', '-lole32', '-loleaut32', '-lshell32', '-lshlwapi', '-lrstrtmgr')

    Write-Host 'Building PepperCutShell.dll ...'
    & g++ @flags -shared -o PepperCutShell.dll PepperCutShell.cpp PepperCutShell.def @link @libs
    if ($LASTEXITCODE -ne 0) { throw 'PepperCutShell.dll failed to build' }

    Write-Host 'Compiling PepperCut.rc (icon, version, embedded DLL) ...'
    & windres PepperCut.rc -O coff -o PepperCut_res.o
    if ($LASTEXITCODE -ne 0) { throw 'PepperCut.rc failed to compile' }

    Write-Host 'Linking PepperCut.exe ...'
    & g++ @flags -municode -mwindows -o PepperCut.exe PepperCut.cpp PepperCut_res.o @link @libs
    if ($LASTEXITCODE -ne 0) { throw 'PepperCut.exe failed to build' }

    $exe = Get-Item PepperCut.exe
    $dll = Get-Item PepperCutShell.dll
    '{0,-20} {1,10:N0} bytes' -f 'PepperCut.exe', $exe.Length
    '{0,-20} {1,10:N0} bytes' -f 'PepperCutShell.dll', $dll.Length

    if (-not $SkipCopy) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
        Copy-Item PepperCut.exe -Destination (Join-Path $OutDir $shippedAs) -Force
        Write-Host "Copied to $((Resolve-Path $OutDir).Path)\$shippedAs"
    }
}
finally {
    Pop-Location
}
