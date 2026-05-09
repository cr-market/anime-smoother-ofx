# Anime Smoother OFX 0.9.10

This release is a C++ speed update for Anime Smoother OFX.

## Highlights

- Faster C++ rendering path.
- Release builds are now enabled by default for local CMake builds.
- `-O3` compiler optimization is enabled for Clang/GCC-style compilers.
- Link-time optimization is enabled when the compiler supports it.
- Pixel comparison now uses a packed 4-byte equality check before per-channel range comparison.
- The OFX wrapper avoids the previous float-pixel intermediate buffer.

## Performance

In the 4K 600-frame development test, macOS render time improved from about 2:43 in v0.9.9 to about 1:07 in v0.9.10.

## Visual Result

The active smoothing algorithm is unchanged. A PNG comparison test between the previous output and the v0.9.10 output found no RGBA pixel differences for the tested frame.

## Install

macOS:

Copy `AnimeSmoother/AnimeSmoother.ofx.bundle` to:

```text
/Library/OFX/Plugins/AnimeSmoother/AnimeSmoother.ofx.bundle
```

Windows:

Copy `AnimeSmoother.ofx.bundle` to:

```text
C:\Program Files\Common Files\OFX\Plugins\AnimeSmoother.ofx.bundle
```

Restart the host application after installing.

## Notes

- Tested mainly in Left Angle Autograph.
- DaVinci Resolve support has been reported in Edit and Fusion contexts, depending on host cache/menu state.
- If an older version remains visible or the effect does not update, quit the host application and clear its OFX plugin cache.

## License

Apache License 2.0.

Anime Smoother OFX includes code adapted from the Apache-2.0 `loilo-inc/smooth` project. Thank you to LoiLo Inc. for releasing the original implementation as open source.
