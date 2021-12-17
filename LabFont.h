#ifndef LABFONT_H
#define LABFONT_H

#include <stdint.h>

/*
 This font rendering scheme is built over sokol's gl emulator.
 It also uses mononen's fontstash for TTF, Morgan Macguire's
 quadplay font system for bitmap fonts.

 LabFont creates it's own sokol pipeline objects, and manages
 its own textures. Drawing text pushes and pops the sokol
 pipeline, and will bind textures as necessary. The sgl state
 is returned to a default state, not a popped state, in other
 words, if texture was enabled, text was drawn, then texture
 is disabled after the draw.

 */

#ifdef __cplusplus
extern "C" {
#endif

const int LabFontTypeTTF = 0;
const int LabFontTypeQuadplay = 1;
struct LabFontType { int type; };

struct LabFont;
struct LabFontState;

struct LabFontColor {
    uint8_t rgba[4];
};

struct LabFontSize {
    float ascender, descender, width, height;
};

const int LabFontAlignTop = 1;
const int LabFontAlignMiddle = 2;
const int LabFontAlignBaseline = 4;
const int LabFontAlignBottom = 8;
const int LabFontAlignLeft = 16;
const int LabFontAlignCenter = 32;
const int LabFontAlignRight = 64;
struct LabFontAlign { int alignment; };

struct LabFont* LabFontLoad(const char* name, const char* path, struct LabFontType type);
struct LabFont* LabFontGet(const char* name);

// note that blur only works with LabFontTypeTTF
struct LabFontState* LabFontStateBake(
    struct LabFont* font,
    float size,
    struct LabFontColor,
    struct LabFontAlign alignment,
    float spacing,
    float blur);

// a version to work around languages that can't bind structs as values
struct LabFontState* LabFontStateBake_bind(
    struct LabFont* font,
    float size,
    struct LabFontColor*,
    struct LabFontAlign* alignment,
    float spacing,
    float blur);



// returns first pixel's x coordinate following the drawn text
float LabFontDraw(const char* str, float x, float y, struct LabFontState* fs);

// returns first pixel following the drawn text, overrides color in font state
float LabFontDrawColor(const char* str, struct LabFontColor* c, float x, float y, struct LabFontState* fs);
float LabFontDrawSubstringColor(const char* str, const char* end, struct LabFontColor* c, float x, float y, struct LabFontState* fs);

// measure a string. Measuring an empty string will fill in font metrics to ascender, descender, and h.
struct LabFontSize LabFontMeasure(const char* str, struct LabFontState* fs);
struct LabFontSize LabFontMeasureSubstring(const char* str, const char* end, struct LabFontState* fs);

// this should be called once before rendering text, after all a frame's LabFontDraw calls have been made.
void LabFontCommitTexture();

#ifdef __cplusplus
}
#endif

#endif
