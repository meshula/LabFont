

#ifdef LABFONT_HAVE_SOKOL
#include "../LabSokol.h"
#endif

#include <cmath>
#include <cstdint>

// retrieved and adapted to sokol_gl from https://raw.githubusercontent.com/floooh/v6502r/master/src/ui.cc
// Atari 8-bitter font extracted from ROM listing, starting at 0x20
static const uint8_t atari_font[96][8] = {
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },    // - space
    { 0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x00 },    // - !
    { 0x00,0x66,0x66,0x66,0x00,0x00,0x00,0x00 },    // - "
    { 0x00,0x66,0xFF,0x66,0x66,0xFF,0x66,0x00 },    // - #
    { 0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00 },    // - $
    { 0x00,0x66,0x6C,0x18,0x30,0x66,0x46,0x00 },    // - %
    { 0x1C,0x36,0x1C,0x38,0x6F,0x66,0x3B,0x00 },    // - &
    { 0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00 },    // - '
    { 0x00,0x0E,0x1C,0x18,0x18,0x1C,0x0E,0x00 },    // - (
    { 0x00,0x70,0x38,0x18,0x18,0x38,0x70,0x00 },    // - )
    { 0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00 },    // - asterisk
    { 0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00 },    // - plus
    { 0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30 },    // - comma
    { 0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00 },    // - minus
    { 0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00 },    // - period
    { 0x00,0x06,0x0C,0x18,0x30,0x60,0x40,0x00 },    // - /
    { 0x00,0x3C,0x66,0x6E,0x76,0x66,0x3C,0x00 },    // - 0
    { 0x00,0x18,0x38,0x18,0x18,0x18,0x7E,0x00 },    // - 1
    { 0x00,0x3C,0x66,0x0C,0x18,0x30,0x7E,0x00 },    // - 2
    { 0x00,0x7E,0x0C,0x18,0x0C,0x66,0x3C,0x00 },    // - 3
    { 0x00,0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x00 },    // - 4
    { 0x00,0x7E,0x60,0x7C,0x06,0x66,0x3C,0x00 },    // - 5
    { 0x00,0x3C,0x60,0x7C,0x66,0x66,0x3C,0x00 },    // - 6
    { 0x00,0x7E,0x06,0x0C,0x18,0x30,0x30,0x00 },    // - 7
    { 0x00,0x3C,0x66,0x3C,0x66,0x66,0x3C,0x00 },    // - 8
    { 0x00,0x3C,0x66,0x3E,0x06,0x0C,0x38,0x00 },    // - 9
    { 0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x00 },    // - colon
    { 0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x30 },    // - semicolon
    { 0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00 },    // - <
    { 0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00 },    // - =
    { 0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00 },    // - >
    { 0x00,0x3C,0x66,0x0C,0x18,0x00,0x18,0x00 },    // - ?
    { 0x00,0x3C,0x66,0x6E,0x6E,0x60,0x3E,0x00 },    // - @
    { 0x00,0x18,0x3C,0x66,0x66,0x7E,0x66,0x00 },    // - A
    { 0x00,0x7C,0x66,0x7C,0x66,0x66,0x7C,0x00 },    // - B
    { 0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00 },    // - C
    { 0x00,0x78,0x6C,0x66,0x66,0x6C,0x78,0x00 },    // - D
    { 0x00,0x7E,0x60,0x7C,0x60,0x60,0x7E,0x00 },    // - E
    { 0x00,0x7E,0x60,0x7C,0x60,0x60,0x60,0x00 },    // - F
    { 0x00,0x3E,0x60,0x60,0x6E,0x66,0x3E,0x00 },    // - G
    { 0x00,0x66,0x66,0x7E,0x66,0x66,0x66,0x00 },    // - H
    { 0x00,0x7E,0x18,0x18,0x18,0x18,0x7E,0x00 },    // - I
    { 0x00,0x06,0x06,0x06,0x06,0x66,0x3C,0x00 },    // - J
    { 0x00,0x66,0x6C,0x78,0x78,0x6C,0x66,0x00 },    // - K
    { 0x00,0x60,0x60,0x60,0x60,0x60,0x7E,0x00 },    // - L
    { 0x00,0x63,0x77,0x7F,0x6B,0x63,0x63,0x00 },    // - M
    { 0x00,0x66,0x76,0x7E,0x7E,0x6E,0x66,0x00 },    // - N
    { 0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00 },    // - O
    { 0x00,0x7C,0x66,0x66,0x7C,0x60,0x60,0x00 },    // - P
    { 0x00,0x3C,0x66,0x66,0x66,0x6C,0x36,0x00 },    // - Q
    { 0x00,0x7C,0x66,0x66,0x7C,0x6C,0x66,0x00 },    // - R
    { 0x00,0x3C,0x60,0x3C,0x06,0x06,0x3C,0x00 },    // - S
    { 0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x00 },    // - T
    { 0x00,0x66,0x66,0x66,0x66,0x66,0x7E,0x00 },    // - U
    { 0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00 },    // - V
    { 0x00,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00 },    // - W
    { 0x00,0x66,0x66,0x3C,0x3C,0x66,0x66,0x00 },    // - X
    { 0x00,0x66,0x66,0x3C,0x18,0x18,0x18,0x00 },    // - Y
    { 0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00 },    // - Z
    { 0x00,0x1E,0x18,0x18,0x18,0x18,0x1E,0x00 },    // - [
    { 0x00,0x40,0x60,0x30,0x18,0x0C,0x06,0x00 },    // - backslash
    { 0x00,0x78,0x18,0x18,0x18,0x18,0x78,0x00 },    // - ]
    { 0x00,0x08,0x1C,0x36,0x63,0x00,0x00,0x00 },    // - ^
    { 0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00 },    // - underline
    { 0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x00 },    // - Spanish ! (should be `)
    { 0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00 },    // - a
    { 0x00,0x60,0x60,0x7C,0x66,0x66,0x7C,0x00 },    // - b
    { 0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00 },    // - c
    { 0x00,0x06,0x06,0x3E,0x66,0x66,0x3E,0x00 },    // - d
    { 0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00 },    // - e
    { 0x00,0x0E,0x18,0x3E,0x18,0x18,0x18,0x00 },    // - f
    { 0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C },    // - g
    { 0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x00 },    // - h
    { 0x00,0x18,0x00,0x38,0x18,0x18,0x3C,0x00 },    // - i
    { 0x00,0x06,0x00,0x06,0x06,0x06,0x06,0x3C },    // - j
    { 0x00,0x60,0x60,0x6C,0x78,0x6C,0x66,0x00 },    // - k
    { 0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00 },    // - l
    { 0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00 },    // - m
    { 0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00 },    // - n
    { 0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00 },    // - o
    { 0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60 },    // - p
    { 0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06 },    // - q
    { 0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00 },    // - r
    { 0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00 },    // - s
    { 0x00,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00 },    // - t
    { 0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00 },    // - u
    { 0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00 },    // - v
    { 0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00 },    // - w
    { 0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00 },    // - x
    { 0x00,0x00,0x66,0x66,0x66,0x3E,0x0C,0x78 },    // - y
    { 0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00 },    // - z
    { 0x66,0x66,0x18,0x3C,0x66,0x7E,0x66,0x00 },    // - umlaut A
    { 0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18 },    // - |
    { 0x00,0x7E,0x78,0x7C,0x6E,0x66,0x06,0x00 },    // - display clear
    { 0x08,0x18,0x38,0x78,0x38,0x18,0x08,0x00 },    // - display backspace
    { 0x10,0x18,0x1C,0x1E,0x1C,0x18,0x10,0x00 },    // - display tab
};

static const uint32_t pal_0[8] = {
    0x000000FF,     // red
    0x000000FF,     // red
    0x000088FF,     // orange
    0x000088FF,     // orange
    0x0000FFFF,     // yellow
    0x0000FFFF,     // yellow
    0x0000FF00,     // green
    0x0000FF00,     // green
};

static const uint32_t pal_1[8] = {
    0x0000FF00,     // green
    0x0000FF00,     // green
    0x00FFFF00,     // teal
    0x00FFFF00,     // teal
    0x00FF8888,     // blue
    0x00FF8888,     // blue
    0x00FF88FF,     // violet
    0x00FF88FF,     // violet
};

#ifdef LABFONT_HAVE_SOKOL

extern "C"
void draw_wobble_string(const char* str, 
    float posx, float posy, 
    float pixel_radius, float pixel_stride, const uint32_t* palette, float time, 
    float warp0, float warp1, float warp2, float alpha) 
{
    sgl_begin_quads();
    uint32_t a = ((uint32_t)(255.0f * alpha)) << 24;
    uint8_t chr;
    int chr_index = 0;
    while ((chr = (uint8_t)*str++) != 0) {
        uint8_t font_index = chr - 0x20;
        if (font_index >= 96) {
            continue;
        }
        float py = posy;
        for (int y = 0; y < 8; y++, py += pixel_stride) {
            float px = posx + chr_index * 8 * pixel_stride + sinf((time + (py * warp0)) * warp1) * warp2;
            uint8_t pixels = atari_font[font_index][y];
            uint32_t col = palette[y] | a;
            for (int x = 7; x >= 0; x--, px += pixel_stride) {
                if (pixels & (1 << x)) {
                    float x0 = px - pixel_radius;
                    float x1 = px + pixel_radius;
                    float y0 = py - pixel_radius;
                    float y1 = py + pixel_radius;
                    sgl_v2f_c1i(x0, y0, col);
                    sgl_v2f_c1i(x0, y1, col);
                    sgl_v2f_c1i(x1, y1, col);
                    sgl_v2f_c1i(x1, y0, col);
                }
            }
        }
        chr_index++;
    }
    sgl_end();
}

// color: r, g<<8, b<<16, a<<24
extern "C"
void draw_atari_string(const char* str,
    float posx, float posy,
    float pixel_radius, float pixel_stride, uint32_t color)
{
    sgl_begin_quads();
    uint8_t chr;
    int chr_index = 0;
    while ((chr = (uint8_t)*str++) != 0) {
        uint8_t font_index = chr - 0x20;
        if (font_index >= 96) {
            continue;
        }
        float py = posy;
        for (int y = 0; y < 8; y++, py += pixel_stride) {
            float px = posx + chr_index * 8 * pixel_stride;
            uint8_t pixels = atari_font[font_index][y];
            for (int x = 7; x >= 0; x--, px += pixel_stride) {
                if (pixels & (1 << x)) {
                    float x0 = px - pixel_radius;
                    float x1 = px + pixel_radius;
                    float y0 = py - pixel_radius;
                    float y1 = py + pixel_radius;
                    sgl_v2f_c1i(x0, y0, color);
                    sgl_v2f_c1i(x0, y1, color);
                    sgl_v2f_c1i(x1, y1, color);
                    sgl_v2f_c1i(x1, y0, color);
                }
            }
        }
        chr_index++;
    }
    sgl_end();
}

#endif

