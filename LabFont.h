#ifndef LABFONT_H
#define LABFONT_H

#include <stdint.h>

const int LabFontTypeTTF = 0;
const int LabFontTypeROMFont = 1;
const int LabFontTypeQuadplay = 2;
struct LabFontType { int t; };

struct LabFont;

LabFont* LabFontLoad(const char* name, const char* path, LabFontType type);

const int LabFontAlignTop = 1;
const int LabFontAlignMiddle = 2;
const int LabFontAlignBaseline = 4;
const int LabFontAlignBottom = 8;
const int LabFontAlignLeft = 16;
const int LabFontAlignCenter = 32;
const int LabFontAlignRight = 64;
struct LabFontAlign { int a; };

struct LabFontState;

struct LabFontColor {
    uint8_t rgba[4];
};

struct LabFontSize {
    float ascender, descender, w, h;
};

LabFontState* LabFontStateBake(LabFont* font,
    float size,
    LabFontColor,
    LabFontAlign alignment,
    float spacing,
    float blur);

// returns first pixel following the drawn text
float LabFontDraw(const char* str, float x, float y, LabFontState* fs);

// returns first pixel following the drawn text, overrides color in font state
float LabFontDrawColor(const char* str, LabFontColor* c, float x, float y, LabFontState* fs);
float LabFontDrawSubstringColor(const char* str, const char* end, LabFontColor* c, float x, float y, LabFontState* fs);


LabFontSize LabFontMeasure(const char* str, LabFontState* fs);
LabFontSize LabFontMeasureSubstring(const char* str, const char* end, LabFontState* fs);

void LabFontCommitTexture();

#endif

#ifdef LABFONT_IMPL

#include "fontstash.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>

struct LabFont
{
    int id;         // non-zero for a TTF
    sg_image img;   // non-zero for a QuadPlay texture
};

struct LabFontState
{
    LabFont* font;
    float size;
    LabFontColor color;
    LabFontAlign alignment;
    float spacing;
    float blur;
};

namespace LabFontInternal {

    struct LoadResult { uint8_t* buff; size_t len; };
    LoadResult load(const char* path)
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

        fseek(f, 0, SEEK_SET);
        size_t len = fread(buff, 1, sz, f);

        fclose(f);
        return { buff, len };
    }

    FONScontext* fontStash() 
    {
        const int atlas_dim = 1024;
        static FONScontext* fons_context = sfons_create(atlas_dim, atlas_dim, FONS_ZERO_TOPLEFT);
        return fons_context;
    }

    constexpr uint32_t fons_rgba(const LabFontColor& c) 
    {
        return ((uint32_t)c.rgba[0]) | 
               ((uint32_t)c.rgba[1] << 8) |
               ((uint32_t)c.rgba[2] << 16) | 
               ((uint32_t)c.rgba[3] << 24);
    }

    constexpr int fons_align(const LabFontAlign& a)
    {
        int r = 0;
        if (a.a & LabFontAlignTop)
            r |= FONS_ALIGN_TOP;
        else if (a.a & LabFontAlignMiddle)
            r |= FONS_ALIGN_MIDDLE;
        else if (a.a & LabFontAlignBaseline)
            r |= FONS_ALIGN_BASELINE;
        else if (a.a & LabFontAlignBottom)
            r |= FONS_ALIGN_BOTTOM;
        if (a.a & LabFontAlignLeft)
            r |= FONS_ALIGN_LEFT;
        else if (a.a & LabFontAlignCenter)
            r |= FONS_ALIGN_CENTER;
        else if (a.a & LabFontAlignRight)
            r |= FONS_ALIGN_RIGHT;
        return r;
    }

    void fontstash_bind(LabFontState* fs)
    {
        FONScontext* fc = LabFontInternal::fontStash();
        fonsSetFont(fc, fs->font->id);
        fonsSetSize(fc, fs->size);
        fonsSetColor(fc, LabFontInternal::fons_rgba(fs->color));
        fonsSetAlign(fc, LabFontInternal::fons_align(fs->alignment));
        fonsSetSpacing(fc, fs->spacing);
        fonsSetBlur(fc, fs->blur);
    }

}


LabFontState* LabFontStateBake(LabFont* font,
    float size,
    LabFontColor color,
    LabFontAlign alignment,
    float spacing,
    float blur)
{
    if (font == nullptr)
        return nullptr;
    if (size < 1)
        return nullptr;

    LabFontState* st = (LabFontState*)calloc(1, sizeof(LabFontState));
    if (st == nullptr)
        return nullptr;

    st->font = font;
    st->size = size;
    st->color = color;
    st->alignment = alignment;
    st->spacing = spacing;
    st->blur = blur;
    return st;
}

LabFont* LabFontLoad(const char* name, const char* path, LabFontType type)
{
    if (type.t == LabFontTypeTTF)
    {
        LabFontInternal::LoadResult res = LabFontInternal::load(path);
        if (!res.len)
            return nullptr;

        LabFont* r = (LabFont*)calloc(1, sizeof(LabFont));
        if (!r)
            return nullptr;

        // pass ownership of res.buff to fons
        r->id = fonsAddFontMem(LabFontInternal::fontStash(), name,
            (unsigned char*)res.buff, (int)res.len, false);

        return r;
    }
    else if (type.t == LabFontTypeQuadplay)
    {
        // force the image to load as R8
        int x, y, n;
        uint8_t* data = stbi_load(path, &x, &y, &n, 1);
        if (data != nullptr && x > 0 && y > 0 && n > 0)
        {
            LabFont* r = (LabFont*)calloc(1, sizeof(LabFont));
            if (!r)
            {
                stbi_image_free(data);
                return nullptr;
            }

            sg_image_desc img_desc;
            memset(&img_desc, 0, sizeof(img_desc));
            img_desc.width = x;
            img_desc.height = y;
            img_desc.min_filter = SG_FILTER_LINEAR;
            img_desc.mag_filter = SG_FILTER_LINEAR;
            img_desc.usage = SG_USAGE_DYNAMIC;
            img_desc.pixel_format = SG_PIXELFORMAT_R8;
            r->img = sg_make_image(&img_desc);

            if (r->img.id == 0)
            {
                stbi_image_free(data);
                return nullptr;
            }

            sg_image_data new_image_data;
            memset(&new_image_data, 0, sizeof(new_image_data));
            new_image_data.subimage[0][0].ptr = data;
            new_image_data.subimage[0][0].size = (size_t)(x * y);
            sg_update_image(r->img, &new_image_data);
            stbi_image_free(data);
            return r;
        }
    }

    return nullptr;
}




float LabFontDraw(const char* str, float x, float y, LabFontState* fs)
{
    if (!fs || !str)
        return x;

    if (fs->font->id > 0) {
        LabFontInternal::fontstash_bind(fs);
        FONScontext* fc = LabFontInternal::fontStash();
        return fonsDrawText(fc, x, y, str, NULL);
    }
    else if (fs->font->img.id > 0) {
        FONScontext* fc = LabFontInternal::fontStash();
        sgl_enable_texture();
        sgl_texture(fs->font->img);
        sgl_push_pipeline();
        sgl_load_pipeline(fs->pip);
        sgl_begin_triangles();
        for (int i = 0; i < nverts; i++) {
            sgl_v2f_t2f_c1i(verts[2 * i + 0], verts[2 * i + 1], tcoords[2 * i + 0], tcoords[2 * i + 1], colors[i]);
        }
        sgl_end();
        sgl_pop_pipeline();
        sgl_disable_texture();

    }
    return x;
}

float LabFontDrawColor(const char* str, LabFontColor* c, float x, float y, LabFontState* fs)
{
    return LabFontDrawSubstringColor(str, NULL, c, x, y, fs);
}

float LabFontDrawSubstringColor(const char* str, const char* end, LabFontColor* c, float x, float y, LabFontState* fs)
{
    if (!fs || !str)
        return x;

    LabFontInternal::fontstash_bind(fs);
    FONScontext* fc = LabFontInternal::fontStash();
    fonsSetColor(fc, LabFontInternal::fons_rgba(*c));
    return fonsDrawText(fc, x, y, str, end);
}

LabFontSize LabFontMeasure(const char* str, LabFontState* fs)
{
    return LabFontMeasureSubstring(str, NULL, fs);
}

LabFontSize LabFontMeasureSubstring(const char* str, const char* end, LabFontState* fs)
{
    if (!str || !fs || !fs->font)
        return { 0, 0 };

    LabFontInternal::fontstash_bind(fs);
    FONScontext* fc = LabFontInternal::fontStash();

    LabFontSize sz;
    float bounds[4];
    sz.w = fonsTextBounds(fc, 0, 0, str, end, bounds);
    fonsVertMetrics(fc, &sz.ascender, &sz.descender, &sz.h);
    return sz;
}


void LabFontCommitTexture()
{
    // flush fontstash's font atlas to sokol-gfx texture
    sfons_flush(LabFontInternal::fontStash());
}

#endif
