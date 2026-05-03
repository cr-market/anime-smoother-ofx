#pragma once

#include "MLAACore.h"

namespace patternaa {

using mlaa::DebugView;
using mlaa::ImageView;
using mlaa::Pixel;
using mlaa::Quality;
using mlaa::Settings;

struct PatternResult {
    Pixel color {};
    float weight = 0.0f;
    uint8_t mode = 0;
    int patternClass = 0;
    bool found = false;
    bool alphaBoundary = false;
};

inline float alphaThreshold(const Settings& settings)
{
    return settings.quality == Quality::High ? 0.020f : 0.035f;
}

inline float smoothRange(const Settings& settings)
{
    if (settings.quality == Quality::High) {
        return 0.075f;
    }
    return 0.055f;
}

inline bool isTransparentLike(const Pixel& p, const Settings& settings)
{
    return p.a <= alphaThreshold(settings);
}

inline float regionDelta(const Pixel& a, const Pixel& b, const Settings& settings)
{
    float delta = std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b);
    if (settings.alphaAware) {
        delta += std::fabs(a.a - b.a);
    }
    return delta;
}

inline bool sameRegion(const Pixel& a, const Pixel& b, const Settings& settings)
{
    const bool aTransparent = isTransparentLike(a, settings);
    const bool bTransparent = isTransparentLike(b, settings);
    if (aTransparent || bTransparent) {
        return aTransparent && bTransparent;
    }
    return regionDelta(a, b, settings) <= smoothRange(settings);
}

inline Pixel blendCoverage(const Pixel& a, const Pixel& b, float t)
{
    t = mlaa::clamp01(t);
    const float invT = 1.0f - t;
    const float outA = a.a * invT + b.a * t;
    if (outA <= 1.0e-6f) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    Pixel out;
    out.a = outA;
    out.r = (a.r * a.a * invT + b.r * b.a * t) / outA;
    out.g = (a.g * a.a * invT + b.g * b.a * t) / outA;
    out.b = (a.b * a.a * invT + b.b * b.a * t) / outA;
    return out;
}

inline void addPremultiplied(Pixel& sum, float& weightSum, const Pixel& p, float weight)
{
    if (weight <= 0.0f) {
        return;
    }
    sum.r += p.r * p.a * weight;
    sum.g += p.g * p.a * weight;
    sum.b += p.b * p.a * weight;
    sum.a += p.a * weight;
    weightSum += weight;
}

inline Pixel finishPremultipliedAverage(const Pixel& sum, float weightSum)
{
    if (weightSum <= 1.0e-6f) {
        return {};
    }
    const float outA = sum.a / weightSum;
    if (outA <= 1.0e-6f) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return {
        mlaa::clamp01(sum.r / (weightSum * outA)),
        mlaa::clamp01(sum.g / (weightSum * outA)),
        mlaa::clamp01(sum.b / (weightSum * outA)),
        mlaa::clamp01(outA),
    };
}

inline int countSameRun(const ImageView& image, int x, int y, int dx, int dy, const Pixel& center, const Settings& settings)
{
    const int limit = settings.quality == Quality::High ? 6 : 4;
    int count = 0;
    for (int step = 1; step <= limit; ++step) {
        if (!sameRegion(center, image.get(x + dx * step, y + dy * step), settings)) {
            break;
        }
        ++count;
    }
    return count;
}

inline float runFactorForCorner(const ImageView& image,
    int x,
    int y,
    int dx0,
    int dy0,
    int dx1,
    int dy1,
    const Pixel& center,
    const Settings& settings)
{
    const int run0 = countSameRun(image, x, y, -dx0, -dy0, center, settings);
    const int run1 = countSameRun(image, x, y, -dx1, -dy1, center, settings);
    const int run = std::max(run0, run1);
    if (run >= 4) {
        return 1.0f;
    }
    if (run >= 2) {
        return 0.82f;
    }
    return 0.62f;
}

inline int bitCount(uint8_t mode)
{
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        count += (mode & (1 << i)) ? 1 : 0;
    }
    return count;
}

inline bool isAdjacentPair(uint8_t mode)
{
    return mode == 0x03 || mode == 0x05 || mode == 0x0a || mode == 0x0c;
}

inline bool isOppositePair(uint8_t mode)
{
    return mode == 0x06 || mode == 0x09;
}

inline std::array<std::array<int, 2>, 2> cornerDirections(uint8_t mode)
{
    switch (mode) {
    case 0x03:
        return {{{1, 0}, {0, -1}}};
    case 0x05:
        return {{{1, 0}, {0, 1}}};
    case 0x0a:
        return {{{-1, 0}, {0, -1}}};
    case 0x0c:
        return {{{-1, 0}, {0, 1}}};
    default:
        return {{{0, 0}, {0, 0}}};
    }
}

inline Pixel cornerReference(const ImageView& image, int x, int y, const Pixel& center, uint8_t mode, const Settings& settings)
{
    const auto dirs = cornerDirections(mode);
    const int dx0 = dirs[0][0];
    const int dy0 = dirs[0][1];
    const int dx1 = dirs[1][0];
    const int dy1 = dirs[1][1];

    Pixel sum;
    float weightSum = 0.0f;
    const Pixel p0 = image.get(x + dx0, y + dy0);
    const Pixel p1 = image.get(x + dx1, y + dy1);
    addPremultiplied(sum, weightSum, p0, 1.0f);
    addPremultiplied(sum, weightSum, p1, 1.0f);

    const Pixel diagonal = image.get(x + dx0 + dx1, y + dy0 + dy1);
    if (!sameRegion(center, diagonal, settings)) {
        addPremultiplied(sum, weightSum, diagonal, 0.75f);
    }

    return finishPremultipliedAverage(sum, weightSum);
}

inline Pixel protrusionReference(const ImageView& image, int x, int y, const Pixel& center, uint8_t mode, const Settings& settings)
{
    constexpr std::array<std::array<int, 2>, 4> dirs = {{
        {1, 0}, {0, -1}, {0, 1}, {-1, 0},
    }};

    Pixel sum;
    float weightSum = 0.0f;
    for (int i = 0; i < 4; ++i) {
        if (!(mode & (1 << i))) {
            continue;
        }
        const Pixel p = image.get(x + dirs[i][0], y + dirs[i][1]);
        if (!sameRegion(center, p, settings)) {
            addPremultiplied(sum, weightSum, p, 1.0f);
        }
    }
    return finishPremultipliedAverage(sum, weightSum);
}

inline PatternResult classifyPattern(const ImageView& image, int x, int y, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    PatternResult result;
    result.color = center;
    if (isTransparentLike(center, settings)) {
        return result;
    }

    const bool right = !sameRegion(center, image.get(x + 1, y), settings);
    const bool up = !sameRegion(center, image.get(x, y - 1), settings);
    const bool down = !sameRegion(center, image.get(x, y + 1), settings);
    const bool left = !sameRegion(center, image.get(x - 1, y), settings);
    const uint8_t mode = static_cast<uint8_t>((right ? 0x01 : 0) | (up ? 0x02 : 0) | (down ? 0x04 : 0) | (left ? 0x08 : 0));
    result.mode = mode;

    if (mode == 0 || isOppositePair(mode)) {
        return result;
    }

    const int changes = bitCount(mode);
    Pixel ref;
    float baseWeight = 0.0f;

    if (isAdjacentPair(mode)) {
        const auto dirs = cornerDirections(mode);
        ref = cornerReference(image, x, y, center, mode, settings);
        const float run = runFactorForCorner(image, x, y, dirs[0][0], dirs[0][1], dirs[1][0], dirs[1][1], center, settings);
        baseWeight = 0.56f * run;
        result.patternClass = 1;
    } else if (changes == 3) {
        ref = protrusionReference(image, x, y, center, mode, settings);
        baseWeight = 0.42f;
        result.patternClass = 2;
    } else if (changes == 4) {
        ref = protrusionReference(image, x, y, center, mode, settings);
        baseWeight = 0.34f;
        result.patternClass = 3;
    } else {
        return result;
    }

    if (ref.a <= 1.0e-6f && !settings.alphaAware) {
        return result;
    }

    result.alphaBoundary = ref.a <= alphaThreshold(settings) || std::fabs(center.a - ref.a) > alphaThreshold(settings);
    const float contrast = std::max(mlaa::colorBoundaryDistance(center, ref), settings.alphaAware ? std::fabs(center.a - ref.a) : 0.0f);
    result.weight = mlaa::clamp01(baseWeight * mlaa::clamp01(settings.blendStrength) * (0.55f + contrast * 1.35f));
    if (settings.quality == Quality::High) {
        result.weight = mlaa::clamp01(result.weight * 1.18f);
    }
    result.found = result.weight > 0.001f;

    if (!result.found) {
        return result;
    }

    if (result.alphaBoundary && ref.a <= alphaThreshold(settings)) {
        result.color = center;
        result.color.a = std::min(center.a, center.a * (1.0f - result.weight * 0.82f));
    } else {
        result.color = blendCoverage(center, ref, result.weight);
        result.color.a = std::min(result.color.a, std::max(center.a, ref.a));
    }
    return result;
}

inline Pixel debugPatternPixel(const PatternResult& result)
{
    if (!result.found) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    if (result.patternClass == 1) {
        return {1.0f, 0.55f, 0.0f, 1.0f};
    }
    if (result.patternClass == 2) {
        return {0.25f, 0.70f, 1.0f, 1.0f};
    }
    return {0.85f, 0.25f, 1.0f, 1.0f};
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

    ImageView image(base.data(), width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * width + x;
            PatternResult result = classifyPattern(image, x, y, settings);
            Pixel out = result.found ? result.color : base[i];

            if (settings.debugView == DebugView::EdgeMask) {
                out = result.found ? Pixel{1.0f, 1.0f, 1.0f, 1.0f} : Pixel{0.0f, 0.0f, 0.0f, 1.0f};
            } else if (settings.debugView == DebugView::BlendWeight) {
                out = {result.weight, result.weight, result.weight, 1.0f};
            } else if (settings.debugView == DebugView::PatternClass) {
                out = debugPatternPixel(result);
            }

            if (settings.processPremultiplied) {
                out = mlaa::premultiply(out);
            }
            dst[i] = out;
        }
    }
}

} // namespace patternaa
