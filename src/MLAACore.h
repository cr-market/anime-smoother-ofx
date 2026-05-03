#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace mlaa {

enum class EdgeMode {
    Luma = 0,
    Color = 1,
    LumaColor = 2,
};

enum class DebugView {
    Final = 0,
    EdgeMask = 1,
    BlendWeight = 2,
    PatternClass = 3,
};

enum class Quality {
    Draft = 0,
    Standard = 1,
    High = 2,
};

enum class FalloffCurve {
    Linear = 0,
    EaseIn = 1,
    EaseOut = 2,
    EaseInOut = 3,
    Dome = 4,
    Bell = 5,
    Needle = 6,
};

enum class ScaleAlgorithm {
    Nearest = 0,
    Bilinear = 1,
    Bicubic = 2,
    Lanczos = 3,
    EdgeAware = 4,
};

struct Settings {
    bool enabled = true;
    EdgeMode edgeMode = EdgeMode::LumaColor;
    float edgeThreshold = 0.08f;
    float blendStrength = 0.75f;
    int maxSearchLength = 8;
    float cornerProtection = 0.6f;
    bool diagonalSupport = false;
    bool alphaAware = true;
    bool processPremultiplied = true;
    Quality quality = Quality::Standard;
    float smoothRange = 1.0f;
    int smoothingPasses = 1;
    ScaleAlgorithm scaleAlgorithm = ScaleAlgorithm::EdgeAware;
    FalloffCurve colorFalloff = FalloffCurve::EaseInOut;
    DebugView debugView = DebugView::Final;
};

struct Pixel {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

inline float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

inline Pixel lerp(const Pixel& a, const Pixel& b, float t)
{
    t = clamp01(t);
    return {
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t,
    };
}

inline Pixel unpremultiply(Pixel p)
{
    if (p.a > 1.0e-6f) {
        const float invA = 1.0f / p.a;
        p.r *= invA;
        p.g *= invA;
        p.b *= invA;
    }
    return p;
}

inline Pixel premultiply(Pixel p)
{
    p.r *= p.a;
    p.g *= p.a;
    p.b *= p.a;
    return p;
}

inline float luma(const Pixel& p)
{
    return 0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b;
}

inline float colorDistanceMaxAbs(const Pixel& a, const Pixel& b)
{
    return std::max({std::fabs(a.r - b.r), std::fabs(a.g - b.g), std::fabs(a.b - b.b)});
}

inline float colorBoundaryDistance(const Pixel& a, const Pixel& b)
{
    const float dr = std::fabs(a.r - b.r);
    const float dg = std::fabs(a.g - b.g);
    const float db = std::fabs(a.b - b.b);
    const float maxChannel = std::max({dr, dg, db});
    const float avgChannel = (dr + dg + db) / 3.0f;
    return maxChannel * 0.72f + avgChannel * 0.28f;
}

inline float edgeDistance(const Pixel& a, const Pixel& b, const Settings& settings)
{
    float diff = 0.0f;
    if (settings.edgeMode == EdgeMode::Luma || settings.edgeMode == EdgeMode::LumaColor) {
        diff = std::max(diff, std::fabs(luma(a) - luma(b)));
    }
    if (settings.edgeMode == EdgeMode::Color || settings.edgeMode == EdgeMode::LumaColor) {
        diff = std::max(diff, colorDistanceMaxAbs(a, b));
    }
    if (settings.alphaAware) {
        diff = std::max(diff, std::fabs(a.a - b.a));
    }
    return diff;
}

class ImageView {
public:
    ImageView(const Pixel* pixels, int width, int height)
        : pixels_(pixels), width_(width), height_(height)
    {
    }

    Pixel get(int x, int y) const
    {
        x = std::max(0, std::min(width_ - 1, x));
        y = std::max(0, std::min(height_ - 1, y));
        return pixels_[static_cast<std::size_t>(y) * width_ + x];
    }

private:
    const Pixel* pixels_;
    int width_;
    int height_;
};

struct EdgeMaps {
    std::vector<uint8_t> horizontal;
    std::vector<uint8_t> vertical;
};

inline int effectiveSearchLength(const Settings& settings)
{
    int length = std::max(2, std::min(32, settings.maxSearchLength));
    if (settings.quality == Quality::Draft) {
        length = std::min(length, 4);
    } else if (settings.quality == Quality::High) {
        length = std::max(length, 12);
    }
    return length;
}

inline EdgeMaps buildEdgeMaps(const Pixel* src, int width, int height, const Settings& settings)
{
    EdgeMaps maps;
    maps.horizontal.assign(static_cast<std::size_t>(width) * height, 0);
    maps.vertical.assign(static_cast<std::size_t>(width) * height, 0);
    ImageView image(src, width, height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Pixel center = image.get(x, y);
            const std::size_t i = static_cast<std::size_t>(y) * width + x;
            if (x + 1 < width && edgeDistance(center, image.get(x + 1, y), settings) > settings.edgeThreshold) {
                maps.vertical[i] = 1;
            }
            if (y + 1 < height && edgeDistance(center, image.get(x, y + 1), settings) > settings.edgeThreshold) {
                maps.horizontal[i] = 1;
            }
        }
    }
    return maps;
}

inline int countRun(const std::vector<uint8_t>& map, int width, int height, int x, int y, int dx, int dy, int maxLength)
{
    int count = 0;
    for (int step = 1; step <= maxLength; ++step) {
        const int xx = x + dx * step;
        const int yy = y + dy * step;
        if (xx < 0 || yy < 0 || xx >= width || yy >= height) {
            break;
        }
        if (!map[static_cast<std::size_t>(yy) * width + xx]) {
            break;
        }
        ++count;
    }
    return count;
}

inline Pixel debugPixel(float value)
{
    value = clamp01(value);
    return {value, value, value, 1.0f};
}

inline Pixel patternPixel(bool hEdge, bool vEdge, bool diagonal)
{
    if (hEdge && vEdge) {
        return {1.0f, 0.55f, 0.0f, 1.0f};
    }
    if (diagonal) {
        return {0.0f, 0.45f, 1.0f, 1.0f};
    }
    if (hEdge) {
        return {0.0f, 1.0f, 0.35f, 1.0f};
    }
    if (vEdge) {
        return {1.0f, 0.15f, 0.2f, 1.0f};
    }
    return {0.0f, 0.0f, 0.0f, 1.0f};
}

inline bool hasLocalContrast(const ImageView& image, int x, int y, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    float maxDiff = 0.0f;
    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            if (xx == 0 && yy == 0) {
                continue;
            }
            maxDiff = std::max(maxDiff, edgeDistance(center, image.get(x + xx, y + yy), settings));
        }
    }
    return maxDiff > settings.edgeThreshold;
}

inline float saturation(const Pixel& p)
{
    const float maxChannel = std::max({p.r, p.g, p.b});
    const float minChannel = std::min({p.r, p.g, p.b});
    return maxChannel - minChannel;
}

inline float colorStrength(const Pixel& p)
{
    return (1.0f - luma(p)) + saturation(p) * 0.25f;
}

inline float applyFalloffCurve(float t, FalloffCurve curve)
{
    t = clamp01(t);
    switch (curve) {
    case FalloffCurve::EaseIn:
        return t * t;
    case FalloffCurve::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case FalloffCurve::EaseInOut:
        return t * t * (3.0f - 2.0f * t);
    case FalloffCurve::Dome:
        return std::sin(t * 1.57079632679f);
    case FalloffCurve::Bell:
        return std::sin(t * 3.14159265359f);
    case FalloffCurve::Needle:
        return std::pow(t, 0.35f) * (1.0f - 0.35f * t);
    case FalloffCurve::Linear:
    default:
        return t;
    }
}

inline bool isBackgroundLike(const Pixel& p)
{
    return p.a < 0.01f || (luma(p) > 0.94f && saturation(p) < 0.08f);
}

inline bool isTransparentLike(const Pixel& p)
{
    return p.a < 0.01f;
}

inline bool isCompatibleLineColor(const Pixel& a, const Pixel& b)
{
    if (isBackgroundLike(a) || isBackgroundLike(b)) {
        return true;
    }
    return colorDistanceMaxAbs(a, b) < 0.18f;
}

inline bool isCompatibleBoundaryColor(const Pixel& a, const Pixel& b)
{
    if (isTransparentLike(a) || isTransparentLike(b)) {
        return true;
    }
    return colorBoundaryDistance(a, b) < 0.045f;
}

struct BoundaryGradient {
    float gx = 0.0f;
    float gy = 0.0f;
};

struct FlowSample {
    Pixel color {};
    float strength = 0.0f;
    int length = 0;
};

inline BoundaryGradient colorBoundaryGradient(const ImageView& image, int x, int y)
{
    BoundaryGradient gradient;
    gradient.gx += colorBoundaryDistance(image.get(x - 1, y), image.get(x + 1, y)) * 0.50f;
    gradient.gx += colorBoundaryDistance(image.get(x - 1, y - 1), image.get(x + 1, y - 1)) * 0.25f;
    gradient.gx += colorBoundaryDistance(image.get(x - 1, y + 1), image.get(x + 1, y + 1)) * 0.25f;

    gradient.gy += colorBoundaryDistance(image.get(x, y - 1), image.get(x, y + 1)) * 0.50f;
    gradient.gy += colorBoundaryDistance(image.get(x - 1, y - 1), image.get(x - 1, y + 1)) * 0.25f;
    gradient.gy += colorBoundaryDistance(image.get(x + 1, y - 1), image.get(x + 1, y + 1)) * 0.25f;
    return gradient;
}

inline float directionalBoundaryWeight(int xx, int yy, const BoundaryGradient& gradient)
{
    const float len = std::sqrt(static_cast<float>(xx * xx + yy * yy));
    if (len <= 0.0f) {
        return 0.0f;
    }

    const float sum = gradient.gx + gradient.gy;
    if (sum <= 1.0e-5f) {
        return 0.45f;
    }

    const float preferX = gradient.gy / sum;
    const float preferY = gradient.gx / sum;
    const float tangent = (std::fabs(static_cast<float>(xx)) * preferX + std::fabs(static_cast<float>(yy)) * preferY) / len;
    const float normal = (std::fabs(static_cast<float>(xx)) * preferY + std::fabs(static_cast<float>(yy)) * preferX) / len;
    return clamp01(0.08f + 0.92f * tangent * (1.0f - 0.68f * normal));
}

inline float tangentKernelWeight(int offset, int radius)
{
    const int distance = std::abs(offset);
    if (distance == 0) {
        return 1.0f;
    }
    if (distance == 1) {
        return radius >= 2 ? 0.72f : 0.55f;
    }
    if (distance == 2) {
        return 0.36f;
    }
    return 0.16f;
}

inline float normalKernelWeight(int offset)
{
    const int distance = std::abs(offset);
    if (distance == 0) {
        return 0.22f;
    }
    if (distance == 1) {
        return 1.0f;
    }
    return 0.24f;
}

inline float orientedColorSampleWeight(int xx, int yy, int radius, const BoundaryGradient& gradient)
{
    const float horizontalConfidence = gradient.gy / (gradient.gx + gradient.gy + 1.0e-5f);
    const float verticalConfidence = 1.0f - horizontalConfidence;

    const float horizontalWeight = tangentKernelWeight(xx, radius) * normalKernelWeight(yy);
    const float verticalWeight = tangentKernelWeight(yy, radius) * normalKernelWeight(xx);
    return horizontalWeight * horizontalConfidence + verticalWeight * verticalConfidence;
}

inline bool canPullBoundaryColor(const Pixel& center, const Pixel& ref)
{
    if (isTransparentLike(ref) || isCompatibleBoundaryColor(center, ref)) {
        return false;
    }
    return true;
}

inline int countBoundaryFlowRun(const ImageView& image,
    int x,
    int y,
    int tx,
    int ty,
    int nx,
    int ny,
    const Pixel& center,
    const Pixel& ref,
    int maxLength)
{
    int length = 0;
    for (int step = 1; step <= maxLength; ++step) {
        const Pixel tangentCenter = image.get(x + tx * step, y + ty * step);
        const Pixel tangentRef = image.get(x + tx * step + nx, y + ty * step + ny);
        if (!isCompatibleBoundaryColor(center, tangentCenter)
            || isTransparentLike(tangentRef)
            || isCompatibleBoundaryColor(tangentCenter, tangentRef)) {
            break;
        }

        const float refDelta = colorBoundaryDistance(ref, tangentRef);
        const float centerDelta = colorBoundaryDistance(center, tangentCenter);
        if (refDelta > 0.32f || centerDelta > 0.14f) {
            break;
        }
        ++length;
    }
    return length;
}

inline float gradientBandWeight(int distance, int radius)
{
    const float remaining = static_cast<float>(radius - distance + 1);
    const float next = std::max(0.0f, remaining - 1.0f);
    const float denom = static_cast<float>(radius * radius);
    return denom > 0.0f ? (remaining * remaining - next * next) / denom : 0.0f;
}

inline float centeredBoundaryCoverage(int distance, int radius)
{
    if (radius <= 1) {
        return 0.50f;
    }
    const float t = clamp01(static_cast<float>(distance - 1) / static_cast<float>(radius - 1));
    const float falloff = 1.0f - t * t * (3.0f - 2.0f * t);
    return 0.50f * falloff;
}

inline int boundaryRunEndDistance(const ImageView& image,
    int x,
    int y,
    int tx,
    int ty,
    int nx,
    int ny,
    const Pixel& center,
    const Pixel& ref,
    int maxDistance)
{
    for (int distance = 1; distance <= maxDistance; ++distance) {
        const Pixel side = image.get(x + tx * distance, y + ty * distance);
        const Pixel across = image.get(x + tx * distance + nx, y + ty * distance + ny);
        if (isTransparentLike(side) || isTransparentLike(across)) {
            return distance;
        }
        if (!isCompatibleBoundaryColor(center, side)) {
            return distance;
        }
        if (!isCompatibleBoundaryColor(ref, across)) {
            return distance;
        }
        if (isCompatibleBoundaryColor(side, across)) {
            return distance;
        }
    }
    return maxDistance + 1;
}

inline bool isLikelyLineCore(const ImageView& image, int x, int y, const Pixel& center)
{
    if (luma(center) > 0.24f || saturation(center) > 0.18f) {
        return false;
    }

    int similar = 0;
    int different = 0;
    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            if (xx == 0 && yy == 0) {
                continue;
            }
            const Pixel p = image.get(x + xx, y + yy);
            if (isTransparentLike(p)) {
                continue;
            }
            if (isCompatibleBoundaryColor(center, p)) {
                ++similar;
            } else {
                ++different;
            }
        }
    }
    return similar >= 2 && different >= 1;
}

inline FlowSample findStraightBoundaryFlow(const ImageView& image, int x, int y, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    FlowSample best;
    if (isTransparentLike(center) || isBackgroundLike(center)) {
        return best;
    }
    const bool centerLineCore = isLikelyLineCore(image, x, y, center);

    constexpr std::array<std::array<int, 2>, 8> normals = {{
        {0, -1}, {0, 1}, {-1, 0}, {1, 0},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
    }};
    const int maxLength = settings.quality == Quality::Draft ? 5 : (settings.quality == Quality::High ? 10 : 6);
    const int gradientRadius = settings.quality == Quality::Draft ? 4 : (settings.quality == Quality::High ? 8 : 5);

    for (const auto& normal : normals) {
        const int nx = normal[0];
        const int ny = normal[1];
        const Pixel ref = image.get(x + nx, y + ny);
        if (!canPullBoundaryColor(center, ref)) {
            continue;
        }

        const bool diagonalNormal = nx != 0 && ny != 0;
        const int tx = diagonalNormal ? -ny : (ny != 0 ? 1 : 0);
        const int ty = diagonalNormal ? nx : (nx != 0 ? 1 : 0);
        const int forwardEnd = boundaryRunEndDistance(image, x, y, tx, ty, nx, ny, center, ref, gradientRadius);
        const int backwardEnd = boundaryRunEndDistance(image, x, y, -tx, -ty, nx, ny, center, ref, gradientRadius);
        const int nearestEnd = std::min(forwardEnd, backwardEnd);
        if (nearestEnd > gradientRadius) {
            continue;
        }

        const int visibleLength = std::min(maxLength, forwardEnd + backwardEnd - 1);
        if (visibleLength < (diagonalNormal ? 2 : 3)) {
            continue;
        }
        const bool refLineCore = isLikelyLineCore(image, x + nx, y + ny, ref);
        if (refLineCore && !centerLineCore) {
            continue;
        }

        const float contrast = colorBoundaryDistance(center, ref);
        const float flow = clamp01(static_cast<float>(visibleLength) / static_cast<float>(maxLength));
        const float centeredCoverage = centeredBoundaryCoverage(nearestEnd, gradientRadius);
        const float flowGate = diagonalNormal ? clamp01((flow - 0.15f) / 0.85f) : clamp01((flow - 0.25f) / 0.75f);
        const float curve = applyFalloffCurve(std::min(1.0f, contrast * 2.45f + flow * 0.20f), settings.colorFalloff);
        float strength = centeredCoverage * (0.72f + 0.28f * settings.blendStrength);
        strength *= 0.55f + 0.45f * curve;
        strength *= 0.42f + 0.58f * flowGate * flowGate;
        if (diagonalNormal) {
            strength *= 1.15f;
        }
        const float maxStrength = centerLineCore ? 0.46f : 0.52f;
        strength = std::min(maxStrength, strength);
        if (strength > best.strength) {
            best.color = ref;
            best.strength = strength;
            best.length = visibleLength;
        }
    }

    return best;
}

inline Pixel dominantCompatibleLineColor(const ImageView& image, int x, int y, float sideWeight)
{
    const std::array<float, 3> k = {sideWeight, 1.0f - 2.0f * sideWeight, sideWeight};
    Pixel best {};
    float bestWeight = -1.0f;

    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            const Pixel p = image.get(x + xx, y + yy);
            if (isBackgroundLike(p)) {
                continue;
            }
            const float w = k[static_cast<std::size_t>(xx + 1)] * k[static_cast<std::size_t>(yy + 1)];
            if (w > bestWeight) {
                best = p;
                bestWeight = w;
            }
        }
    }
    return bestWeight >= 0.0f ? best : image.get(x, y);
}

inline Pixel coverageSmooth3x3(const ImageView& image, int x, int y, float sideWeight)
{
    sideWeight = std::max(0.0f, std::min(0.24f, sideWeight));
    const std::array<float, 3> k = {sideWeight, 1.0f - 2.0f * sideWeight, sideWeight};
    const Pixel center = image.get(x, y);
    const Pixel reference = isBackgroundLike(center) ? dominantCompatibleLineColor(image, x, y, sideWeight) : center;
    Pixel out {};
    float totalWeight = 0.0f;

    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            const float w = k[static_cast<std::size_t>(xx + 1)] * k[static_cast<std::size_t>(yy + 1)];
            const Pixel p = image.get(x + xx, y + yy);
            if (!isCompatibleLineColor(reference, p)) {
                continue;
            }
            out.r += p.r * w;
            out.g += p.g * w;
            out.b += p.b * w;
            out.a += p.a * w;
            totalWeight += w;
        }
    }

    if (totalWeight <= 0.0f) {
        return center;
    }
    const float invWeight = 1.0f / totalWeight;
    out.r *= invWeight;
    out.g *= invWeight;
    out.b *= invWeight;
    out.a *= invWeight;
    return out;
}

inline Pixel findNearestLinePixel(const ImageView& image, int x, int y, int radius)
{
    Pixel best = image.get(x, y);
    int bestDist = radius * radius + 1;

    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            const Pixel p = image.get(x + xx, y + yy);
            if (isBackgroundLike(p)) {
                continue;
            }
            const int dist = xx * xx + yy * yy;
            if (dist < bestDist) {
                best = p;
                bestDist = dist;
            }
        }
    }
    return best;
}

inline Pixel findNearestOpaquePixel(const ImageView& image, int x, int y, int radius)
{
    Pixel best = image.get(x, y);
    int bestDist = radius * radius + 1;

    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            const Pixel p = image.get(x + xx, y + yy);
            if (isTransparentLike(p)) {
                continue;
            }
            const int dist = xx * xx + yy * yy;
            if (dist < bestDist) {
                best = p;
                bestDist = dist;
            }
        }
    }
    return best;
}

inline Pixel smoothAlphaCoverage(const ImageView& image, int x, int y, float sideWeight, const Settings& settings)
{
    const Pixel center = image.get(x, y);

    std::array<float, 7> kernel {};
    int radius = 2;
    if (settings.quality == Quality::Draft) {
        radius = 1;
        kernel = {0.0f, 0.0f, 0.20f, 0.60f, 0.20f, 0.0f, 0.0f};
    } else if (settings.quality == Quality::High) {
        radius = 3;
        kernel = {0.035f, 0.105f, 0.225f, 0.27f, 0.225f, 0.105f, 0.035f};
    } else {
        radius = 2;
        kernel = {0.0f, 0.065f, 0.24f, 0.39f, 0.24f, 0.065f, 0.0f};
    }

    float alpha = 0.0f;
    float nearestOpaqueDistance = std::numeric_limits<float>::max();
    for (int yy = -radius; yy <= radius; ++yy) {
        for (int xx = -radius; xx <= radius; ++xx) {
            const Pixel p = image.get(x + xx, y + yy);
            alpha += p.a
                * kernel[static_cast<std::size_t>(xx + 3)]
                * kernel[static_cast<std::size_t>(yy + 3)];
            if (p.a > 0.5f) {
                nearestOpaqueDistance = std::min(nearestOpaqueDistance, std::sqrt(static_cast<float>(xx * xx + yy * yy)));
            }
        }
    }

    const float alphaBoost = 1.05f + 0.85f * settings.blendStrength;
    alpha = clamp01(alpha * alphaBoost);
    if (center.a < 0.5f && nearestOpaqueDistance <= 1.5f && alpha > 0.0f) {
        const float nearBoost = settings.quality == Quality::High ? 0.30f : 0.24f;
        alpha = std::max(alpha, std::min(0.72f, alpha + nearBoost * settings.blendStrength));
    }
    if (center.a > 0.5f) {
        alpha = std::max(center.a, alpha);
    }

    Pixel out = center;
    out.a = alpha;

    if (center.a < 1.0e-4f && alpha > 1.0e-4f) {
        const Pixel nearest = findNearestOpaquePixel(image, x, y, radius + 1);
        out.r = nearest.r;
        out.g = nearest.g;
        out.b = nearest.b;
    } else if (alpha <= 1.0e-4f) {
        out.r = center.r;
        out.g = center.g;
        out.b = center.b;
    }
    return out;
}

inline bool hasAlphaBoundary(const ImageView& image, int x, int y)
{
    const float centerAlpha = image.get(x, y).a;
    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            if (xx == 0 && yy == 0) {
                continue;
            }
            if (std::fabs(centerAlpha - image.get(x + xx, y + yy).a) > 0.01f) {
                return true;
            }
        }
    }
    return false;
}

inline bool hasColorBoundary(const ImageView& image, int x, int y)
{
    const Pixel center = image.get(x, y);
    if (isTransparentLike(center)) {
        return false;
    }

    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            if (xx == 0 && yy == 0) {
                continue;
            }
            const Pixel p = image.get(x + xx, y + yy);
            if (isTransparentLike(p)) {
                continue;
            }
            if (!isCompatibleBoundaryColor(center, p)) {
                return true;
            }
        }
    }
    return false;
}

inline bool hasNearbyColorBoundary(const ImageView& image, int x, int y, int radius)
{
    const Pixel center = image.get(x, y);
    if (isTransparentLike(center)) {
        return false;
    }

    constexpr std::array<std::array<int, 2>, 8> directions = {{
        {0, -1}, {0, 1}, {-1, 0}, {1, 0},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
    }};

    for (const auto& direction : directions) {
        for (int distance = 1; distance <= radius; ++distance) {
            const Pixel p = image.get(x + direction[0] * distance, y + direction[1] * distance);
            if (isTransparentLike(p)) {
                break;
            }
            if (!isCompatibleBoundaryColor(center, p)) {
                return true;
            }
        }
    }
    return false;
}

inline Pixel smoothWhiteBackgroundCoverage(const ImageView& image, int x, int y, float sideWeight, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    if (!isBackgroundLike(center)) {
        return center;
    }

    const Pixel lineColor = findNearestLinePixel(image, x, y, 2);
    if (isBackgroundLike(lineColor)) {
        return center;
    }

    sideWeight = std::max(0.0f, std::min(0.24f, sideWeight));
    const std::array<float, 3> k = {sideWeight, 1.0f - 2.0f * sideWeight, sideWeight};
    float coverage = 0.0f;

    for (int yy = -1; yy <= 1; ++yy) {
        for (int xx = -1; xx <= 1; ++xx) {
            const Pixel p = image.get(x + xx, y + yy);
            if (isBackgroundLike(p)) {
                continue;
            }
            if (!isCompatibleLineColor(lineColor, p)) {
                continue;
            }
            coverage += k[static_cast<std::size_t>(xx + 1)] * k[static_cast<std::size_t>(yy + 1)];
        }
    }

    const float densityBoost = 1.0f + 1.35f * settings.blendStrength;
    coverage = std::min(0.55f, coverage * densityBoost);
    if (coverage <= 0.0f) {
        return center;
    }

    Pixel out;
    out.r = center.r * (1.0f - coverage) + lineColor.r * coverage;
    out.g = center.g * (1.0f - coverage) + lineColor.g * coverage;
    out.b = center.b * (1.0f - coverage) + lineColor.b * coverage;
    out.a = center.a;
    return out;
}

inline Pixel smoothColorBoundaryCoverage(const ImageView& image, int x, int y, float sideWeight, const Settings& settings)
{
    const Pixel center = image.get(x, y);
    if (isBackgroundLike(center)) {
        return smoothWhiteBackgroundCoverage(image, x, y, sideWeight, settings);
    }

    const FlowSample straightFlow = findStraightBoundaryFlow(image, x, y, settings);
    if (straightFlow.strength > 0.0f) {
        return lerp(center, straightFlow.color, straightFlow.strength);
    }
    return center;
}

inline float smoothSideWeight(const Settings& settings)
{
    float base = 0.095f;
    if (settings.quality == Quality::Draft) {
        base = 0.06f;
    } else if (settings.quality == Quality::High) {
        base = 0.13f;
    }
    return base * settings.blendStrength;
}

inline void process(const Pixel* src, Pixel* dst, int width, int height, Settings settings)
{
    if (!settings.enabled || settings.blendStrength <= 0.0f || width <= 0 || height <= 0) {
        std::copy(src, src + static_cast<std::size_t>(width) * height, dst);
        return;
    }

    settings.edgeThreshold = clamp01(settings.edgeThreshold);
    settings.blendStrength = clamp01(settings.blendStrength);
    settings.cornerProtection = clamp01(settings.cornerProtection);

    std::vector<Pixel> working(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < working.size(); ++i) {
        working[i] = settings.processPremultiplied ? unpremultiply(src[i]) : src[i];
    }

    const EdgeMaps maps = buildEdgeMaps(working.data(), width, height, settings);
    ImageView image(working.data(), width, height);
    const int maxLength = effectiveSearchLength(settings);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * width + x;
            const bool hEdge = maps.horizontal[i] != 0;
            const bool vEdge = maps.vertical[i] != 0;

            const bool diagonal = settings.diagonalSupport
                && edgeDistance(image.get(x - 1, y - 1), image.get(x + 1, y + 1), settings) > settings.edgeThreshold
                && edgeDistance(image.get(x + 1, y - 1), image.get(x - 1, y + 1), settings) <= settings.edgeThreshold;

            if (settings.debugView == DebugView::EdgeMask) {
                dst[i] = debugPixel((hEdge || vEdge || diagonal) ? 1.0f : 0.0f);
                continue;
            }

            int hLength = 0;
            int vLength = 0;
            if (hEdge) {
                hLength = 1
                    + countRun(maps.horizontal, width, height, x, y, -1, 0, maxLength)
                    + countRun(maps.horizontal, width, height, x, y, 1, 0, maxLength);
            }
            if (vEdge) {
                vLength = 1
                    + countRun(maps.vertical, width, height, x, y, 0, -1, maxLength)
                    + countRun(maps.vertical, width, height, x, y, 0, 1, maxLength);
            }

            const bool corner = hEdge && vEdge;
            float weightH = hEdge ? std::min(hLength / 4.0f, 1.0f) : 0.0f;
            float weightV = vEdge ? std::min(vLength / 4.0f, 1.0f) : 0.0f;
            const float cornerFactor = corner ? (1.0f - settings.cornerProtection) : 1.0f;
            float weight = settings.blendStrength * std::max(weightH, weightV) * cornerFactor;

            if (diagonal) {
                weight = std::max(weight, settings.blendStrength * 0.35f);
            }

            if (settings.debugView == DebugView::BlendWeight) {
                dst[i] = debugPixel(weight);
                continue;
            }
            if (settings.debugView == DebugView::PatternClass) {
                dst[i] = patternPixel(hEdge, vEdge, diagonal);
                continue;
            }

            Pixel out = image.get(x, y);
            const bool adjacentEdge = hEdge || vEdge || diagonal || hasLocalContrast(image, x, y, settings);
            const bool alphaBoundary = hasAlphaBoundary(image, x, y);
            const bool colorBoundary = hasColorBoundary(image, x, y);
            const int colorGradientRadius = settings.quality == Quality::Draft ? 4 : (settings.quality == Quality::High ? 8 : 5);
            const bool nearbyColorBoundary = colorBoundary || hasNearbyColorBoundary(image, x, y, colorGradientRadius);
            if (adjacentEdge || alphaBoundary || nearbyColorBoundary) {
                const float sideWeight = smoothSideWeight(settings) * (corner ? (1.0f - 0.35f * settings.cornerProtection) : 1.0f);
                if (alphaBoundary) {
                    out = smoothAlphaCoverage(image, x, y, sideWeight, settings);
                }
                if (nearbyColorBoundary) {
                    const float keepAlpha = out.a;
                    const Pixel colorOut = smoothColorBoundaryCoverage(image, x, y, sideWeight, settings);
                    out.r = colorOut.r;
                    out.g = colorOut.g;
                    out.b = colorOut.b;
                    out.a = keepAlpha;
                }
            }
            dst[i] = settings.processPremultiplied ? premultiply(out) : out;
        }
    }
}

} // namespace mlaa
