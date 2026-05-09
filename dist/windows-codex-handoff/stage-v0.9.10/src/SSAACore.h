#pragma once

#include "MLAACore.h"

namespace ssaa {

using mlaa::DebugView;
using mlaa::ImageView;
using mlaa::Pixel;
using mlaa::Quality;
using mlaa::ScaleAlgorithm;
using mlaa::Settings;

inline float similarityThreshold(const Settings& settings)
{
    if (settings.quality == Quality::Draft) {
        return 0.060f;
    }
    if (settings.quality == Quality::High) {
        return 0.025f;
    }
    return 0.040f;
}

inline float alphaThreshold(const Settings& settings)
{
    return settings.quality == Quality::High ? 0.025f : 0.040f;
}

inline bool isTransparentLike(const Pixel& p, const Settings& settings)
{
    return p.a <= alphaThreshold(settings);
}

inline bool sameCoverageColor(const Pixel& a, const Pixel& b, const Settings& settings)
{
    const bool aTransparent = isTransparentLike(a, settings);
    const bool bTransparent = isTransparentLike(b, settings);
    if (aTransparent || bTransparent) {
        return aTransparent && bTransparent;
    }

    const float color = mlaa::colorDistanceMaxAbs(a, b);
    const float alpha = std::fabs(a.a - b.a);
    return color <= similarityThreshold(settings) && alpha <= alphaThreshold(settings);
}

inline float differenceAmount(const Pixel& a, const Pixel& b, const Settings& settings)
{
    float diff = mlaa::colorDistanceMaxAbs(a, b);
    if (settings.alphaAware) {
        diff = std::max(diff, std::fabs(a.a - b.a));
    }
    return diff;
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

inline Pixel averageCoverageBlock(const Pixel* src, int srcWidth, int startX, int startY, int scale)
{
    float sumA = 0.0f;
    float sumR = 0.0f;
    float sumG = 0.0f;
    float sumB = 0.0f;

    for (int yy = 0; yy < scale; ++yy) {
        for (int xx = 0; xx < scale; ++xx) {
            const Pixel p = src[static_cast<std::size_t>(startY + yy) * srcWidth + startX + xx];
            sumA += p.a;
            sumR += p.r * p.a;
            sumG += p.g * p.a;
            sumB += p.b * p.a;
        }
    }

    const float invCount = 1.0f / static_cast<float>(scale * scale);
    const float outA = sumA * invCount;
    if (outA <= 1.0e-6f) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    return {
        sumR / sumA,
        sumG / sumA,
        sumB / sumA,
        outA,
    };
}

inline Pixel fromPremultipliedSample(float r, float g, float b, float a)
{
    a = mlaa::clamp01(a);
    if (a <= 1.0e-6f) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    return {
        mlaa::clamp01(r / a),
        mlaa::clamp01(g / a),
        mlaa::clamp01(b / a),
        a,
    };
}

inline void addPremultipliedSample(float& r, float& g, float& b, float& a, const Pixel& p, float weight)
{
    r += p.r * p.a * weight;
    g += p.g * p.a * weight;
    b += p.b * p.a * weight;
    a += p.a * weight;
}

inline Pixel sampleNearest(const ImageView& image, float x, float y)
{
    return image.get(static_cast<int>(std::floor(x + 0.5f)), static_cast<int>(std::floor(y + 0.5f)));
}

inline Pixel sampleBilinear(const ImageView& image, float x, float y)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    addPremultipliedSample(r, g, b, a, image.get(x0, y0), (1.0f - tx) * (1.0f - ty));
    addPremultipliedSample(r, g, b, a, image.get(x0 + 1, y0), tx * (1.0f - ty));
    addPremultipliedSample(r, g, b, a, image.get(x0, y0 + 1), (1.0f - tx) * ty);
    addPremultipliedSample(r, g, b, a, image.get(x0 + 1, y0 + 1), tx * ty);
    return fromPremultipliedSample(r, g, b, a);
}

inline float cubicWeight(float x)
{
    x = std::fabs(x);
    constexpr float a = -0.5f;
    if (x < 1.0f) {
        return (a + 2.0f) * x * x * x - (a + 3.0f) * x * x + 1.0f;
    }
    if (x < 2.0f) {
        return a * x * x * x - 5.0f * a * x * x + 8.0f * a * x - 4.0f * a;
    }
    return 0.0f;
}

inline Pixel sampleBicubic(const ImageView& image, float x, float y)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    float weightSum = 0.0f;
    for (int yy = -1; yy <= 2; ++yy) {
        const float wy = cubicWeight(y - static_cast<float>(iy + yy));
        for (int xx = -1; xx <= 2; ++xx) {
            const float wx = cubicWeight(x - static_cast<float>(ix + xx));
            const float weight = wx * wy;
            weightSum += weight;
            addPremultipliedSample(r, g, b, a, image.get(ix + xx, iy + yy), weight);
        }
    }

    if (std::fabs(weightSum) > 1.0e-6f) {
        const float invWeight = 1.0f / weightSum;
        r *= invWeight;
        g *= invWeight;
        b *= invWeight;
        a *= invWeight;
    }
    return fromPremultipliedSample(r, g, b, a);
}

inline float sinc(float x)
{
    x = std::fabs(x);
    if (x < 1.0e-5f) {
        return 1.0f;
    }
    constexpr float pi = 3.14159265359f;
    const float pix = pi * x;
    return std::sin(pix) / pix;
}

inline float lanczosWeight(float x)
{
    constexpr float radius = 3.0f;
    x = std::fabs(x);
    if (x >= radius) {
        return 0.0f;
    }
    return sinc(x) * sinc(x / radius);
}

inline Pixel sampleLanczos(const ImageView& image, float x, float y)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
    float weightSum = 0.0f;
    for (int yy = -2; yy <= 3; ++yy) {
        const float wy = lanczosWeight(y - static_cast<float>(iy + yy));
        for (int xx = -2; xx <= 3; ++xx) {
            const float wx = lanczosWeight(x - static_cast<float>(ix + xx));
            const float weight = wx * wy;
            weightSum += weight;
            addPremultipliedSample(r, g, b, a, image.get(ix + xx, iy + yy), weight);
        }
    }

    if (std::fabs(weightSum) > 1.0e-6f) {
        const float invWeight = 1.0f / weightSum;
        r *= invWeight;
        g *= invWeight;
        b *= invWeight;
        a *= invWeight;
    }
    return fromPremultipliedSample(r, g, b, a);
}

inline Pixel sampleScaled(const ImageView& image, float x, float y, ScaleAlgorithm algorithm)
{
    switch (algorithm) {
    case ScaleAlgorithm::Nearest:
        return sampleNearest(image, x, y);
    case ScaleAlgorithm::Bilinear:
        return sampleBilinear(image, x, y);
    case ScaleAlgorithm::Bicubic:
        return sampleBicubic(image, x, y);
    case ScaleAlgorithm::Lanczos:
        return sampleLanczos(image, x, y);
    case ScaleAlgorithm::EdgeAware:
    default:
        return sampleNearest(image, x, y);
    }
}

inline void upscaleSampled(const Pixel* src,
    int width,
    int height,
    int scale,
    std::vector<Pixel>& dst,
    ScaleAlgorithm algorithm)
{
    const int outWidth = width * scale;
    const int outHeight = height * scale;
    dst.assign(static_cast<std::size_t>(outWidth) * outHeight, {});

    ImageView image(src, width, height);
    const float invScale = 1.0f / static_cast<float>(scale);
    for (int y = 0; y < outHeight; ++y) {
        const float sy = (static_cast<float>(y) + 0.5f) * invScale - 0.5f;
        for (int x = 0; x < outWidth; ++x) {
            const float sx = (static_cast<float>(x) + 0.5f) * invScale - 0.5f;
            dst[static_cast<std::size_t>(y) * outWidth + x] = sampleScaled(image, sx, sy, algorithm);
        }
    }
}

inline std::array<Pixel, 4> scale2xBlock(const ImageView& image, int x, int y, const Settings& settings)
{
    const Pixel e = image.get(x, y);
    const Pixel b = image.get(x, y - 1);
    const Pixel d = image.get(x - 1, y);
    const Pixel f = image.get(x + 1, y);
    const Pixel h = image.get(x, y + 1);

    std::array<Pixel, 4> out = {e, e, e, e};

    if (!sameCoverageColor(b, h, settings) && !sameCoverageColor(d, f, settings)) {
        if (sameCoverageColor(d, b, settings)) {
            out[0] = d;
        }
        if (sameCoverageColor(b, f, settings)) {
            out[1] = f;
        }
        if (sameCoverageColor(d, h, settings)) {
            out[2] = d;
        }
        if (sameCoverageColor(h, f, settings)) {
            out[3] = f;
        }
    }

    return out;
}

inline void scale2x(const Pixel* src, int width, int height, std::vector<Pixel>& dst, const Settings& settings)
{
    const int outWidth = width * 2;
    dst.assign(static_cast<std::size_t>(outWidth) * height * 2, {});

    ImageView image(src, width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::array<Pixel, 4> block = scale2xBlock(image, x, y, settings);
            const int ox = x * 2;
            const int oy = y * 2;
            dst[static_cast<std::size_t>(oy) * outWidth + ox] = block[0];
            dst[static_cast<std::size_t>(oy) * outWidth + ox + 1] = block[1];
            dst[static_cast<std::size_t>(oy + 1) * outWidth + ox] = block[2];
            dst[static_cast<std::size_t>(oy + 1) * outWidth + ox + 1] = block[3];
        }
    }
}

inline int internalScale(const Settings& settings)
{
    return settings.quality == Quality::High ? 4 : 2;
}

inline float finalBlendAmount(const Settings& settings)
{
    return mlaa::clamp01(settings.blendStrength);
}

inline Pixel debugChangedPixel(float diff)
{
    diff = mlaa::clamp01(diff * 8.0f);
    return {diff, diff, diff, 1.0f};
}

inline Pixel debugPatternPixel(const Pixel& original, const Pixel& reconstructed, const Settings& settings)
{
    const float colorDiff = mlaa::colorDistanceMaxAbs(original, reconstructed);
    const float alphaDiff = std::fabs(original.a - reconstructed.a);
    if (alphaDiff > 0.005f) {
        return {0.20f, 0.65f, 1.0f, 1.0f};
    }
    if (colorDiff > similarityThreshold(settings) * 0.50f) {
        return {1.0f, 0.55f, 0.10f, 1.0f};
    }
    return {0.0f, 0.0f, 0.0f, 1.0f};
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

    std::vector<Pixel> highA;
    std::vector<Pixel> highB;
    const int scale = internalScale(settings);
    int highWidth = width * scale;
    if (settings.scaleAlgorithm == ScaleAlgorithm::EdgeAware) {
        scale2x(base.data(), width, height, highA, settings);
        if (scale == 4) {
            scale2x(highA.data(), width * 2, height * 2, highB, settings);
            highWidth = width * 4;
        }
    } else {
        upscaleSampled(base.data(), width, height, scale, highA, settings.scaleAlgorithm);
    }

    const Pixel* highPixels = settings.scaleAlgorithm == ScaleAlgorithm::EdgeAware && scale == 4 ? highB.data() : highA.data();
    std::vector<Pixel> reconstructed(pixelCount);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            reconstructed[static_cast<std::size_t>(y) * width + x] =
                averageCoverageBlock(highPixels, highWidth, x * scale, y * scale, scale);
        }
    }

    const float amount = finalBlendAmount(settings);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        Pixel out;
        if (settings.debugView == DebugView::EdgeMask) {
            out = differenceAmount(base[i], reconstructed[i], settings) > 0.005f
                ? Pixel{1.0f, 1.0f, 1.0f, 1.0f}
                : Pixel{0.0f, 0.0f, 0.0f, 1.0f};
        } else if (settings.debugView == DebugView::BlendWeight) {
            out = debugChangedPixel(differenceAmount(base[i], reconstructed[i], settings));
        } else if (settings.debugView == DebugView::PatternClass) {
            out = debugPatternPixel(base[i], reconstructed[i], settings);
        } else {
            out = blendCoverage(base[i], reconstructed[i], amount);
        }

        if (settings.processPremultiplied) {
            out = mlaa::premultiply(out);
        }
        dst[i] = out;
    }
}

} // namespace ssaa
