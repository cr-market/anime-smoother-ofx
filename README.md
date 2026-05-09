# Anime Smoother OFX

[日本語](#日本語) / [English](#english)

## 日本語

`Anime Smoother OFX` は、アニメ、線画、セル調画像などに出やすいジャギーを軽減するためのCPU版 OpenFX プラグインです。透明エッジ、線画、色の境界を対象に、画像のフラットな塗りをできるだけ保ったまま境界を滑らかにします。

無料で利用でき、ライセンスは Apache License 2.0 です。

このプラグインは、LoiLo Inc. がApache-2.0で公開している After Effects 用プラグイン [`loilo-inc/smooth`](https://github.com/loilo-inc/smooth) の処理を参考にし、OpenFX向けに移植したものです。オープンソースとして公開してくださったLoiLo Inc.に感謝します。OpenFX移植と調整は OpenAI Codex を使って行いました。

### ダウンロード

GitHub Releases から以下のZIPをダウンロードしてください。

- `AnimeSmoother-0.9.10-macOS.zip`
- `AnimeSmoother-0.9.10-Windows.zip`

### インストール

macOS:

1. ZIPを展開します。
2. `AnimeSmoother` フォルダを以下へコピーします。

```text
/Library/OFX/Plugins/
```

最終的に以下の形になればOKです。

```text
/Library/OFX/Plugins/AnimeSmoother/AnimeSmoother.ofx.bundle
```

Windows:

1. ZIPを展開します。
2. `AnimeSmoother.ofx.bundle` を以下へコピーします。

```text
C:\Program Files\Common Files\OFX\Plugins\
```

最終的に以下の形になればOKです。

```text
C:\Program Files\Common Files\OFX\Plugins\AnimeSmoother.ofx.bundle
```

コピー後、AutographまたはDaVinci Resolveを再起動してください。エフェクトは以下に表示されます。

```text
Filter / Anime / Anime Smoother
```

### 確認済みホスト

- Left Angle Autograph で主にテストしています。
- DaVinci Resolve / Fusion でも使用できた報告がありますが、ホスト側のOFXキャッシュや表示場所によって見え方が変わる場合があります。

### 使い方

1. アニメ、線画、セル調画像などの素材に `Anime Smoother` を適用します。
2. まずは `Smoothing Mode` を `Standard` のまま試してください。
3. 効果が弱い場合は `Smoothness` を上げます。
4. 色が近い境界をより拾いたい場合は `Smooth Range` を少し上げます。
5. さらに滑らかにしたい場合は `Smoothing Mode` を `Smooth` または `Extra Smooth` にします。

最近傍法で大きく拡大した後の画像では、元の階段パターンが変わってしまい、スムージング対象として検出しづらい場合があります。できれば拡大前、または適切な解像度の素材に適用してください。

### パラメータ

| 表示名 | 説明 |
| --- | --- |
| `Enabled` | エフェクトのオン／オフです。 |
| `Smoothness` | スムージングの強さです。高くすると効果が強くなります。 |
| `Smooth Range` | 色差をどこまで同じ境界として扱うかの範囲です。近い色の境界に効かせたい場合に上げます。 |
| `Alpha Aware` | アルファ境界を考慮します。通常はオンのままで使います。 |
| `Process Premultiplied` | Premultiplied Alpha素材向けの処理です。通常はオフで、必要な場合だけオンにします。 |
| `Smoothing Mode` | `Standard`、`Smooth`、`Extra Smooth` から選べます。`Extra Smooth` は2回処理するため、細部が少し柔らかくなる場合があります。 |

### 0.9.10 高速化アップデート

v0.9.10ではC++のみで高速化しました。Rustは使用していません。

- Releaseビルドを標準化
- `-O3` とリンク時最適化を有効化
- OFXラッパー内の不要な中間変換を削減
- ピクセル比較の前に4バイト単位の高速な同一判定を追加

開発中の4K 600フレームテストでは、macOS版で v0.9.9 の約2分43秒から v0.9.10 の約1分7秒まで短縮しました。

### ライセンス

Anime Smoother OFX は Apache License 2.0 で公開しています。詳細は [LICENSE](LICENSE) と [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を確認してください。

---

## English

`Anime Smoother OFX` is a CPU OpenFX image effect plugin for smoothing aliased anime/cel-style artwork, transparent edges, line art, and color boundaries.

It is free to use and released under the Apache License 2.0.

Version 0.9.x switches the active renderer to a port of the Apache-2.0 `loilo-inc/smooth` pattern algorithm. The previous MLAA-style, region-based, SSAA, contour, and simplified pattern test cores remain in the source tree for comparison, but the OFX plugin now runs the original up/down/8-connected/lack pattern routines through an OFX pixel wrapper.

The current prototype is designed not to grow silhouettes outward. Fully transparent pixels are left transparent, and alpha smoothing is applied by reducing coverage on the existing inside edge rather than painting new pixels outside the source shape.

## Download

Prebuilt macOS and Windows packages are distributed from GitHub Releases.

- `AnimeSmoother-0.9.10-macOS.zip`
- `AnimeSmoother-0.9.10-Windows.zip`

Each ZIP contains the OFX bundle, install notes, license text, and third-party notices.

## Speed Update

Version 0.9.10 is a C++ speed update without changing the active algorithm or expected visual output:

- splits the frame into horizontal bands
- processes each band on a worker thread with a safety overlap
- merges only the worker's central rows back into the final output
- skips the float-pixel intermediate buffer in the OFX wrapper
- builds Release by default for local CMake builds
- enables `-O3` and link-time optimization when supported
- adds a packed 4-byte equality check before per-channel range comparisons

In the 4K 600-frame test used during development, the macOS build improved from about 2:43 in v0.9.9 to about 1:07 in v0.9.10.

An earlier difference-map prototype was tested but removed from the active path because it added more memory overhead than it saved on typical frames.

## Credits

Anime Smoother OFX includes code adapted from [`loilo-inc/smooth`](https://github.com/loilo-inc/smooth), an Apache-2.0 After Effects smoothing plugin released by LoiLo Inc.

Thank you to LoiLo Inc. for making the original smoothing implementation available as open source.

This OpenFX port was developed with help from OpenAI Codex.

## Features

- OFX `Filter` context
- OFX `General` context for hosts that expose OFX tools differently, such as Fusion
- One `Source` clip and one `Output` clip
- Same-size output
- RGBA and Alpha support
- Byte, Short, and Float pixel depths
- Pattern-based AA prototype for flat-color anime and cel-style images
- Ported `loilo-inc/smooth` upMode, downMode, 8link, and lack routines
- Local corner/protrusion classification using right/up/down/left difference bits
- Triangle-coverage-style corner blending inspired by `loilo-inc/smooth`
- Smooth Range parameter based on the original `range` control
- Color contour smoothing without expanding region silhouettes outward
- Alpha silhouette smoothing by reducing existing inside-edge coverage
- Transparent pixels are preserved to avoid alpha growth
- Flat interiors are preserved unless they are near a detected contour
- SSAA reconstruction prototype retained in source for comparison
- Region-based AA prototype retained in source for comparison
- Quantized label image and local boundary-midpoint distance estimation
- Local color/alpha region detection with coverage-style boundary blending
- Luma, Color, and Luma+Color edge detection
- Alpha coverage smoothing for transparent line art
- Multi-step alpha gradients retained in the legacy region prototype
- Dense inner alpha ramp so the smoothing begins from a stronger line color
- Opaque source pixel preservation so one-pixel lines do not fade
- White-background aware line coverage smoothing
- Different color boundary smoothing
- Conservative color-boundary smoothing for opaque color-to-color transitions
- Symmetric color-boundary smoothing for experimenting without darker-side preservation
- Narrow color-boundary smoothing to avoid a blurred look on detailed artwork
- Direction-aware color-boundary smoothing using RGB channel contrast
- Tangent-oriented color gradients for near-horizontal and near-vertical color boundaries
- Straight-boundary flow smoothing without dark-side priority
- Four-to-eight pixel color gradients along detected boundary flow
- Single-direction boundary flow resolve to avoid smoothing both axes at once
- Direction-only color boundary smoothing with no isotropic color fallback
- Tangent-only color smoothing near stair-step ends; straight horizontal and vertical runs are left untouched
- Centered color-boundary blending to reduce apparent line thickening
- Long-run boundary scoring so near-horizontal edges prefer horizontal gradients over short vertical steps
- Diagonal boundary-flow smoothing for near-45-degree edges
- Boundary-nearest pixels resolve toward the midpoint color before falling off
- Line-aware color smoothing that lets interpolation erode line edges instead of expanding them outward
- Different-line-color protection
- Optional simple diagonal support
- Premultiplied alpha handling

## Files

- `src/MLAACore.h`: host-independent smoothing core
- `src/RegionAACore.h`: previous region-based prototype core
- `src/SSAACore.h`: previous pseudo-SSAA reconstruction core
- `src/ContourAACore.h`: previous constrained contour AA core
- `src/PatternAACore.h`: previous simplified pattern-based AA core
- `src/LoiloSmoothCore.h`: active loilo smooth port wrapper
- `external/smooth`: Apache-2.0 source used for the ported pattern routines
- `src/MLAAEdgeSmoother.cpp`: OpenFX plugin wrapper and parameters
- `tests/mlaa_core_test.cpp`: small standalone regression test for the core algorithm
- `CMakeLists.txt`: build entry point

## Build

The plugin wrapper expects the OpenFX Support Library headers to be available. Point CMake at your OpenFX SDK/support checkout:

```sh
cmake -S . -B build-anime-smoother -DOFX_SUPPORT_ROOT=/path/to/openfx
cmake --build build-anime-smoother
```

For the local SDK installed at `/Users/miyatatoshihide/openfx`:

```sh
/Applications/CMake.app/Contents/bin/cmake -S . -B build-anime-smoother -DOFX_SUPPORT_ROOT=/Users/miyatatoshihide/openfx
/Applications/CMake.app/Contents/bin/cmake --build build-anime-smoother
```

On Windows with Visual Studio 2022:

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 -DOFX_SUPPORT_ROOT=C:\path\to\openfx
cmake --build build-windows --config Release
```

The host-independent core test does not require the OFX SDK:

```sh
cmake -S . -B build -DANIME_LINE_BUILD_STANDALONE_TEST=ON
cmake --build build --target mlaa_core_test
./build/mlaa_core_test
```

On macOS, a successful plugin build produces:

```text
build-anime-smoother/AnimeSmoother.ofx.bundle
```

On Windows, a successful plugin build produces:

```text
build-windows/ofx-package/AnimeSmoother.ofx.bundle/Contents/Win64/AnimeSmoother.ofx
```

Install into the standard macOS OFX folder:

```sh
./install_autograph_macos.sh
```

Install into the standard Windows OFX folder:

```powershell
.\install_windows.ps1
```

The plugin appears as `Anime Smoother` under `Filter/Anime`.

## Parameters

| ID | Label | Type | Default |
| --- | --- | --- | --- |
| `enabled` | Enabled | Boolean | `true` |
| `blendStrength` | Smoothness | Double 0.0-1.0 | `0.75` |
| `smoothRange` | Smooth Range | Double 0.0-20.0 | `1.0` |
| `alphaAware` | Alpha Aware | Boolean | `true` |
| `processPremultiplied` | Process Premultiplied | Boolean | `false` |
| `quality` | Smoothing Mode | Choice: Standard, Smooth, Extra Smooth | `Standard` |

## Notes

This plugin is no longer a strict MLAA implementation. It is tuned for anime and line-art sources where preserving flat line colors matters more than generic edge reconstruction.

`Smoothing Mode` keeps the default look stable in `Standard`. `Smooth` widens the color matching range slightly and is easier to see on close-color boundaries. `Extra Smooth` runs the smoothing pass twice, which can help stubborn stair steps but may soften dense details.

For best results, apply the plugin before enlarging artwork with nearest-neighbor scaling. If an image has already been strongly upscaled with nearest-neighbor, the smoothing pattern may be harder to detect.

## License

Anime Smoother OFX is released under the Apache License 2.0. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
