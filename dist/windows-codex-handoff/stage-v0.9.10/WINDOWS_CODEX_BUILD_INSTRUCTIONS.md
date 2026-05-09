# Windows Codex Build Instructions - Anime Smoother OFX 0.9.10

Build the Windows x64 release package for Anime Smoother OFX v0.9.10.

## Goal

Create this ZIP:

```text
AnimeSmoother-0.9.10-Windows.zip
```

The ZIP should contain:

```text
AnimeSmoother.ofx.bundle/Contents/Win64/AnimeSmoother.ofx
README.txt
LICENSE.txt
THIRD_PARTY_NOTICES.txt
```

## Requirements

- Windows 10 or newer
- Visual Studio 2022 with C++ tools
- CMake 3.20 or newer
- OpenFX SDK / OpenFX Support Library checkout

## Build

From this source folder:

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DOFX_SUPPORT_ROOT="C:\path\to\openfx" -DANIME_LINE_BUILD_STANDALONE_TEST=ON
cmake --build build-windows --config Release
.\build-windows\Release\mlaa_core_test.exe
```

The plugin should be produced at:

```text
build-windows\ofx-package\AnimeSmoother.ofx.bundle\Contents\Win64\AnimeSmoother.ofx
```

## Package

Create a folder containing:

```text
AnimeSmoother.ofx.bundle
README.txt
LICENSE.txt
THIRD_PARTY_NOTICES.txt
```

Use this install destination in the README:

```text
C:\Program Files\Common Files\OFX\Plugins\AnimeSmoother.ofx.bundle
```

## Release Notes

v0.9.10 is the C++ speed update:

- Release build by default for local CMake builds.
- `-O3` for Clang/GCC-style compilers.
- Link-time optimization when available.
- Direct 8-bit OFX wrapper path from v0.9.9.
- Packed 4-byte equality check before per-channel range comparisons.

Do not use Rust. This release must stay C++ only.
