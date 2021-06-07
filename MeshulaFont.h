#ifndef LABFONT_H
#define LABFONT_H

#include <stdint.h>

const int LabFontTypeTTF = 0;
const int LabFontTypeROMFont = 1;
struct LabFontType { int t; };

struct LabFont;

LabFont* LabFontLoad(const char* name, const char* path, LabFontType type);

const int LabFontAlignTop = 0;
const int LabFontAlignMiddle = 1;
const int LabFontAlignBottom = 2;
const int LabFontAlignLeft = 0;
const int LabFontAlignCenter = 16;
const int LabFontAlignRight = 32;
struct LabFontAlign { int a; };

struct LabFontState;

struct LabFontColor {
    uint8_t rgba[4];
};


LabFontState LabFontStateBake(LabFont* font,
    float size,
    LabFontColor,
    LabFontAlign alignment,
    float spacing,
    float blur);

void LabFontDraw(const char* str, float x, float y, LabFontState* fs);

#endif

#define LABFONT_IMPL
#ifdef LABFONT_IMPL

#include <stdio.h>

namespace LabFontInternal {

    load(const char* path) -> struct { uint8_t* buff, size_t len }
    {
        FILE* f = fopen(path, "rb");
        if (!f)
            return { nullptr, 0 };

        fseek(f, 0, SEEK_END);
        size_t sz = ftell(f);

        if (sz == 0) {
            fclose(f);
            return { nullptr, 0 };
        }

        uint8_t* buff = (uint8_t*)malloc(sz);
        if (buff == nullptr) {
            fclose(f);
            return { nullptr, 0 };
        }

        fseek(f, 0, SEEK_CURR);
        size_t len = fread(buff, 1, sz, f);
        fclose(f);

        return { buff, len };
    }

}

#endif
