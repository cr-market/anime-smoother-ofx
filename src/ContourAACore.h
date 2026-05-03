#pragma once

#include "MLAACore.h"

namespace contouraa {

using mlaa::DebugView;
using mlaa::ImageView;
using mlaa::Pixel;
using mlaa::Quality;
using mlaa::Settings;

struct BoundarySample {
    Pixel other {};
    float distance = 0.0f;
    float contrast = 0.0f;
    float straightFactor = 1.0f;
    int nx = 0;
    int ny = 0;
    bool found = false;
    bool alphaBoundary = false;
    bool colorBoundary = false;
};

inline float colorThreshold(const Settings& settings)
{
    return settings.quality == Quality::High ? 0.035f : 0.050f;
}

inline float alphaThreshold(const Settings& settings)
{
    return settings.quality == Quality::High ? 0.020f : 0.035f;
}

inline float contourWidth(const Settings& settings)
{
    return settings.quality == Quality::High ? 2.25f : 1.65f;
}

inline int searchRadius(const Settings& settings)
{
    return settings.quality == Quality::High ? 3 : 2;
}

inline bool isTransparentLike(const Pixel& p, const Settings& settings)
{
    return p.a <= alphaThreshold(settings);
}

inline bool sameRegion(const Pixel& a, const Pixel& b, const Settings& settings)
{
    const bool aTransparent = isTransparentLike(a, settings);
    const bool bTransparent = isTransparentLike(b, settings);
    if (aTransparent || bTransparent) {
        return aTransparent && bTransparent;
    }

    if (std::fabs(a.a - b.a) > alphaThreshold(settings)) {
        return false;
    }
    return mlaa::colorBoundaryDistance(a, b) <= colorThreshold(settings);
}

inline float smoothstep(float edge0, float edge1, float value)
{
    if (std::fabs(edge1 - edge0) <= 1.0e-6f) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = mlaa::clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
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

inline float regionContrast(const Pixel& a, const Pixel& b, const Settings& settings)
{
    float contrast = mlaa::colorBoundaryDistance(a, b);
    if (settings.alphaAware) {
        contrast = std::max(contrast, std::fabs(a.a - b.a));
    }
    return contrast;
}

inline float straightBoundaryFactor(const ImageView& image,
    int x,
    int y,
    int nx,
    int ny,
    const Pixel& center,
    const Pixel& other,
    const Settings& settings)
{
    if (std::abs(nx) + std::abs(ny) != 1) {
        return 1.0f;
    }

    const int tx = ny == 0 ? 0 : 1;
    const int ty = nx == 0 ? 0 : 1;
    int run = 0;
    for (int sign : {-1, 1}) {
        for (int step = 1; step <= 4; ++step) {
            const int sx = x + tx * step * sign;
            const int sy = y + ty * step * sign;
            const Pixel sideA = image.get(sx, sy);
            const Pixel sideB = image.get(sx + nx, sy + ny);
            if (!sameRegion(center, sideA, settings) || !sameRegion(other, sideB, settings)) {
                break;
            }
            ++run;
        }
    }

    if (run >= 6) {
        return 0.10f;
    }
    if (run >= 4) {
        return 0.35f;
    }
    return 1.0f;
}

inline BoundarySample findBoundary(const ImageView& image, int x, int y, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    BoundarySample best;
    if (isTransparentLike(center, settings)) {
        return best;
    }

    const int radius = searchRadius(settings);
    float bestScore = std::numeric_limits<float>::max();

    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            if (xx == 0 && yy == 0) {
                continue;
            }

            const Pixel other = image.get(x + xx, y + yy);
            if (sameRegion(center, other, settings)) {
                continue;
            }

            const bool otherTransparent = isTransparentLike(other, settings);
            if (otherTransparent && !settings.alphaAware) {
                continue;
            }

            const float distance = std::max(0.5f, std::sqrt(static_cast<float>(xx * xx + yy * yy)) - 0.5f);
            const float contrast = regionContrast(center, other, settings);
            const float score = distance - contrast * 0.18f;
            if (score >= bestScore) {
                continue;
            }

            const int nx = xx == 0 ? 0 : (xx < 0 ? -1 : 1);
            const int ny = yy == 0 ? 0 : (yy < 0 ? -1 : 1);
            bestScore = score;
            best.other = other;
            best.distance = distance;
            best.contrast = contrast;
            best.nx = nx;
            best.ny = ny;
            best.found = true;
            best.alphaBoundary = otherTransparent || std::fabs(center.a - other.a) > alphaThreshold(settings);
            best.colorBoundary = !otherTransparent && mlaa::colorBoundaryDistance(center, other) > colorThreshold(settings);
        }
    }

    if (best.found) {
        best.straightFactor = straightBoundaryFactor(image, x, y, best.nx, best.ny, center, best.other, settings);
    }
    return best;
}

inline float boundaryWeight(const BoundarySample& sample, const Settings& settings)
{
    if (!sample.found) {
        return 0.0f;
    }

    const float width = contourWidth(settings);
    float weight = 1.0f - smoothstep(0.35f, width + 0.35f, sample.distance);
    weight *= sample.straightFactor;
    weight *= mlaa::clamp01(settings.blendStrength);
    return mlaa::clamp01(weight);
}

inline Pixel applyBoundary(const Pixel& center, const BoundarySample& sample, const Settings& settings, float weight)
{
    Pixel out = center;

    if (sample.colorBoundary) {
        const float colorMix = 0.42f * weight * mlaa::clamp01(sample.contrast * 2.25f + 0.35f);
        out = blendCoverage(out, sample.other, colorMix);
    }

    if (sample.alphaBoundary && isTransparentLike(sample.other, settings)) {
        const float alphaCut = 0.52f * weight;
        out.a = std::min(out.a, center.a * (1.0f - alphaCut));
        out.r = center.r;
        out.g = center.g;
        out.b = center.b;
    }

    out.a = std::min(out.a, center.a);
    return out;
}

inline Pixel debugPatternPixel(const BoundarySample& sample)
{
    if (!sample.found) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    if (sample.alphaBoundary) {
        return {0.15f, 0.65f, 1.0f, 1.0f};
    }
    if (sample.colorBoundary) {
        return {1.0f, 0.55f, 0.10f, 1.0f};
    }
    return {0.2f, 0.2f, 0.2f, 1.0f};
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
            const Pixel center = base[i];

            Pixel out = center;
            BoundarySample sample;
            float weight = 0.0f;
            if (!isTransparentLike(center, settings)) {
                sample = findBoundary(image, x, y, settings);
                weight = boundaryWeight(sample, settings);
                out = applyBoundary(center, sample, settings, weight);
            }

            if (settings.debugView == DebugView::EdgeMask) {
                out = weight > 0.001f ? Pixel{1.0f, 1.0f, 1.0f, 1.0f} : Pixel{0.0f, 0.0f, 0.0f, 1.0f};
            } else if (settings.debugView == DebugView::BlendWeight) {
                out = {weight, weight, weight, 1.0f};
            } else if (settings.debugView == DebugView::PatternClass) {
                out = debugPatternPixel(sample);
            }

            if (settings.processPremultiplied) {
                out = mlaa::premultiply(out);
            }
            dst[i] = out;
        }
    }
}

} // namespace contouraa
