
#include "../LabMicroUI.h"

#ifdef LABFONT_HAVE_SOKOL
#include "../LabSokol.h"
#endif

#include "microui.h"
#include "../LabZep.h"

#include "../LabFont.h"
#include <stdlib.h>
#include <string.h>


struct LabFontState;

extern "C" void r_begin(LabFontDrawState*, 
                        int disp_width, int disp_height);
extern "C" void r_end(void);
extern "C" void r_draw(void);
extern "C" void r_init(LabFontState * font);
extern "C" void r_draw_rect(mu_Rect rect, mu_Color color);
extern "C" void r_draw_text(LabFontDrawState* ds,
                            const char* text, mu_Vec2 pos, mu_Color color);
extern "C" void r_draw_icon(int id, mu_Rect rect, mu_Color color);
extern "C" int r_get_text_width(const char* text, int len);
extern "C" int r_get_text_height(void);
extern "C" void r_set_clip_rect(mu_Rect rect);
//extern "C" void r_clear(mu_Color color);
//extern "C" void r_present();



/*== micrui renderer =========================================================*/
LabFontState* font = nullptr;
static LabFontSize font_size;

extern "C"
void r_init(LabFontState * font_) {

    font = font_;
    font_size = LabFontMeasure("M", font_);
}

extern "C"
void r_begin(LabFontDrawState* ds, int disp_width, int disp_height) {
#ifdef LABFONT_HAVE_SOKOL
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_push_matrix();
    sgl_ortho(0.0f, (float)disp_width, (float)disp_height, 0.0f, -1.0f, +1.0f);
#else
#endif
}

extern "C"
void r_end(void) {
#ifdef LABFONT_HAVE_SOKOL
    sgl_pop_matrix();
#else
#endif
}

extern "C"
void r_draw(void) {
#ifdef LABFONT_HAVE_SOKOL
    sgl_draw();
#endif
}

extern "C"
void r_push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
#ifdef LABFONT_HAVE_SOKOL
    float x0 = (float)dst.x;
    float y0 = (float)dst.y;
    float x1 = (float)(dst.x + dst.w);
    float y1 = (float)(dst.y + dst.h);

    sgl_begin_quads();
    sgl_c4b(color.r, color.g, color.b, color.a);
    sgl_v2f(x0, y0);
    sgl_v2f(x1, y0);
    sgl_v2f(x1, y1);
    sgl_v2f(x0, y1);
    sgl_end();
#else
#endif
}

extern "C"
void r_draw_rect(mu_Rect rect, mu_Color color) {
    r_push_quad(rect, rect, color);
}

inline uint32_t ToPackedABGR(const mu_Color* val)
{
    uint32_t col = 0;
    col |= uint32_t(val->r * 255.0f) << 24;
    col |= uint32_t(val->g * 255.0f);
    col |= uint32_t(val->b * 255.0f) << 8;
    col |= uint32_t(val->a * 255.0f) << 16;
    return col;
}

extern "C"
void r_draw_text(LabFontDrawState* ds, const char* text, mu_Vec2 pos, mu_Color color) {
#if 1
    LabFontColor c;
    c.rgba[0] = color.r;
    c.rgba[1] = color.g;
    c.rgba[2] = color.b;
    c.rgba[3] = color.a;
    LabFontDrawColor(ds, text, &c, (float) pos.x, (float) pos.y, font);
#else
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (const char* p = text; *p; p++) {
        mu_Rect src = atlas[ATLAS_FONT + (unsigned char)*p];
        dst.w = src.w;
        dst.h = src.h;
        r_push_quad(dst, src, color);
        dst.x += dst.w;
    }
#endif
}

extern "C"
int r_get_text_width(const char* text, int len) {
#if 1
    LabFontSize sz = LabFontMeasureSubstring(text, text + len, font);
    return (int) sz.width;
#else
    int res = 0;
    for (const char* p = text; *p && len--; p++) {
        res += atlas[ATLAS_FONT + (unsigned char)*p].w;
    }
    return res;
#endif
}

extern "C"
void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
#ifdef LABFONT_HAVE_SOKOL
    float w2 = rect.w * 0.5f;
    float h2 = rect.h * 0.5f;
    float x = rect.x + w2 * 0.5f;
    float y = rect.y + h2 * 0.5f;
    switch (id) {
    case MU_ICON_CLOSE:
        sgl_begin_lines();
        sgl_c4b(color.r, color.g, color.b, color.a);
        sgl_v2f(x, y);
        sgl_v2f(x + w2, y + h2);
        sgl_v2f(x + w2, y);
        sgl_v2f(x, y + h2);
        sgl_end();
        break;
    case MU_ICON_CHECK:
        sgl_begin_lines();
        sgl_c4b(color.r, color.g, color.b, color.a);
        sgl_v2f(x, y + rect.h * 0.2f);
        sgl_v2f(x + rect.w * 0.1f, y + rect.h * 0.6f);

        sgl_v2f(x + rect.w * 0.1f, y + rect.h * 0.6f);
        sgl_v2f(x + rect.w * 0.6f, y - rect.h * 0.1f);
        sgl_end();
        break;
    case MU_ICON_EXPANDED:
        sgl_begin_lines();
        sgl_c4b(color.r, color.g, color.b, color.a);
        sgl_v2f(x + rect.w * 0.4f, y + h2 * 0.1f);
        sgl_v2f(x + rect.w * 0.4f, y + h2 * 0.9f);

        sgl_v2f(x + rect.w * 0.4f, y + h2 * 0.9f);
        sgl_v2f(x, y + h2 * 0.9f);

        sgl_v2f(x, y + h2 * 0.9f);
        sgl_v2f(x + rect.w * 0.4f, y + h2 * 0.1f);
        sgl_end();
        break;
    case MU_ICON_COLLAPSED:
        sgl_begin_lines();
        sgl_c4b(color.r, color.g, color.b, color.a);
        sgl_v2f(x, y);
        sgl_v2f(x, y + h2);

        sgl_v2f(x, y + h2);
        sgl_v2f(x + w2 * 0.5f, y + h2 * 0.5f);

        sgl_v2f(x + w2 * 0.5f, y + h2 * 0.5f);
        sgl_v2f(x, y);
        sgl_end();
        break;
    }
#else
#endif
}


extern "C"
int r_get_text_height(void) {
    return (int)font_size.height;
}

extern "C"
void r_set_clip_rect(mu_Rect rect) {
#ifdef LABFONT_HAVE_SOKOL
    sgl_scissor_rect(rect.x, rect.y, rect.w, rect.h, true);
#else
#endif
}


static int text_width_cb(mu_Font font, const char* text, int len) {
    (void)font;
    if (len == -1) {
        len = (int)strlen(text);
    }
    return r_get_text_width(text, len);
}

static int text_height_cb(mu_Font font) {
    (void)font;
    return r_get_text_height();
}


//  _       _ _   
// (_)_ __ (_) |_ 
// | | '_ \| | __|
// | | | | | | |_ 
// |_|_| |_|_|\__|

static mu_Context ctx;
extern "C" mu_Context* lab_microui_init(LabFontState * fs)
{
    /* setup microui renderer */
    r_init(fs);

    /* setup microui */
    mu_init(&ctx);
    ctx.text_width = text_width_cb;
    ctx.text_height = text_height_cb;
    return &ctx;
}

extern "C" void lab_microui_render(LabFontDrawState* ds, int w, int h, LabZep* zep)
{
    /* micro-ui rendering */
    r_begin(ds, w, h);
    //r_clear(mu_color(bg[0], bg[1], bg[2], 255));
    mu_Command* cmd = 0;
    while (mu_next_command(&ctx, &cmd)) {
        switch (cmd->type) {
        case MU_COMMAND_TEXT: r_draw_text(ds, cmd->text.str, cmd->text.pos, cmd->text.color); break;
        case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
        case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
        case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
        case MU_COMMAND_ZEP: LabZep_render(zep); break;
        }
    }
    r_end();
}

#ifdef LABFONT_HAVE_SOKOL

const int key_map(int c)
{
    switch (c & 511) {
    case SAPP_KEYCODE_LEFT_SHIFT: return MU_KEY_SHIFT;
    case SAPP_KEYCODE_RIGHT_SHIFT: return MU_KEY_SHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: return MU_KEY_CTRL;
    case SAPP_KEYCODE_RIGHT_CONTROL: return MU_KEY_CTRL;
    case SAPP_KEYCODE_LEFT_ALT: return MU_KEY_ALT;
    case SAPP_KEYCODE_RIGHT_ALT: return MU_KEY_ALT;
    case SAPP_KEYCODE_ENTER: return MU_KEY_RETURN;
    case SAPP_KEYCODE_BACKSPACE: return MU_KEY_BACKSPACE;
    }
    return 0;// c & 511;
}

extern "C" void lab_microui_input_sokol_keydown(int c)
{
    mu_input_keydown(&ctx, key_map(c));
}

extern "C" void lab_microui_input_sokol_keyup(int c)
{
    mu_input_keyup(&ctx, key_map(c));
}

extern "C" void lab_microui_input_sokol_text(int c)
{
    char txt[2] = { (char)(c & 255), 0 };
    mu_input_text(&ctx, txt);
}

#endif
