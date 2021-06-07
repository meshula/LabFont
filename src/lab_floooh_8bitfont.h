#ifndef LAB_FLOOOH_8BITFONT_H
#define LAB_FLOOOH_8BITFONT_H

// These routines leverage floooh's atari font wobble string render.

#include <stdint.h>

#ifdef __cpluslus
#define LF_EXTERNC extern "C"
#else
#define LF_EXTERNC
#endif

// color: r, g<<8, b<<16, a<<24
LF_EXTERNC
void draw_atari_string(const char* str,
    float posx, float posy,
    float pixel_radius, float pixel_stride, uint32_t color);

// palette is 8 uint32's, one per pixel row. r, g<<8, b<<16, a<<24
// alpha will be folded into the palette.
// warp controls the wobble.
// warp0 is phase shift, warp1 is frequency, warp2 is magnitude
LF_EXTERNC
void draw_wobble_string(const char* str,
    float posx, float posy,
    float pixel_radius, float pixel_stride, const uint32_t* palette, float time,
    float warp0, float warp1, float warp2,
    float alpha);

#undef LF_EXTERNC
#endif
