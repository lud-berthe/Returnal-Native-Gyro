# Build from source

Requirements: Windows x64, Visual Studio 2022 C++ Build Tools (including MASM and a Windows SDK), CMake 3.24+, Python 3.10+.

From the repository root in PowerShell:

```powershell
python tools/fetch_sdl.py
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
python tools/package.py
```

The SDL bootstrap downloads the official SDL 3.4.16 VC development archive and verifies the pinned SHA-256 before extracting it into ignored `_deps/`. An existing official ZIP can be supplied as `python tools/fetch_sdl.py --archive <path>`. MinHook and GamepadMotionHelpers source are included. All origins and hashes are in `dependencies.json`; full license texts are in `LICENSES.txt`.

Outputs:

- `build/Release/ReturnalGyro.dll`: native gyro/mixed-input runtime.
- `build/loader/Release/version.dll`: forwarding loader.
- `dist/ReturnalGyro-1.0.1.zip`: three DLLs plus LICENSES.txt at the archive root. No INI or installer script.
- `dist/ReturnalGyro-1.0.1-SHA256.json`: hashes of the archive and payload.

Tests run offline, without launching Returnal or accessing a physical controller. The development toolchain used MSVC 14.44.35207 and Windows SDK 10.0.26100. Byte-identical binaries across different compilers, paths or SDK versions are not promised.

Edit the JSON files in `localization/`, then run `python tools/generate_localization.py` to update `src/localization.cpp` before rebuilding. All six catalogs must contain the same keys.

The local GamepadMotionHelpers extension initializes gravity from acceleration after a true device reset. Its patch is included under `third_party/patches/`. Steam gyro calibration remains owned by Steam. MinHook source files retain their original copyright notices; only the required source/include directories are shipped here.

Publish source changes as ordinary commits and use version tags for released snapshots. This initial public snapshot does not reconstruct the earlier private development history.
