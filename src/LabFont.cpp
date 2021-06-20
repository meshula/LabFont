

#include "../LabFont.h"
#include "../LabSokol.h"
#include "fontstash.h"
#include "rapidjson/document.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <array>
#include <map>
#include <string>
#include <stdio.h>

struct LabFont
{
    int id;           // non-zero for a TTF

    sg_image img;     // non-zero for a QuadPlay texture
    int img_w, img_h; // non-zero for a QuadPlay texture
    int baseline;
    int charsz_x, charsz_y;
    int charspc_x, charspc_y;
    std::array<int8_t, 256> kern;
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
        if (a.alignment & LabFontAlignTop)
            r |= FONS_ALIGN_TOP;
        else if (a.alignment & LabFontAlignMiddle)
            r |= FONS_ALIGN_MIDDLE;
        else if (a.alignment & LabFontAlignBaseline)
            r |= FONS_ALIGN_BASELINE;
        else if (a.alignment & LabFontAlignBottom)
            r |= FONS_ALIGN_BOTTOM;
        if (a.alignment & LabFontAlignLeft)
            r |= FONS_ALIGN_LEFT;
        else if (a.alignment & LabFontAlignCenter)
            r |= FONS_ALIGN_CENTER;
        else if (a.alignment & LabFontAlignRight)
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

sg_image debug_texture;
static std::array<int, 256> qp_font_map;
std::array<int, 256> build_quadplay_font_map();

static std::map<std::string, std::unique_ptr<LabFont>> fonts;

LabFont* LabFontLoad(const char* name, const char* path, LabFontType type)
{
    std::string key(name);
    if (type.type == LabFontTypeTTF)
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

        fonts[key] = std::unique_ptr<LabFont>(r);
        return r;
    }
    else if (type.type == LabFontTypeQuadplay)
    {
        qp_font_map = build_quadplay_font_map();
        LabFont* r = (LabFont*)calloc(1, sizeof(LabFont));
        if (!r)
        {
            return nullptr;
        }

        bool mono_numeric = true;
        bool monospaced = false;
        bool word_space_override = false;
        int word_space = 0;
        std::string path_str = path;
        size_t lastindex = path_str.find_last_of(".");
        if (lastindex != std::string::npos) {
            std::string jpath = path_str.substr(0, lastindex) + ".font.json";
            LabFontInternal::LoadResult res = LabFontInternal::load(jpath.c_str());
            if (res.len) {
                rapidjson::Document json;
                json.Parse((const char*)res.buff, res.len);
                if (!json.HasParseError()) {
                    rapidjson::Value::MemberIterator baseline = json.FindMember("baseline");
                    if (baseline != json.MemberEnd() && baseline->value.IsNumber())
                        r->baseline = baseline->value.GetInt();
                    rapidjson::Value::MemberIterator char_size = json.FindMember("char_size");
                    if (char_size != json.MemberEnd() && char_size->value.IsObject()) {
                        auto obj = char_size->value.GetObject();
                        rapidjson::Value::MemberIterator x = obj.FindMember("x");
                        if (x != obj.MemberEnd() && x->value.IsNumber())
                            r->charsz_x = x->value.GetInt();
                        rapidjson::Value::MemberIterator y = obj.FindMember("y");
                        if (y != obj.MemberEnd() && y->value.IsNumber())
                            r->charsz_y = y->value.GetInt();
                    }
                    rapidjson::Value::MemberIterator letter_spacing = json.FindMember("letter_spacing");
                    if (letter_spacing != json.MemberEnd() && letter_spacing->value.IsObject()) {
                        auto obj = letter_spacing->value.GetObject();
                        rapidjson::Value::MemberIterator x = obj.FindMember("x");
                        if (x != obj.MemberEnd() && x->value.IsNumber())
                            r->charspc_x = x->value.GetInt();
                        rapidjson::Value::MemberIterator y = obj.FindMember("y");
                        if (y != obj.MemberEnd() && y->value.IsNumber())
                            r->charspc_y = y->value.GetInt();
                    }
                    rapidjson::Value::MemberIterator atlas = json.FindMember("atlas");
                    if (atlas != json.MemberEnd() && atlas->value.IsString()) {
                        std::string atlas_str = atlas->value.GetString();
                        if (atlas_str == "proportional")
                            mono_numeric = true;
                        else if (atlas_str == "monospaced")
                            monospaced = true;
                        else if (atlas_str == "proportional, mono-numeric")
                            mono_numeric = true;
                    }
                    rapidjson::Value::MemberIterator wordspace = json.FindMember("word_spacing");
                    if (wordspace != json.MemberEnd() && wordspace->value.IsNumber()) {
                        word_space_override = true;
                        word_space = wordspace->value.GetInt();
                    }
                }
                else {
                    auto err = json.GetParseError();
                }
                free(res.buff);
            }
        }

        // force the image to load as R8
        int x, y, n;
        uint8_t* data = stbi_load(path, &x, &y, &n, STBI_rgb_alpha);
        if (data != nullptr && x > 0 && y > 0 && n > 0)
        {
            // stbi fills in alpha with 255, so zero out alpha for empty pixels
            for (int i = 0; i < x * y; ++i) {
                int addr = i * 4;
                data[addr + 3] = data[addr] == 0 ? 0 : 255;
            }
            sg_image_desc img_desc;
            memset(&img_desc, 0, sizeof(img_desc));
            img_desc.width = x;
            img_desc.height = y;
            img_desc.min_filter = SG_FILTER_LINEAR;
            img_desc.mag_filter = SG_FILTER_LINEAR;
            img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
            img_desc.data.subimage[0][0].ptr = data;
            img_desc.data.subimage[0][0].size = (size_t)(x * 4 * y);
            r->img = sg_make_image(&img_desc);

            debug_texture = r->img;

            if (r->img.id == 0)
            {
                stbi_image_free(data);
                return nullptr;
            }

            r->img_w = x;
            r->img_h = y;
            r->charsz_x = x / 32;
            r->charsz_y = y / 14;

            if (monospaced) {
                for (int idx = 0; idx < 255; ++idx) {
                    r->kern[idx] = 0;
                }
            }
            else {
                bool printit = false;
                int char_w = x / 32;
                int char_h = y / 14;
                for (int idx = 0; idx < 255; ++idx) {
                    int i = qp_font_map[idx];
                    int px = (i & 0x1f) * char_w;
                    int py = (i / 32) * x * char_h;
                    printit = idx == '/';
                    int maxx = 0;
                    for (int cy = 0; cy < char_h; ++cy) {
                        for (int cx = 0; cx < char_w; ++cx) {
                            int addr = (py + (cy * x) + px + cx) * 4;
                            if (data[addr] != 0) {
                                if (printit)
                                    printf("A");
                                if (cx > maxx)
                                    maxx = cx;
                            }
                            else {
                                if (printit)
                                    printf(" ");
                            }
                        }
                        if (printit)
                            printf("\n");
                    }
                    r->kern[idx] = maxx - (x / 32);
                }
                if (word_space_override) {
                    r->kern[32] = word_space - (x / 32);
                }
                else {
                    r->kern[32] += char_w / 2;
                }
            }

            if (mono_numeric) {
                for (int i = '0'; i <= '9'; ++i)
                    r->kern[i] = -r->charspc_x;
            }

            stbi_image_free(data);
            fonts[key] = std::unique_ptr<LabFont>(r);
            return r;
        }
    }

    return nullptr;
}

LabFont* LabFontGet(const char* name)
{
    std::string key(name);
    auto it = fonts.find(key);
    if (it == fonts.end())
        return nullptr;

    return it->second.get();
}



static bool qp_must_init = true;
static sgl_pipeline qp_pip;

static void quadplay_font_init()
{
    sg_pipeline_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.colors[0].blend.enabled = true;
    desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    qp_pip = sgl_make_pipeline(&desc);
}

static float quadplay_font_draw(const char* str, const char* end, LabFontColor* c, float x, float y, LabFontState* fs)
{
    if (str == nullptr || fs == nullptr || fs->font->img.id <= 0)
        return x;

    if (end == nullptr) {
        end = str + strlen(str);
    }

    if (qp_must_init) {
        quadplay_font_init();
        qp_must_init = false;
    }

    sgl_push_pipeline();
    sgl_load_pipeline(qp_pip);
    sgl_enable_texture();
    sgl_texture(fs->font->img);
    sgl_begin_quads();
    sgl_c4b(c->rgba[0], c->rgba[1], c->rgba[2], c->rgba[3]);

    int px = fs->font->img_w / 32;
    int py = fs->font->img_h / 14;

    if (fs->alignment.alignment & LabFontAlignBottom)
        y -= py;
    else if (fs->alignment.alignment & LabFontAlignMiddle)
        y -= py / 2;
    else if (fs->alignment.alignment & LabFontAlignBaseline)
        y -= fs->font->baseline;

    if (fs->alignment.alignment & LabFontAlignCenter) {
        LabFontSize sz = LabFontMeasureSubstring(str, nullptr, fs);
        x -= sz.width / 2;
    }
    else if (fs->alignment.alignment & LabFontAlignRight) {
        LabFontSize sz = LabFontMeasureSubstring(str, nullptr, fs);
        x -= sz.width;
    }

    float fpx = px / float(fs->font->img_w);
    float fpy = py / float(fs->font->img_h);
    float idx = 0;
    int kern = 0;
    for (const char* p = str; p != end; p++, idx += 1.f) {
        int i = qp_font_map[*p];
        int ix = i & 0x1f;
        int iy = i / 32;

        float u0 = (float(ix) * px) / fs->font->img_w;
        float v0 = (float(iy) * py) / fs->font->img_h;
        float u1 = u0 + fpx;
        float v1 = v0 + fpy;
        float x0 = (x + kern) + (idx * px);
        float y0 = y;
        float x1 = x0 + px;
        float y1 = y0 + py;
        sgl_v2f_t2f(x0, y0, u0, v0);
        sgl_v2f_t2f(x1, y0, u1, v0);
        sgl_v2f_t2f(x1, y1, u1, v1);
        sgl_v2f_t2f(x0, y1, u0, v1);

        kern += fs->font->kern[*p] + fs->font->charspc_x;
    }

    sgl_end();
    sgl_pop_pipeline();
    sgl_disable_texture();
    return x + kern + (idx * px);
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
        LabFontColor c = { 255,255,255,255 };
        return quadplay_font_draw(str, nullptr, &c, x, y, fs);
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

    if (fs->font->id > 0) {
        LabFontInternal::fontstash_bind(fs);
        FONScontext* fc = LabFontInternal::fontStash();
        fonsSetColor(fc, LabFontInternal::fons_rgba(*c));
        return fonsDrawText(fc, x, y, str, end);
    }
    else if (fs->font->img.id > 0) {
        quadplay_font_draw(str, end, c, x, y, fs);
    }
    return x;
}

LabFontSize LabFontMeasure(const char* str, LabFontState* fs)
{
    return LabFontMeasureSubstring(str, NULL, fs);
}

LabFontSize LabFontMeasureSubstring(const char* str, const char* end, LabFontState* fs)
{
    if (!str || !fs || !fs->font)
        return { 0, 0, 0, 0 };

    if (fs->font->id > 0) {
        LabFontInternal::fontstash_bind(fs);
        FONScontext* fc = LabFontInternal::fontStash();

        LabFontSize sz;
        float bounds[4];
        sz.width = fonsTextBounds(fc, 0, 0, str, end, bounds);
        fonsVertMetrics(fc, &sz.ascender, &sz.descender, &sz.height);
        return sz;
    }
    else if (fs->font->img.id > 0) {
        LabFontSize sz;
        sz.ascender = 0;
        sz.descender = (float) (fs->font->charsz_y - fs->font->baseline);
        size_t len = end != nullptr ? end - str : strlen(str);

        float x = 0;
        float kern = 0;
        float idx = 0.f;
        float px = fs->font->img_w / 32.f;
        for (size_t c = 0; c < len; ++c, idx += 1.f) {
            unsigned char ch = *(str + c);
            int i = qp_font_map[ch];
            int ix = i & 0x1f;

            float u0 = (float(ix) * px) / fs->font->img_w;
            float u1 = u0 + px;
            float x0 = (x + kern) + (idx * px);
            float x1 = x0 + px;
            kern += fs->font->kern[ch] + fs->font->charspc_x;
        }
        sz.width = x + kern + (idx * px);
        sz.height = (float) fs->font->charsz_y;
        return sz;
    }
    return { 0, 0, 0, 0 };
}


void LabFontCommitTexture()
{
    // flush fontstash's font atlas to sokol-gfx texture
    sfons_flush(LabFontInternal::fontStash());
}

