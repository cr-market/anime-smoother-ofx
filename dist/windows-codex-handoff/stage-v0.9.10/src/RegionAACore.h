#pragma once

#include "MLAACore.h"

#include <unordered_map>

namespace regionaa {

using mlaa::DebugView;
using mlaa::ImageView;
using mlaa::Pixel;
using mlaa::Quality;
using mlaa::Settings;

struct RegionCandidate {
    Pixel color {};
    float distance = 0.0f;
    float score = 0.0f;
    bool found = false;
};

struct Region {
    Pixel color {};
    float count = 0.0f;
    bool transparent = false;
    bool darkLine = false;
};

struct LabelImage {
    std::vector<int> labels;
    std::vector<Region> regions;
    int width = 0;
    int height = 0;

    int labelAt(int x, int y) const
    {
        x = std::max(0, std::min(width - 1, x));
        y = std::max(0, std::min(height - 1, y));
        return labels[static_cast<std::size_t>(y) * width + x];
    }
};

struct BoundarySample {
    Pixel color {};
    float distance = 0.0f;
    float contrast = 0.0f;
    int otherLabel = -1;
    bool found = false;
    bool centerDarkLine = false;
    bool otherDarkLine = false;
};

inline float colorThreshold(const Settings& settings)
{
    if (settings.quality == Quality::Draft) {
        return 0.070f;
    }
    if (settings.quality == Quality::High) {
        return 0.045f;
    }
    return 0.055f;
}

inline float alphaThreshold(const Settings& settings)
{
    return settings.quality == Quality::High ? 0.025f : 0.040f;
}

inline float aaWidth(const Settings& settings)
{
    if (settings.quality == Quality::Draft) {
        return 1.10f;
    }
    if (settings.quality == Quality::High) {
        return 1.75f;
    }
    return 1.40f;
}

inline int searchRadius(const Settings& settings)
{
    return settings.quality == Quality::High ? 3 : 2;
}

inline bool isTransparentRegion(const Pixel& p, const Settings& settings)
{
    return p.a <= alphaThreshold(settings);
}

inline float regionDistance(const Pixel& a, const Pixel& b)
{
    const float color = mlaa::colorDistanceMaxAbs(a, b);
    const float alpha = std::fabs(a.a - b.a);
    return std::max(color, alpha);
}

inline bool sameRegion(const Pixel& a, const Pixel& b, const Settings& settings)
{
    const bool aTransparent = isTransparentRegion(a, settings);
    const bool bTransparent = isTransparentRegion(b, settings);
    if (aTransparent || bTransparent) {
        return aTransparent && bTransparent;
    }
    if (std::fabs(a.a - b.a) > alphaThreshold(settings)) {
        return false;
    }
    return mlaa::colorDistanceMaxAbs(a, b) <= colorThreshold(settings);
}

inline bool isDarkLineRegion(const Pixel& p)
{
    return p.a > 0.35f && mlaa::luma(p) < 0.22f && mlaa::saturation(p) < 0.45f;
}

inline float smoothstep(float edge0, float edge1, float value)
{
    if (edge0 == edge1) {
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

inline int quantizedComponent(float value, float step)
{
    return std::max(0, std::min(255, static_cast<int>(value / step + 0.5f)));
}

inline uint32_t regionKey(const Pixel& p, const Settings& settings)
{
    if (isTransparentRegion(p, settings)) {
        return 0;
    }

    const float cStep = colorThreshold(settings);
    const float aStep = alphaThreshold(settings);
    const uint32_t r = static_cast<uint32_t>(quantizedComponent(p.r, cStep));
    const uint32_t g = static_cast<uint32_t>(quantizedComponent(p.g, cStep));
    const uint32_t b = static_cast<uint32_t>(quantizedComponent(p.b, cStep));
    const uint32_t a = static_cast<uint32_t>(quantizedComponent(p.a, aStep));
    return (r << 24) | (g << 16) | (b << 8) | a;
}

inline LabelImage buildLabels(const Pixel* pixels, int width, int height, const Settings& settings)
{
    LabelImage out;
    out.width = width;
    out.height = height;
    out.labels.assign(static_cast<std::size_t>(width) * height, 0);

    std::unordered_map<uint32_t, int> labelForKey;
    labelForKey.reserve(256);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * width + x;
            const Pixel p = pixels[i];
            const uint32_t key = regionKey(p, settings);

            auto it = labelForKey.find(key);
            if (it == labelForKey.end()) {
                const int label = static_cast<int>(out.regions.size());
                it = labelForKey.emplace(key, label).first;
                Region region;
                region.transparent = isTransparentRegion(p, settings);
                out.regions.push_back(region);
            }

            const int label = it->second;
            out.labels[i] = label;
            Region& region = out.regions[static_cast<std::size_t>(label)];
            region.color.r += p.r;
            region.color.g += p.g;
            region.color.b += p.b;
            region.color.a += p.a;
            region.count += 1.0f;
        }
    }

    for (Region& region : out.regions) {
        if (region.count > 0.0f) {
            const float invCount = 1.0f / region.count;
            region.color.r *= invCount;
            region.color.g *= invCount;
            region.color.b *= invCount;
            region.color.a *= invCount;
        }
        if (region.transparent) {
            region.color = {0.0f, 0.0f, 0.0f, 0.0f};
        }
        region.darkLine = isDarkLineRegion(region.color);
    }
    return out;
}

inline int boundarySearchRadius(const Settings& settings)
{
    if (settings.quality == Quality::High) {
        return 4;
    }
    return 3;
}

inline BoundarySample findNearestBoundary(const LabelImage& labels, int x, int y, const Settings& settings)
{
    constexpr std::array<std::array<int, 2>, 4> directions = {{
        {1, 0}, {0, 1}, {1, 1}, {1, -1},
    }};

    BoundarySample best;
    const int centerLabel = labels.labelAt(x, y);
    const Region& centerRegion = labels.regions[static_cast<std::size_t>(centerLabel)];
    const int radius = boundarySearchRadius(settings);

    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            const int bx0 = x + xx;
            const int by0 = y + yy;

            for (const auto& direction : directions) {
                const int dx = direction[0];
                const int dy = direction[1];
                const int labelA = labels.labelAt(bx0, by0);
                const int labelB = labels.labelAt(bx0 + dx, by0 + dy);
                if (labelA == labelB || (centerLabel != labelA && centerLabel != labelB)) {
                    continue;
                }

                const int otherLabel = centerLabel == labelA ? labelB : labelA;
                const Region& otherRegion = labels.regions[static_cast<std::size_t>(otherLabel)];
                if (otherRegion.darkLine && !centerRegion.darkLine && !centerRegion.transparent) {
                    continue;
                }

                const float mx = static_cast<float>(bx0) + static_cast<float>(dx) * 0.5f;
                const float my = static_cast<float>(by0) + static_cast<float>(dy) * 0.5f;
                float distance = std::sqrt((static_cast<float>(x) - mx) * (static_cast<float>(x) - mx)
                    + (static_cast<float>(y) - my) * (static_cast<float>(y) - my));
                if (dx != 0 && dy != 0) {
                    distance *= 1.08f;
                }

                const float width = aaWidth(settings);
                if (distance > width + 0.65f) {
                    continue;
                }

                const float contrast = regionDistance(centerRegion.color, otherRegion.color);
                const float score = distance - contrast * 0.10f;
                if (!best.found || score < best.distance - best.contrast * 0.10f) {
                    best.color = otherRegion.color;
                    best.distance = distance;
                    best.contrast = contrast;
                    best.otherLabel = otherLabel;
                    best.centerDarkLine = centerRegion.darkLine;
                    best.otherDarkLine = otherRegion.darkLine;
                    best.found = true;
                }
            }
        }
    }
    return best;
}

inline Pixel representativeRegionColor(const ImageView& image,
    int x,
    int y,
    const Pixel& seed,
    const Settings& settings)
{
    const int radius = searchRadius(settings);
    Pixel sum {};
    float total = 0.0f;

    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            const Pixel p = image.get(x + xx, y + yy);
            if (!sameRegion(seed, p, settings)) {
                continue;
            }
            const float d2 = static_cast<float>(xx * xx + yy * yy);
            const float w = 1.0f / (1.0f + d2);
            sum.r += p.r * w;
            sum.g += p.g * w;
            sum.b += p.b * w;
            sum.a += p.a * w;
            total += w;
        }
    }

    if (total <= 0.0f) {
        return seed;
    }
    const float invTotal = 1.0f / total;
    sum.r *= invTotal;
    sum.g *= invTotal;
    sum.b *= invTotal;
    sum.a *= invTotal;
    return sum;
}

inline RegionCandidate findNearestRegion(const ImageView& image, int x, int y, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    const bool centerDarkLine = isDarkLineRegion(center);
    const int radius = searchRadius(settings);
    RegionCandidate best;

    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            if (xx == 0 && yy == 0) {
                continue;
            }
            const float distance = std::sqrt(static_cast<float>(xx * xx + yy * yy));
            if (distance > static_cast<float>(radius) + 0.01f) {
                continue;
            }

            const Pixel p = image.get(x + xx, y + yy);
            if (sameRegion(center, p, settings)) {
                continue;
            }

            const bool otherDarkLine = isDarkLineRegion(p);
            if (otherDarkLine && !centerDarkLine && center.a > alphaThreshold(settings)) {
                continue;
            }

            const float contrast = regionDistance(center, p);
            if (contrast <= colorThreshold(settings) * 0.75f) {
                continue;
            }

            const float score = contrast / (distance + 0.20f);
            if (!best.found || score > best.score) {
                best.color = p;
                best.distance = distance;
                best.score = score;
                best.found = true;
            }
        }
    }

    if (best.found) {
        best.color = representativeRegionColor(image, x, y, best.color, settings);
    }
    return best;
}

inline float coverageForCandidate(const RegionCandidate& candidate, const Settings& settings)
{
    if (!candidate.found) {
        return 0.0f;
    }

    const float width = aaWidth(settings);
    const float boundaryDistance = std::max(0.0f, candidate.distance - 1.0f);
    const float falloff = 1.0f - smoothstep(0.0f, width, boundaryDistance);
    const float strengthScale = 0.85f + 0.15f * mlaa::clamp01(settings.blendStrength);
    return 0.50f * falloff * strengthScale;
}

inline float coverageForBoundary(const BoundarySample& sample, const Settings& settings)
{
    if (!sample.found) {
        return 0.0f;
    }

    const float width = aaWidth(settings);
    const float falloff = 1.0f - smoothstep(0.35f, 0.35f + width, sample.distance);
    if (falloff <= 0.0f) {
        return 0.0f;
    }

    const float contrast = mlaa::clamp01(sample.contrast * 2.0f);
    const float strengthScale = 0.82f + 0.18f * mlaa::clamp01(settings.blendStrength);
    return 0.52f * falloff * (0.72f + 0.28f * contrast) * strengthScale;
}

inline Pixel processPixel(const ImageView& image,
    const LabelImage& labels,
    int x,
    int y,
    const Settings& settings,
    float* coverageOut = nullptr)
{
    const Pixel center = image.get(x, y);
    const BoundarySample sample = findNearestBoundary(labels, x, y, settings);
    float coverage = coverageForBoundary(sample, settings);

    if (!sample.found || coverage <= 0.0f) {
        if (coverageOut) {
            *coverageOut = 0.0f;
        }
        return center;
    }

    if (sample.centerDarkLine) {
        coverage = std::min(coverage, 0.46f);
    } else if (sample.otherDarkLine && center.a > alphaThreshold(settings)) {
        coverage = 0.0f;
    }

    if (coverageOut) {
        *coverageOut = coverage;
    }
    return coverage > 0.0f ? blendCoverage(center, sample.color, coverage) : center;
}

inline void process(const Pixel* src, Pixel* dst, int width, int height, Settings settings)
{
    if (!settings.enabled || settings.blendStrength <= 0.0f || width <= 0 || height <= 0) {
        std::copy(src, src + static_cast<std::size_t>(width) * height, dst);
        return;
    }

    settings.blendStrength = mlaa::clamp01(settings.blendStrength);
    std::vector<Pixel> working(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < working.size(); ++i) {
        working[i] = settings.processPremultiplied ? mlaa::unpremultiply(src[i]) : src[i];
    }

    const LabelImage labels = buildLabels(working.data(), width, height, settings);
    ImageView image(working.data(), width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * width + x;
            float coverage = 0.0f;
            Pixel out = processPixel(image, labels, x, y, settings, &coverage);

            if (settings.debugView == DebugView::EdgeMask) {
                out = mlaa::debugPixel(coverage > 0.0f ? 1.0f : 0.0f);
            } else if (settings.debugView == DebugView::BlendWeight) {
                out = mlaa::debugPixel(coverage);
            } else if (settings.debugView == DebugView::PatternClass) {
                const int label = labels.labelAt(x, y);
                const Region& region = labels.regions[static_cast<std::size_t>(label)];
                const float hue = static_cast<float>((label * 37) % 255) / 255.0f;
                out = region.darkLine ? Pixel{0.0f, 0.0f, 0.0f, 1.0f} : Pixel{hue, 1.0f - hue * 0.5f, 1.0f - hue, 1.0f};
            }

            dst[i] = settings.processPremultiplied ? mlaa::premultiply(out) : out;
        }
    }
}

} // namespace regionaa
