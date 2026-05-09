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

inline bool exactDifferent(const PF_Pixel8& a, const PF_Pixel8& b)
{
    return a.red != b.red || a.green != b.green || a.blue != b.blue || a.alpha != b.alpha;
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

inline void runLoilo8(std::vector<PF_Pixel8>& inputPixels,
    std::vector<PF_Pixel8>& outputPixels,
    int width,
    int height,
    const Settings& settings)
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

    BlendingInfo<PF_Pixel8> blend_info;
    blend_info.input = &input;
    blend_info.output = &output;
    blend_info.in_ptr = inputPixels.data();
    blend_info.out_ptr = outputPixels.data();
    blend_info.range = smoothRange(settings);
    blend_info.LineWeight = lineWeight(settings);

    const int in_width = width;
    bool lack_flg = false;

    for (int j = extent.top; j < extent.bottom; ++j) {
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

} // namespace loilosmooth
