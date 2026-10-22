standalone app with zero dependencies

simply download `PepperCut_v1.1.exe` in releases and run it

stays in the background, option in tray, integrated in right-click

<img width="306" height="79" alt="2026-10-07_18-54-37" src="https://github.com/user-attachments/assets/1445ba43-2ad7-4542-ba4e-9dec9d0d560b" />
<img width="151" height="48" alt="2026-10-07_18-36-44" src="https://github.com/user-attachments/assets/40612ccc-a2a8-4fca-8992-e4b5be16e77c" />

## Building

`PepperCut_v1.1.exe` is one file: the shell extension DLL is carried as an
`RCDATA` resource and written to `%APPDATA%\PepperCut` on first run.

Requires MinGW-w64 (`g++` and `windres` on `PATH`):

```powershell
cd native
.\build.ps1
```

The release flags are `-Os -s -static`, with `-fno-exceptions -fno-rtti` and
section garbage collection; without them the C++ runtime alone dwarfs the code.
The result is about 110 KB, published as `public\PepperCut_v1.1.exe` (the
version in the name is read from `kAppVersion`).

## License

Licensed under the [Apache License 2.0](LICENSE).
