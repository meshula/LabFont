
#include "../LabMicroUI.h"
#include "../LabDrawImmediate.h"
#include "../LabDrawImmediate-metal.h"

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
extern "C" void r_init(LabImmDrawContext* imm_ctx, LabFontState * font);
extern "C" void r_draw_rect(mu_Rect rect, mu_Color color);
extern "C" void r_draw_text(LabFontDrawState* ds,
                            const char* text, mu_Vec2 pos, mu_Color color);
extern "C" void r_draw_icon(int id, mu_Rect rect, mu_Color color);
extern "C" int r_get_text_width(const char* text, int len);
extern "C" int r_get_text_height(void);
extern "C" void r_set_clip_rect(mu_Rect rect);


/*== microui renderer =========================================================*/

static LabFontState* _microui_current_font = nullptr;
static LabFontSize font_size;
static LabImmPlatformContext* _imm_ctx = nullptr;

extern "C"
void r_init(LabImmDrawContext* imm_ctx, LabFontState * font_) {
    _imm_ctx = imm_ctx;
    _microui_current_font = font_;
    font_size = LabFontMeasure("", font_);
}

extern "C"
void r_begin(LabFontDrawState* ds, int width, int height) {
    lab_imm_viewport_set(_imm_ctx, 0, 0, width, height);
}

extern "C"
void r_end(void) {
}

extern "C"
void r_draw(void) {
}

extern "C"
void r_push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    float x0 = (float)dst.x;
    float y0 = (float)dst.y;
    float x1 = (float)(dst.x + dst.w);
    float y1 = (float)(dst.y + dst.h);

    const int vert_count = 4;
    static size_t buff_size = lab_imm_size_bytes(vert_count);
    static float* buff = (float*) malloc(buff_size);
    LabImmContext lic;
    lab_imm_batch_begin(&lic, _imm_ctx, vert_count, labprim_tristrip, false, buff);

    lab_imm_c4f(&lic, (float)color.r / 255.f, (float)color.g / 255.f, (float)color.b / 255.f, (float)color.a / 255.f);

    lab_imm_v2f(&lic, x0, y0);
    lab_imm_v2f(&lic, x1, y0);
    lab_imm_v2f(&lic, x0, y1);
    lab_imm_v2f(&lic, x1, y1);

    // texture slot 1 ~~ solid color
    lab_imm_batch_draw(&lic, 1, true);
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
    LabFontColor c;
    c.rgba[0] = color.r;
    c.rgba[1] = color.g;
    c.rgba[2] = color.b;
    c.rgba[3] = color.a;
    LabFontDrawColor(ds, text, &c, (float) pos.x, (float) pos.y, _microui_current_font);
}

extern "C"
int r_get_text_width(const char* text, int len) {
    LabFontSize sz = LabFontMeasureSubstring(text, text + len, _microui_current_font);
    return (int) sz.width;
}

extern "C"
void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    const int vert_count = 24;
    static size_t buff_size = lab_imm_size_bytes(vert_count);
    static float* buff = (float*) malloc(buff_size);
    LabImmContext lic;
    lab_imm_batch_begin(&lic, _imm_ctx, vert_count, labprim_triangles, false, buff);

    lab_imm_c4f(&lic, (float)color.r / 255.f, (float)color.g / 255.f, (float)color.b / 255.f, (float)color.a / 255.f);

    float w2 = rect.w * 0.5f;
    float h2 = rect.h * 0.5f;
    float x = rect.x + w2 * 0.5f;
    float y = rect.y + h2 * 0.5f;
    float w = 1.5f;
    switch (id) {
        case MU_ICON_CLOSE:
            lab_imm_line(&lic, x, y,    x+w2, y+h2, w); // '\'
            lab_imm_line(&lic, x, y+h2, x+w2, y,    w); // '/'
            break;
        case MU_ICON_CHECK:
            lab_imm_line(&lic, x,                 y + rect.h * 0.25f, x + rect.w * 0.2f, y + rect.h * 0.6f, w);
            lab_imm_line(&lic, x + rect.w * 0.2f, y + rect.h * 0.6f,  x + rect.w * 0.6f, y - rect.h * 0.1f, w);
            break;
        case MU_ICON_EXPANDED:
            lab_imm_line(&lic, x + rect.w * 0.4f, y + h2 * 0.1f, x + rect.w * 0.4f, y + h2 * 0.9f, w);
            lab_imm_line(&lic, x + rect.w * 0.4f, y + h2 * 0.9f, x,                 y + h2 * 0.9f, w);
            lab_imm_line(&lic, x,                 y + h2 * 0.9f, x + rect.w * 0.4f, y + h2 * 0.1f, w);
            break;
        case MU_ICON_COLLAPSED:
            lab_imm_line(&lic, x,             y,             x,             y + h2, w);
            lab_imm_line(&lic, x,             y + h2,        x + w2 * 0.5f, y + h2 * 0.5f, w);
            lab_imm_line(&lic, x + w2 * 0.5f, y + h2 * 0.5f, x,             y, w);
            break;
    }

    // texture slot 1 ~~ solid color
    lab_imm_batch_draw(&lic, 1, true);
}


extern "C"
int r_get_text_height(void) {
    return (int)font_size.height;
}

extern "C"
void r_set_clip_rect(mu_Rect rect) {
    if (rect.x > _imm_ctx->viewport.width || rect.y > _imm_ctx->viewport.height)
        return;
    
    double max_width = _imm_ctx->viewport.width - rect.x;
    NSUInteger w = (NSUInteger) (max_width < rect.w ? max_width : rect.w);
    double max_height = _imm_ctx->viewport.height - rect.y;
    NSUInteger h = (NSUInteger) (max_height < rect.h ? max_height : rect.h);
    MTLScissorRect r = { (NSUInteger)rect.x, (NSUInteger)rect.y, w, h };
    [_imm_ctx->currentRenderCommandEncoder setScissorRect:r];
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

static mu_Context ctx;
extern "C" mu_Context* lab_microui_init(LabImmDrawContext* imm_ctx, LabFontState * fs)
{
    /* setup microui renderer */
    r_init(imm_ctx, fs);

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
