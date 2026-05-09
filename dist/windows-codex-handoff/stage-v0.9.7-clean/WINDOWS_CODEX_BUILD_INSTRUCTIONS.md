# Windows Codex Build Instructions

Build the Windows x64 release of Anime Smoother OFX v0.9.7.

## Goal

Create this release asset:

```text
AnimeSmoother-0.9.7-Windows.zip
```

The ZIP should contain:

```text
AnimeSmoother/
  AnimeSmoother.ofx.bundle/
    Contents/
      Win64/
        AnimeSmoother.ofx

README.txt
LICENSE.txt
THIRD_PARTY_NOTICES.txt
```

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with "Desktop development with C++"
- CMake
- OpenFX Support Library checkout

`OFX_SUPPORT_ROOT` must point to the OpenFX checkout that contains:

```text
include/
Support/
```

Example:

```text
C:\openfx
```

## Build

From the repository root:

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DOFX_SUPPORT_ROOT=C:\path\to\openfx -DANIME_LINE_BUILD_STANDALONE_TEST=ON
cmake --build build-windows --config Release
```

Run the standalone test:

```powershell
.\build-windows\Release\mlaa_core_test.exe
```

Confirm the plugin exists:

```text
build-windows\ofx-package\AnimeSmoother.ofx.bundle\Contents\Win64\AnimeSmoother.ofx
```

## Package

Create a folder:

```text
release-windows\AnimeSmoother
```

Copy:

```text
build-windows\ofx-package\AnimeSmoother.ofx.bundle
```

to:

```text
release-windows\AnimeSmoother\AnimeSmoother.ofx.bundle
```

Create `release-windows\README.txt` with this content:

```text
Anime Smoother OFX 0.9.7 - Windows

Anime Smoother OFX is a free OpenFX plugin for smoothing jagged edges in anime, line-art, and cel-style artwork.

Tested mainly with Left Angle Autograph.
DaVinci Resolve/Fusion support is experimental.

Installation:
1. Unzip this package.
2. Copy the AnimeSmoother folder to:

   C:\Program Files\Common Files\OFX\Plugins\

   The final path should be:

   C:\Program Files\Common Files\OFX\Plugins\AnimeSmoother\AnimeSmoother.ofx.bundle

3. Restart Autograph or DaVinci Resolve.
4. Look for:

   Filter / Anime / Anime Smoother

Notes:
- Copying into Program Files may require administrator permission.
- If an older AnimeLineSmoother build is installed, remove it to avoid confusion.
- For best results, apply the plugin before enlarging artwork with nearest-neighbor scaling.
```

Copy the root `LICENSE` to:

```text
release-windows\LICENSE.txt
```

Copy the root `THIRD_PARTY_NOTICES.md` to:

```text
release-windows\THIRD_PARTY_NOTICES.txt
```

Zip the contents of `release-windows` as:

```text
AnimeSmoother-0.9.7-Windows.zip
```

Do not put `release-windows` itself as the top-level folder in the ZIP. The ZIP top level should contain:

```text
AnimeSmoother/
README.txt
LICENSE.txt
THIRD_PARTY_NOTICES.txt
```

## Optional install test

Run PowerShell as Administrator:

```powershell
.\install_windows.ps1
```

Then restart the host application.

