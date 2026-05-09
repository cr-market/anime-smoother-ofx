#include "ContourAACore.h"
#include "LoiloSmoothCore.h"
#include "MLAACore.h"
#include "PatternAACore.h"
#include "RegionAACore.h"
#include "SSAACore.h"

#include <cassert>
#include <iostream>
#include <vector>

int main()
{
    constexpr int width = 8;
    constexpr int height = 8;
    std::vector<mlaa::Pixel> src(width * height);
    std::vector<mlaa::Pixel> dst(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool rightSide = x >= 4;
            src[static_cast<std::size_t>(y) * width + x] = rightSide
                ? mlaa::Pixel{0.0f, 0.0f, 0.0f, 0.0f}
                : mlaa::Pixel{0.0f, 0.0f, 0.0f, 1.0f};
        }
    }

    mlaa::Settings settings;
    settings.edgeMode = mlaa::EdgeMode::Luma;
    settings.edgeThreshold = 0.1f;
    settings.blendStrength = 0.75f;
    mlaa::process(src.data(), dst.data(), width, height, settings);

    const mlaa::Pixel linePixel = dst[3];
    const mlaa::Pixel backgroundEdgePixel = dst[4];
    assert(linePixel.a > 0.0f);
    assert(backgroundEdgePixel.a > 0.0f);
    assert(backgroundEdgePixel.a < 1.0f);

    settings.debugView = mlaa::DebugView::EdgeMask;
    mlaa::process(src.data(), dst.data(), width, height, settings);
    assert(dst[3].r == 1.0f);

    settings.debugView = mlaa::DebugView::Final;
    regionaa::process(src.data(), dst.data(), width, height, settings);
    assert(dst[3].a > 0.0f);
    assert(dst[4].a > 0.0f);
    assert(dst[4].a < 1.0f);

    for (const mlaa::ScaleAlgorithm algorithm : {
             mlaa::ScaleAlgorithm::Nearest,
             mlaa::ScaleAlgorithm::Bilinear,
             mlaa::ScaleAlgorithm::Bicubic,
             mlaa::ScaleAlgorithm::Lanczos,
             mlaa::ScaleAlgorithm::EdgeAware,
         }) {
        settings.scaleAlgorithm = algorithm;
        ssaa::process(src.data(), dst.data(), width, height, settings);
        for (const mlaa::Pixel& pixel : dst) {
            assert(pixel.r >= 0.0f && pixel.r <= 1.0f);
            assert(pixel.g >= 0.0f && pixel.g <= 1.0f);
            assert(pixel.b >= 0.0f && pixel.b <= 1.0f);
            assert(pixel.a >= 0.0f && pixel.a <= 1.0f);
        }
    }

    settings.quality = mlaa::Quality::High;
    settings.alphaAware = true;
    contouraa::process(src.data(), dst.data(), width, height, settings);
    for (int y = 0; y < height; ++y) {
        assert(dst[static_cast<std::size_t>(y) * width + 4].a == 0.0f);
        assert(dst[static_cast<std::size_t>(y) * width + 3].a <= src[static_cast<std::size_t>(y) * width + 3].a);
    }

    patternaa::process(src.data(), dst.data(), width, height, settings);
    for (int y = 0; y < height; ++y) {
        assert(dst[static_cast<std::size_t>(y) * width + 4].a == 0.0f);
        assert(dst[static_cast<std::size_t>(y) * width + 3].a <= src[static_cast<std::size_t>(y) * width + 3].a);
    }

    loilosmooth::process(src.data(), dst.data(), width, height, settings);
    for (int y = 0; y < height; ++y) {
        assert(dst[static_cast<std::size_t>(y) * width + 4].a == 0.0f);
    }

    std::cout << "MLAA core test passed\n";
    return 0;
}
