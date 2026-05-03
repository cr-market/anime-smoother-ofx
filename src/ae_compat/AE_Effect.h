#pragma once

#include <cstdint>

using PF_Err = int;
constexpr PF_Err PF_Err_NONE = 0;

struct PF_Pixel8 {
    uint8_t alpha = 0;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
};

struct PF_Pixel16 {
    uint16_t alpha = 0;
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;
};

struct PF_LayerDef {
    int width = 0;
    int height = 0;
    int rowbytes = 0;
    void* data = nullptr;
};

struct PF_Rect {
    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;
};
