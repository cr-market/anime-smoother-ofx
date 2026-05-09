#pragma once

// This core adapts the Apache-2.0 loilo-inc/smooth algorithm to the OFX
// float-pixel wrapper used by Anime Smoother OFX.

#include "MLAACore.h"

#include "util.h"
#include "upMode.h"
#include "downMode.h"
#include "8link.h"
#include "Lack.h"

#include <cstring>
#include <cstdint>
#include <thread>

namespace loilosmooth {

using mlaa::DebugView;
using mlaa::Pixel;
using mlaa::Quality;
using mlaa::Settings;

inline uint8_t toU8(float value)
{
    return static_cast<uint8_t>(mlaa::clamp01(value) * 255.0f + 0.5f);
}

inline PF_Pixel8 toAePixel(const Pixel& p)
{
    PF_Pixel8 out;
    out.alpha = toU8(p.a);
    out.red = toU8(p.r);
    out.green = toU8(p.g);
    out.blue = toU8(p.b);
    return out;
}

inline Pixel fromAePixel(const PF_Pixel8& p)
{
    return {
        static_cast<float>(p.red) / 255.0f,
        static_cast<float>(p.green) / 255.0f,
        static_cast<float>(p.blue) / 255.0f,
        static_cast<float>(p.alpha) / 255.0f,
    };
}

inline PF_Pixel8 unpremultiplyAe(PF_Pixel8 p)
{
    if (p.alpha == 0) {
        return p;
    }
    const unsigned int alpha = p.alpha;
    p.red = static_cast<uint8_t>(std::min(255u, (static_cast<unsigned int>(p.red) * 255u + alpha / 2u) / alpha));
    p.green = static_cast<uint8_t>(std::min(255u, (static_cast<unsigned int>(p.green) * 255u + alpha / 2u) / alpha));
    p.blue = static_cast<uint8_t>(std::min(255u, (static_cast<unsigned int>(p.blue) * 255u + alpha / 2u) / alpha));
    return p;
}

inline PF_Pixel8 premultiplyAe(PF_Pixel8 p)
{
    const unsigned int alpha = p.alpha;
    p.red = static_cast<uint8_t>((static_cast<unsigned int>(p.red) * alpha + 127u) / 255u);
    p.green = static_cast<uint8_t>((static_cast<unsigned int>(p.green) * alpha + 127u) / 255u);
    p.blue = static_cast<uint8_t>((static_cast<unsigned int>(p.blue) * alpha + 127u) / 255u);
    return p;
}

inline bool exactDifferent(const PF_Pixel8& a, const PF_Pixel8& b)
{
    static_assert(sizeof(PF_Pixel8) == sizeof(uint32_t), "PF_Pixel8 must stay 4 bytes");
    uint32_t packedA = 0;
    uint32_t packedB = 0;
    std::memcpy(&packedA, &a, sizeof(packedA));
    std::memcpy(&packedB, &b, sizeof(packedB));
    return packedA != packedB;
}

inline bool rangeDifferent(const PF_Pixel8& a, const PF_Pixel8& b, unsigned int range)
{
    const unsigned int delta =
        static_cast<unsigned int>(std::abs(static_cast<int>(a.red) - static_cast<int>(b.red))) +
        static_cast<unsigned int>(std::abs(static_cast<int>(a.green) - static_cast<int>(b.green))) +
        static_cast<unsigned int>(std::abs(static_cast<int>(a.blue) - static_cast<int>(b.blue))) +
        static_cast<unsigned int>(std::abs(static_cast<int>(a.alpha) - static_cast<int>(b.alpha)));
    return delta > range;
}

inline bool isWhiteOpaque(const PF_Pixel8& p)
{
    return p.alpha != 0 && p.red == 255 && p.green == 255 && p.blue == 255;
}

inline void computeExtent(const std::vector<PF_Pixel8>& pixels, int width, int height, PF_Rect& rect)
{
    int top = 0;
    int left = width;
    int right = 0;
    int bottom = 0;
    bool found = false;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const PF_Pixel8& p = pixels[static_cast<std::size_t>(y) * width + x];
            if (p.alpha == 0 || isWhiteOpaque(p)) {
                continue;
            }
            if (!found) {
                top = y;
            }
            found = true;
            left = std::min(left, x);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }

    if (!found) {
        rect = {};
        return;
    }

    rect.top = std::max(1, top);
    rect.left = std::max(1, left);
    rect.right = std::min(width - 1, right + 1);
    rect.bottom = std::min(height - 1, bottom + 1);
}

struct DifferenceMaps {
    std::vector<uint8_t> right;
    std::vector<uint8_t> up;
    std::vector<uint8_t> down;
    std::vector<uint8_t> left;
    std::vector<uint8_t> activeRows;
    PF_Rect rect {};
    bool hasDifference = false;
};

inline void markActiveRow(std::vector<uint8_t>& rows, int height, int y)
{
    if (y >= 0 && y < height) {
        rows[static_cast<std::size_t>(y)] = 1;
    }
}

inline void markActiveNeighborhood(std::vector<uint8_t>& rows, int height, int y, int radius)
{
    for (int yy = y - radius; yy <= y + radius; ++yy) {
        markActiveRow(rows, height, yy);
    }
}

inline DifferenceMaps buildDifferenceMaps(const std::vector<PF_Pixel8>& pixels,
    int width,
    int height,
    unsigned int range)
{
    DifferenceMaps maps;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    maps.right.assign(pixelCount, 0);
    maps.up.assign(pixelCount, 0);
    maps.down.assign(pixelCount, 0);
    maps.left.assign(pixelCount, 0);
    maps.activeRows.assign(static_cast<std::size_t>(height), 0);

    int top = height;
    int left = width;
    int right = 0;
    int bottom = 0;

    auto markPixel = [&](int x, int y) {
        maps.hasDifference = true;
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
        markActiveNeighborhood(maps.activeRows, height, y, 2);
    };

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * width + x;
            const PF_Pixel8& center = pixels[idx];

            if (rangeDifferent(center, pixels[idx + 1], range)) {
                maps.right[idx] = 1;
                maps.left[idx + 1] = 1;
                markPixel(x, y);
                markPixel(x + 1, y);
            }
            if (rangeDifferent(center, pixels[idx + width], range)) {
                maps.down[idx] = 1;
                maps.up[idx + width] = 1;
                markPixel(x, y);
                markPixel(x, y + 1);
            }
        }
    }

    if (!maps.hasDifference) {
        return maps;
    }

    const int margin = 2;
    maps.rect.top = std::max(1, top - margin);
    maps.rect.left = std::max(1, left - margin);
    maps.rect.right = std::min(width - 1, right + margin + 1);
    maps.rect.bottom = std::min(height - 1, bottom + margin + 1);
    return maps;
}

inline unsigned int smoothRange(const Settings& settings)
{
    const float value = std::max(0.0f, settings.smoothRange);
    const float qualityBoost = settings.quality == Quality::High ? 1.35f : 1.0f;
    return static_cast<unsigned int>(value * 255.0f * 4.0f * qualityBoost / 100.0f + 0.5f);
}

inline float alphaThreshold(const Settings& settings)
{
    return settings.quality == Quality::High ? 0.020f : 0.035f;
}

inline float lineWeight(const Settings& settings)
{
    return 0.5f + 0.5f * mlaa::clamp01(settings.blendStrength);
}

inline void applyDebug(Pixel& out, const Pixel& before, const Pixel& after, DebugView view)
{
    if (view == DebugView::Final) {
        out = after;
        return;
    }

    const float diff = std::max({
        std::fabs(before.r - after.r),
        std::fabs(before.g - after.g),
        std::fabs(before.b - after.b),
        std::fabs(before.a - after.a),
    });

    if (view == DebugView::EdgeMask) {
        out = diff > 1.0f / 255.0f ? Pixel{1.0f, 1.0f, 1.0f, 1.0f} : Pixel{0.0f, 0.0f, 0.0f, 1.0f};
    } else if (view == DebugView::BlendWeight) {
        const float v = mlaa::clamp01(diff * 8.0f);
        out = {v, v, v, 1.0f};
    } else {
        out = diff > 1.0f / 255.0f ? Pixel{1.0f, 0.55f, 0.0f, 1.0f} : Pixel{0.0f, 0.0f, 0.0f, 1.0f};
    }
}

inline void runLoilo8Band(std::vector<PF_Pixel8>& inputPixels,
    std::vector<PF_Pixel8>& outputPixels,
    int width,
    int height,
    const Settings& settings,
    int bandTop,
    int bandBottom)
{
    using PixelType = PF_Pixel8;

    PF_LayerDef input;
    input.width = width;
    input.height = height;
    input.rowbytes = width * static_cast<int>(sizeof(PF_Pixel8));
    input.data = inputPixels.data();

    PF_LayerDef output = input;
    output.data = outputPixels.data();

    outputPixels = inputPixels;

    PF_Rect extent {};
    computeExtent(inputPixels, width, height, extent);
    if (extent.right <= extent.left || extent.bottom <= extent.top) {
        return;
    }

    const unsigned int range = smoothRange(settings);
    if (bandBottom < 0) {
        bandBottom = height;
    }
    const int loopTop = std::max(extent.top, std::max(1, bandTop));
    const int loopBottom = std::min(extent.bottom, std::min(height - 1, bandBottom));
    if (loopBottom <= loopTop) {
        return;
    }

    BlendingInfo<PF_Pixel8> blend_info;
    blend_info.input = &input;
    blend_info.output = &output;
    blend_info.in_ptr = inputPixels.data();
    blend_info.out_ptr = outputPixels.data();
    blend_info.range = range;
    blend_info.LineWeight = lineWeight(settings);

    const int in_width = width;
    bool lack_flg = false;

    for (int j = loopTop; j < loopBottom; ++j) {
        lack_flg = false;
        long in_target = static_cast<long>(j) * in_width + extent.left;
        long out_target = in_target;

        for (int i = extent.left; i < extent.right; ++i, ++in_target, ++out_target) {
            if (lack_flg) {
                lack_flg = false;
                blend_info.i = i;
                blend_info.j = j;
                blend_info.in_target = in_target;
                blend_info.out_target = out_target;
                blend_info.flag = 0;
                LackMode0304Execute(&blend_info);
            }

            if (!exactDifferent(inputPixels[static_cast<std::size_t>(in_target)],
                    inputPixels[static_cast<std::size_t>(in_target + 1)])) {
                continue;
            }

            unsigned char mode_flg = 0;
            blend_info.i = i;
            blend_info.j = j;
            blend_info.in_target = in_target;
            blend_info.out_target = out_target;
            blend_info.flag = 0;
            std::memset(&blend_info.core, 0, sizeof(Cinfo) * 4);

            BlendingInfo<PF_Pixel8>* info = &blend_info;
            if (ComparePixel(in_target, in_target + 1)) {
                mode_flg |= 1 << 0;
            }
            if (ComparePixel(in_target, in_target - in_width)) {
                mode_flg |= 1 << 1;
            }
            if (ComparePixel(in_target, in_target + in_width)) {
                mode_flg |= 1 << 2;
            }
            if (ComparePixel(in_target, in_target - 1)) {
                mode_flg |= 1 << 3;
            }

            if (mode_flg == 0) {
                continue;
            }

            if (i < width - 2 && (mode_flg & (1 << 0))) {
                lack_flg = true;
            }

            float weight = 0.0f;
            switch (mode_flg) {
            case 3:
                if (ComparePixelEqual(in_target - in_width, in_target + 1)
                    && ComparePixel(in_target - in_width + 1, in_target - in_width)
                    && ComparePixel(in_target - in_width + 1, in_target + 1)) {
                    break;
                }

                upMode_LeftCountLength(&blend_info);
                upMode_RightCountLength(&blend_info);
                upMode_TopCountLength(&blend_info);
                upMode_BottomCountLength(&blend_info);

                if (blend_info.core[0].length - blend_info.core[1].length == 1) {
                    blend_info.core[0].start -= 0.5f;
                    blend_info.core[1].start -= 0.5f;
                }
                weight = (blend_info.core[0].flg & CR_FLG_FILL) || (blend_info.core[1].flg & CR_FLG_FILL)
                    ? 0.5f
                    : blend_info.LineWeight;
                blend_info.core[0].end = blend_info.core[0].start - (blend_info.core[0].start - blend_info.core[0].end) * weight;
                blend_info.core[1].end = blend_info.core[1].start + (blend_info.core[1].end - blend_info.core[1].start) * weight;

                if (blend_info.core[3].length - blend_info.core[2].length == 1) {
                    blend_info.core[2].start += 0.5f;
                    blend_info.core[3].start += 0.5f;
                }
                weight = (blend_info.core[2].flg & CR_FLG_FILL) || (blend_info.core[3].flg & CR_FLG_FILL)
                    ? 0.5f
                    : blend_info.LineWeight;
                blend_info.core[2].end = blend_info.core[2].start - (blend_info.core[2].start - blend_info.core[2].end) * weight;
                blend_info.core[3].end = blend_info.core[3].start + (blend_info.core[3].end - blend_info.core[3].start) * weight;

                if (blend_info.core[0].length >= 2 && blend_info.core[3].length >= 2) {
                    LackMode02Execute(&blend_info);
                } else if (blend_info.core[1].length > 0) {
                    blend_info.mode = BLEND_MODE_UP_H;
                    upMode_LeftBlending(&blend_info);
                    upMode_RightBlending(&blend_info);
                    if (blend_info.core[2].length > 1) {
                        upMode_TopBlending(&blend_info);
                        upMode_BottomBlending(&blend_info);
                    }
                } else if (blend_info.core[2].length > 0) {
                    blend_info.mode = BLEND_MODE_UP_V;
                    upMode_TopBlending(&blend_info);
                    upMode_BottomBlending(&blend_info);
                }
                break;

            case 5:
                if (ComparePixelEqual(in_target + in_width, in_target + 1)
                    && ComparePixel(in_target + in_width + 1, in_target + in_width)
                    && ComparePixel(in_target + in_width + 1, in_target + 1)) {
                    break;
                }

                downMode_LeftCountLength(&blend_info);
                downMode_RightCountLength(&blend_info);
                downMode_TopCountLength(&blend_info);
                downMode_BottomCountLength(&blend_info);

                if (blend_info.core[0].length - blend_info.core[1].length == 1) {
                    blend_info.core[0].start -= 0.5f;
                    blend_info.core[1].start -= 0.5f;
                }
                weight = (blend_info.core[0].flg & CR_FLG_FILL) || (blend_info.core[1].flg & CR_FLG_FILL)
                    ? 0.5f
                    : blend_info.LineWeight;
                blend_info.core[0].end = blend_info.core[0].start - (blend_info.core[0].start - blend_info.core[0].end) * weight;
                blend_info.core[1].end = blend_info.core[1].start + (blend_info.core[1].end - blend_info.core[1].start) * weight;

                if (blend_info.core[3].length - blend_info.core[2].length == 1) {
                    blend_info.core[2].start += 0.5f;
                    blend_info.core[3].start += 0.5f;
                }
                weight = (blend_info.core[2].flg & CR_FLG_FILL) || (blend_info.core[3].flg & CR_FLG_FILL)
                    ? 0.5f
                    : blend_info.LineWeight;
                blend_info.core[2].end = blend_info.core[2].start - (blend_info.core[2].start - blend_info.core[2].end) * weight;
                blend_info.core[3].end = blend_info.core[3].start + (blend_info.core[3].end - blend_info.core[3].start) * weight;

                if (blend_info.core[0].length >= 2 && blend_info.core[2].length >= 2) {
                    LackMode01Execute(&blend_info);
                } else if (blend_info.core[1].length > 0) {
                    blend_info.mode = BLEND_MODE_UP_H;
                    downMode_LeftBlending(&blend_info);
                    downMode_RightBlending(&blend_info);
                    if (blend_info.core[3].length > 1) {
                        downMode_TopBlending(&blend_info);
                        downMode_BottomBlending(&blend_info);
                    }
                } else if (blend_info.core[3].length > 0) {
                    blend_info.mode = BLEND_MODE_UP_V;
                    downMode_TopBlending(&blend_info);
                    downMode_BottomBlending(&blend_info);
                }
                break;

            case 7:
                Link8Mode01Execute(&blend_info);
                break;
            case 11:
                Link8Mode02Execute(&blend_info);
                break;
            case 13:
                Link8Mode04Execute(&blend_info);
                break;
            case 15:
                Link8SquareExecute(&blend_info);
                break;
            default:
                break;
            }

            if (i < width - 2) {
                blend_info.i = i + 1;
                blend_info.j = j;
                blend_info.in_target = in_target + 1;
                blend_info.out_target = out_target + 1;
                blend_info.flag = 0;

                mode_flg = 0;
                if (ComparePixel(blend_info.in_target, blend_info.in_target - in_width)) {
                    mode_flg |= 1 << 0;
                }
                if (ComparePixel(blend_info.in_target, blend_info.in_target + in_width)) {
                    mode_flg |= 1 << 1;
                }
                if (ComparePixel(blend_info.in_target, blend_info.in_target + 1)) {
                    mode_flg |= 1 << 2;
                }
                if (mode_flg == 3) {
                    Link8Mode03Execute(&blend_info);
                }
            }
        }
    }
}

inline int chooseThreadCount(int width, int height)
{
    const unsigned int hardware = std::thread::hardware_concurrency();
    const int suggested = hardware == 0 ? 4 : static_cast<int>(hardware);
    const int byHeight = std::max(1, height / 192);
    const int byPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) >= 512u * 512u ? suggested : 1;
    return std::max(1, std::min({8, byHeight, byPixels}));
}

inline void copyRows(const std::vector<PF_Pixel8>& src,
    std::vector<PF_Pixel8>& dst,
    int width,
    int y0,
    int y1)
{
    if (y1 <= y0) {
        return;
    }
    const std::size_t start = static_cast<std::size_t>(y0) * width;
    const std::size_t count = static_cast<std::size_t>(y1 - y0) * width;
    std::copy(src.begin() + static_cast<std::ptrdiff_t>(start),
        src.begin() + static_cast<std::ptrdiff_t>(start + count),
        dst.begin() + static_cast<std::ptrdiff_t>(start));
}

inline void runLoilo8(std::vector<PF_Pixel8>& inputPixels,
    std::vector<PF_Pixel8>& outputPixels,
    int width,
    int height,
    const Settings& settings)
{
    const int threadCount = chooseThreadCount(width, height);
    if (threadCount <= 1) {
        runLoilo8Band(inputPixels, outputPixels, width, height, settings, 0, height);
        return;
    }

    outputPixels = inputPixels;
    const int overlap = 128;
    const int bandHeight = (height + threadCount - 1) / threadCount;
    std::vector<std::vector<PF_Pixel8>> bandOutputs(static_cast<std::size_t>(threadCount));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(threadCount));

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        const int centralTop = threadIndex * bandHeight;
        const int centralBottom = std::min(height, centralTop + bandHeight);
        if (centralTop >= centralBottom) {
            continue;
        }

        workers.emplace_back([&, threadIndex, centralTop, centralBottom]() {
            std::vector<PF_Pixel8>& localOutput = bandOutputs[static_cast<std::size_t>(threadIndex)];
            const int scanTop = std::max(0, centralTop - overlap);
            const int scanBottom = std::min(height, centralBottom + overlap);
            runLoilo8Band(inputPixels, localOutput, width, height, settings, scanTop, scanBottom);
            copyRows(localOutput, outputPixels, width, centralTop, centralBottom);
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }
}

inline void process(const Pixel* src, Pixel* dst, int width, int height, const Settings& settings)
{
    if (!settings.enabled || settings.blendStrength <= 0.0f || width <= 0 || height <= 0) {
        std::copy(src, src + static_cast<std::size_t>(width) * height, dst);
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    std::vector<Pixel> base(src, src + pixelCount);
    if (settings.processPremultiplied) {
        for (Pixel& p : base) {
            p = mlaa::unpremultiply(p);
        }
    }

    std::vector<PF_Pixel8> inputPixels(pixelCount);
    std::vector<PF_Pixel8> outputPixels(pixelCount);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        inputPixels[i] = toAePixel(base[i]);
    }

    const int passes = std::max(1, std::min(2, settings.smoothingPasses));
    runLoilo8(inputPixels, outputPixels, width, height, settings);
    for (int pass = 1; pass < passes; ++pass) {
        inputPixels = outputPixels;
        runLoilo8(inputPixels, outputPixels, width, height, settings);
    }

    for (std::size_t i = 0; i < pixelCount; ++i) {
        Pixel out = fromAePixel(outputPixels[i]);
        if (base[i].a <= alphaThreshold(settings)) {
            out = base[i];
        }

        Pixel debugOut;
        applyDebug(debugOut, base[i], out, settings.debugView);
        out = debugOut;

        if (settings.processPremultiplied) {
            out = mlaa::premultiply(out);
        }
        dst[i] = out;
    }
}

inline void process8(const PF_Pixel8* src, PF_Pixel8* dst, int width, int height, const Settings& settings)
{
    if (!settings.enabled || settings.blendStrength <= 0.0f || width <= 0 || height <= 0) {
        std::copy(src, src + static_cast<std::size_t>(width) * height, dst);
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    std::vector<PF_Pixel8> inputPixels(src, src + pixelCount);
    if (settings.processPremultiplied) {
        for (PF_Pixel8& p : inputPixels) {
            p = unpremultiplyAe(p);
        }
    }

    std::vector<PF_Pixel8> outputPixels(pixelCount);
    const int passes = std::max(1, std::min(2, settings.smoothingPasses));
    runLoilo8(inputPixels, outputPixels, width, height, settings);
    for (int pass = 1; pass < passes; ++pass) {
        inputPixels = outputPixels;
        runLoilo8(inputPixels, outputPixels, width, height, settings);
    }

    const uint8_t alphaCutoff = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, alphaThreshold(settings) * 255.0f + 0.5f)));
    for (std::size_t i = 0; i < pixelCount; ++i) {
        PF_Pixel8 out = inputPixels[i].alpha <= alphaCutoff ? inputPixels[i] : outputPixels[i];
        if (settings.processPremultiplied) {
            out = premultiplyAe(out);
        }
        dst[i] = out;
    }
}

} // namespace loilosmooth
