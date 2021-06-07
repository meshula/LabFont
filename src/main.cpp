
#include "lab_sokol.h"
#include "microui.h"
#include "microui_renderer.h"
#include "LabZep.h"
#include "../LabFontDemo.h"

#define LABFONT_IMPL
#include "../LabFont.h"

#include <queue>
#include <string>

#ifdef _MSC_VER
void add_quit_menu() {}
#else
extern "C" void add_quit_menu();
#endif

std::string g_app_path;
static sgl_pipeline immediate_pipeline;

const std::string shader = R"R(
#version 330 core

uniform mat4 Projection;

// Coordinates  of the geometry
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

// Outputs to the pixel shader
out vec2 frag_tex_coord;
out vec4 frag_color;

void main()
{
    gl_Position = Projection * vec4(in_position.xyz, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_color = in_color;
}
)R";


int zep_x = 0;
int zep_y = 0;
int zep_w = 0;
int zep_h = 0;

typedef struct {
    float r, g, b;
} color_t;

static struct {
    mu_Context mu_ctx;
    char logbuf[64000];
    int logbuf_updated;
    color_t bg = { 90.f, 95.f, 100.f };
    float dpi_scale;

    LabFont* font_japanese = nullptr;
    LabFont* font_normal = nullptr;
    LabFont* font_italic = nullptr;
    LabFont* font_bold = nullptr;
    LabFont* font_cousine = nullptr;
} state;

static  char logbuf[64000];
static   int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };


static void write_log(const char *text) {
  if (logbuf[0]) { strcat(logbuf, "\n"); }
  strcat(logbuf, text);
  logbuf_updated = 1;
}

static void test_window(mu_Context* ctx) {
    /* do window */
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
        mu_Container* win = mu_get_current_container(ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        /* window info */
        if (mu_header(ctx, "Window Info")) {
            mu_Container* win = mu_get_current_container(ctx);
            char buf[64];
            int widths[] = { 54, -1 };
            mu_layout_row(ctx, 2, widths, 0);
            mu_label(ctx, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        /* labels + buttons */
        if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
            int widths[] = { 86, -110, -1 };
            mu_layout_row(ctx, 3, widths, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
            if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
            mu_label(ctx, "Test buttons 2:");
            if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
            if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
            if (mu_begin_popup(ctx, "Test Popup")) {
                mu_button(ctx, "Hello");
                mu_button(ctx, "World");
                mu_end_popup(ctx);
            }
        }

        /* tree */
        if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
            int widths[] = { 140, -1 };
            mu_layout_row(ctx, 2, widths, 0);
            mu_layout_begin_column(ctx);
            if (mu_begin_treenode(ctx, "Test 1")) {
                if (mu_begin_treenode(ctx, "Test 1a")) {
                    mu_label(ctx, "Hello");
                    mu_label(ctx, "world");
                    mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, "Test 1b")) {
                    if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
                    if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
                    mu_end_treenode(ctx);
                }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 2")) {
                int w2[] = { 54, 54 };
                mu_layout_row(ctx, 2, w2, 0);
                if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
                if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
                if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
                if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(ctx, "Checkbox 1", &checks[0]);
                mu_checkbox(ctx, "Checkbox 2", &checks[1]);
                mu_checkbox(ctx, "Checkbox 3", &checks[2]);
                mu_end_treenode(ctx);
            }
            mu_layout_end_column(ctx);

            mu_layout_begin_column(ctx);
            int w2[] = { -1 };
            mu_layout_row(ctx, 1, widths, 0);
            mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(ctx);
        }

        /* background color sliders */
        if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
            int w1[] = { -78, -1 };
            mu_layout_row(ctx, 2, w1, 74);
            /* sliders */
            mu_layout_begin_column(ctx);
            int w2[] = { 46, -1 };
            mu_layout_row(ctx, 2, w2, 0);
            mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* color preview */
            mu_Rect r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color((int)bg[0], (int)bg[1], (int)bg[2], 255));
            char buf[32];
            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}

static mu_Id zep_id;
static void log_window(mu_Context* ctx) {
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        /* output text panel */
        int w1[] = { -1 };
        mu_layout_row(ctx, 1, w1, -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container* panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, w1, -1);
        mu_text(ctx, logbuf);
        mu_end_panel(ctx);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        int w2[] = { -70, -1 };
        mu_layout_row(ctx, 2, w2, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id); // keep focus on the text box
            submitted = 1;
        }
        if (mu_button(ctx, "Submit"))
        {
            submitted = 1;
        }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}

static void zep_window(mu_Context* ctx) {
    if (mu_begin_window_ex(ctx, "Zep", mu_rect(100, 100, 800, 600), MU_OPT_NOFRAME)) {

        int w1[] = { -1 };

        static int zep_secret = 0x1337babe;
        mu_Id zep_id = mu_get_id(ctx, &zep_secret, sizeof(zep_secret));

        mu_layout_row(ctx, 1, w1, -1);
        mu_Rect rect = mu_layout_next(ctx);
        mu_update_control(ctx, zep_id, rect, 0);

        mu_layout_set_next(ctx, rect, 0);
        zep_x = rect.x;
        zep_y = rect.y;
        zep_w = rect.w;
        zep_h = rect.h;

        int w = 5;
        rect.x -= w;
        rect.y -= w;
        rect.w += w+w;
        rect.h += w+w;
        mu_draw_box_ex(ctx, rect, { 50, 50, 50, 255 }, w);

        mu_Command* cmd = mu_push_command(ctx, MU_COMMAND_ZEP, sizeof(mu_RectCommand));
        cmd->rect.rect = rect;

        mu_end_window(ctx);
    }
}


static int uint8_slider(mu_Context *ctx, uint8_t *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, (mu_Real) low, (mu_Real) high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = (uint8_t) tmp;
  mu_pop_id(ctx);
  return res;
}


static void style_window(mu_Context *ctx) {
  static struct { const char *label; int idx; } colors[] = {
    { "text:",         MU_COLOR_TEXT        },
    { "border:",       MU_COLOR_BORDER      },
    { "windowbg:",     MU_COLOR_WINDOWBG    },
    { "titlebg:",      MU_COLOR_TITLEBG     },
    { "titletext:",    MU_COLOR_TITLETEXT   },
    { "panelbg:",      MU_COLOR_PANELBG     },
    { "button:",       MU_COLOR_BUTTON      },
    { "buttonhover:",  MU_COLOR_BUTTONHOVER },
    { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
    { "base:",         MU_COLOR_BASE        },
    { "basehover:",    MU_COLOR_BASEHOVER   },
    { "basefocus:",    MU_COLOR_BASEFOCUS   },
    { "scrollbase:",   MU_COLOR_SCROLLBASE  },
    { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
    { NULL }
  };

  if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
    int sw = (int) (mu_get_current_container(ctx)->body.w * 0.14f);
    int w[] = { 80, sw, sw, sw, sw, -1 };
    mu_layout_row(ctx, 6, w, 0);
    for (int i = 0; colors[i].label; i++) {
      mu_label(ctx, colors[i].label);
      uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
      uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
      mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
    }
    mu_end_window(ctx);
  }
}

static void line(float sx, float sy, float ex, float ey)
{
    sgl_begin_lines();
    sgl_c4b(255, 255, 0, 128);
    sgl_v2f(sx, sy);
    sgl_v2f(ex, ey);
    sgl_end();
}

static int text_width_cb(mu_Font font, const char* text, int len) {
    (void)font;
    if (len == -1) {
        len = (int) strlen(text);
    }
    return r_get_text_width(text, len);
}

static int text_height_cb(mu_Font font) {
    (void)font;
    return r_get_text_height();
}

/* round to next power of 2 (see bit-twiddling-hacks) */
static int round_pow2(float v) {
    uint32_t vi = ((uint32_t) v) - 1;
    for (uint32_t i = 0; i < 5; i++) {
        vi |= (vi >> (1<<i));
    }
    return (int) (vi + 1);
}



/* initialization */
static void init(void) 
{
    state.dpi_scale = sapp_dpi_scale();

    /* setup sokol-gfx */
    sg_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.context = sapp_sgcontext();
    sg_setup(&desc);
    //__dbgui_setup(sapp_sample_count());

    /* setup sokol-gl */
    sgl_desc_t desc_t;
    memset(&desc_t, 0, sizeof(desc_t));
    sgl_setup(&desc_t);

    sg_pipeline_desc pipeline_desc;
    memset(&pipeline_desc, 0, sizeof(pipeline_desc));
    pipeline_desc.colors[0].blend.enabled = true;
    pipeline_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pipeline_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    immediate_pipeline = sgl_make_pipeline(&pipeline_desc);

    static std::string dsj_path = std::string(lab_font_demo_asset_base) + "DroidSansJapanese.ttf";
    state.font_japanese = LabFontLoad("sans-japanese", dsj_path.c_str(), LabFontType{ LabFontTypeTTF });
    static std::string dsr_path = std::string(lab_font_demo_asset_base) + "DroidSerif-Regular.ttf";
    state.font_normal = LabFontLoad("serif-normal", dsr_path.c_str(), LabFontType{ LabFontTypeTTF });
    static std::string dsi_path = std::string(lab_font_demo_asset_base) + "DroidSerif-Italic.ttf";
    state.font_italic = LabFontLoad("serif-italic", dsi_path.c_str(), LabFontType{ LabFontTypeTTF });
    static std::string dsb_path = std::string(lab_font_demo_asset_base) + "DroidSerif-Bold.ttf";
    state.font_bold = LabFontLoad("serif-bold", dsb_path.c_str(), LabFontType{ LabFontTypeTTF });
    static std::string csr_path = std::string(lab_font_demo_asset_base) + "Cousine-Regular.ttf";
    state.font_cousine = LabFontLoad("cousine-regular", csr_path.c_str(), LabFontType{ LabFontTypeTTF });
    
    add_quit_menu();
}


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

static LabZep* zep = nullptr;

static struct {
    float mouse_pos_x, mouse_pos_y;
    bool lmb_clicked = false;
    bool lmb_released = false;
    bool rmb_clicked = false;
    bool rmb_released = false;
} zi;

static void event(const sapp_event* ev) {

    static bool ctrl_l = false;
    static bool ctrl_r = false;
    static bool shift_l = false;
    static bool shift_r = false;

    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.lmb_clicked = true;
            zi.lmb_released = false;
            
            mu_input_mousedown(&state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.lmb_clicked = false;
            zi.lmb_released = true;
            
            mu_input_mouseup(&state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            
            mu_input_mousemove(&state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y);
            break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            mu_input_scroll(&state.mu_ctx, 0, (int)ev->scroll_y);
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
            switch (ev->key_code & 511) {
            case SAPP_KEYCODE_LEFT_SHIFT: shift_l = true; break;
            case SAPP_KEYCODE_RIGHT_SHIFT: shift_r = true; break;
            case SAPP_KEYCODE_LEFT_CONTROL: ctrl_l = true; break;
            case SAPP_KEYCODE_RIGHT_CONTROL: ctrl_r = true; break;
                //case SAPP_KEYCODE_LEFT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_RIGHT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_ENTER: return MU_KEY_RETURN;
                //case SAPP_KEYCODE_BACKSPACE: return MU_KEY_BACKSPACE;
            default:
            {
                if (state.mu_ctx.focus == zep_id && ev->key_code >= SAPP_KEYCODE_ESCAPE) {
                    LabZep_input_sokol_key(zep, ev->key_code, shift_l || shift_r, ctrl_l || ctrl_r);
                }
                break;
            }
            }
            mu_input_keydown(&state.mu_ctx, key_map(ev->key_code));
            break;

            case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code & 511) {
            case SAPP_KEYCODE_LEFT_SHIFT: shift_l = false; break;
            case SAPP_KEYCODE_RIGHT_SHIFT: shift_r = false; break;
            case SAPP_KEYCODE_LEFT_CONTROL: ctrl_l = false; break;
            case SAPP_KEYCODE_RIGHT_CONTROL: ctrl_r = false; break;
                //case SAPP_KEYCODE_LEFT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_RIGHT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_ENTER: return MU_KEY_RETURN;
                //case SAPP_KEYCODE_BACKSPACE: return MU_KEY_BACKSPACE;
            }

            mu_input_keyup(&state.mu_ctx, key_map(ev->key_code));
            break;

        case SAPP_EVENTTYPE_CHAR:
            if (state.mu_ctx.focus == zep_id) {
                LabZep_input_sokol_key(zep, ev->char_code, shift_l || shift_r, ctrl_l || ctrl_r);
            }
            else {
                char txt[2] = { (char)(ev->char_code & 255), 0 };
                mu_input_text(&state.mu_ctx, txt);
            }
            break;

        default:
            break;
    }
}

void fontDemo(float& dx, float& dy, float sx, float sy) {

    if (state.font_japanese == nullptr)
        return;

    float dpis = state.dpi_scale;
    float sz;

    //-------------------------------------
    sz = 80.f * dpis;
    static LabFontState* a_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    static LabFontState* b_st = LabFontStateBake(state.font_italic, sz, { {128, 128, 0, 255} },   LabFontAlign{ 0 }, 0, 0);
    static LabFontState* c_st = LabFontStateBake(state.font_italic, sz, { {255, 255, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    static LabFontState* d_st = LabFontStateBake(state.font_bold,   sz, { {255, 255, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    dx = sx;
    dy += 30.f * dpis;
    dx = LabFontDraw("The quick ", dx, dy, a_st);
    dx = LabFontDraw("brown ", dx, dy, b_st);
    dx = LabFontDraw("fox ", dx, dy, a_st);
    dx = LabFontDraw("jumps over ", dx, dy, c_st);
    dx = LabFontDraw("the lazy ", dx, dy, c_st);
    dx = LabFontDraw("dog ", dx, dy, a_st);
    //-------------------------------------
    dx = sx;
    dy += sz * 1.2f;
    sz = 24.f * dpis;
    static LabFontState* e_st = LabFontStateBake(state.font_normal, sz, { {0, 125, 255, 255} }, LabFontAlign{ 0 }, 0, 0);
    LabFontDraw("Now is the time for all good men to come to the aid of the party.", dx, dy, e_st);
    //-------------------------------------
    dx = sx;
    dy += sz * 1.2f * 2;
    sz = 18.f * dpis;
    static LabFontState* f_st = LabFontStateBake(state.font_italic, sz, { {255, 255, 0, 255} }, LabFontAlign{ 0 }, 0, 0);
    LabFontDraw("Ég get etið gler án þess að meiða mig.", dx, dy, f_st);
    //-------------------------------------
    static LabFontState* j_st = LabFontStateBake(state.font_japanese, 24, { {128, 0, 0, 255} }, LabFontAlign{ 0 }, 0, 0);
    dx = sx;
    dy += 20.f * dpis;
    LabFontDraw("いろはにほへと ちりぬるを わかよたれそ つねならむ うゐのおくやま けふこえて あさきゆめみし ゑひもせす　京（ん）", dx, dy, j_st);
    //-------------------------------------
    sz = 18.f * dpis;
    static LabFontState* p_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignTop }, 0, 0);
    dx = 50 * dpis; dy = 350 * dpis;
    line(dx - 10 * dpis, dy, dx + 250 * dpis, dy);
    dx = LabFontDraw("Top", dx, dy, p_st);
    static LabFontState* g_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignMiddle }, 0, 0);
    dx += 10 * dpis;
    dx = LabFontDraw("Middle", dx, dy, g_st);
    dx += 10 * dpis;
    static LabFontState* q_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBaseline }, 0, 0);
    dx = LabFontDraw("Baseline", dx, dy, q_st);
    dx += 10 * dpis;
    static LabFontState* h_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBottom }, 0, 0);
    LabFontDraw("Bottom", dx, dy, h_st);
    dx = 150 * dpis; dy = 400 * dpis;
    line(dx, dy - 30 * dpis, dx, dy + 80.0f * dpis);
    static LabFontState* i_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0, 0);
    static LabFontState* k_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignCenter | LabFontAlignBaseline }, 0, 0);
    static LabFontState* l_st = LabFontStateBake(state.font_normal, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignRight | LabFontAlignBaseline }, 0, 0);
    LabFontDraw("Left", dx, dy, i_st);
    dy += 30 * dpis;
    LabFontDraw("Center", dx, dy, k_st);
    dy += 30 * dpis;
    LabFontDraw("Right", dx, dy, l_st);
    //-------------------------------------
    dx = 500 * dpis; dy = 350 * dpis;
    sz = 60.f * dpis;
    static LabFontState* m_st = LabFontStateBake(state.font_italic, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 5.f * dpis, 10.f);
    LabFontDraw("Blurry...", dx, dy, m_st);
    //-------------------------------------
    dy += sz;
    sz = 18.f * dpis;
    static LabFontState* n_st = LabFontStateBake(state.font_bold, sz, { {0,0,0, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0.f, 3.f);
    static LabFontState* o_st = LabFontStateBake(state.font_bold, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0.f, 0.f);
    LabFontDraw("DROP THAT SHADOW", dx+5*dpis, dy+5*dpis, n_st);
    LabFontDraw("DROP THAT SHADOW", dx, dy, o_st);
}

/* do one frame */
void frame(void) {

    static bool init_zep = true;
    if (init_zep && state.font_cousine >= 0)
    {
        int fontPixelHeight = 20;
        static LabFontState* microui_st = LabFontStateBake(state.font_cousine, 
            (float) fontPixelHeight, { {255, 255, 255, 255} }, 
            LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);

        /* setup microui renderer */
        r_init(microui_st);

        /* setup microui */
        mu_init(&state.mu_ctx);
        state.mu_ctx.text_width = text_width_cb;
        state.mu_ctx.text_height = text_height_cb;

        fontPixelHeight = 18;
        static LabFontState* zep_st = LabFontStateBake(state.font_cousine, (float) fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
        zep = LabZep_create(zep_st, "Shader.frag", shader.c_str());
        init_zep = false;
    }

    if (init_zep)
        return;
    
    /* text rendering via fontstash.h */
    float sx, sy, dx, dy, lh = 0.0f;
    const float dpis = state.dpi_scale;
    sx = 50*dpis; sy = 50*dpis;
    dx = sx; dy = sy;

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float) sapp_width(), (float) sapp_height(), 0.0f, -1.0f, +1.0f);
    sgl_scissor_rect(0, 0, sapp_width(), sapp_height(), true);

    fontDemo(dx, dy, sx, sy);

    //---- UI layer
    if (true) {
        /* UI definition */
        mu_begin(&state.mu_ctx);
        test_window(&state.mu_ctx);
        log_window(&state.mu_ctx);
        zep_window(&state.mu_ctx);
        style_window(&state.mu_ctx);
        mu_end(&state.mu_ctx);

        LabZep_process_events(zep, zep_x, zep_y, zep_w, zep_h,
            zi.mouse_pos_x, zi.mouse_pos_y, zi.lmb_clicked, zi.lmb_released,
            zi.rmb_clicked, zi.rmb_released);

        /* micro-ui rendering */
        r_begin(sapp_width(), sapp_height());
        //r_clear(mu_color(bg[0], bg[1], bg[2], 255));
        mu_Command* cmd = 0;
        while(mu_next_command(&state.mu_ctx, &cmd)) {
            switch (cmd->type) {
                case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
                case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
                case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
                case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
                case MU_COMMAND_ZEP: LabZep_render(zep); break;
            }
        }
        r_end();
    }

    LabFontCommitTexture();

    // begin rendering with a clear pass
    sg_pass_action pass_action;
    memset(&pass_action, 0, sizeof(pass_action));
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].value = { state.bg.r / 255.0f, state.bg.g / 255.0f, state.bg.b / 255.0f, 1.0f };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());

    // draw all the sgl stuff
    sgl_draw();

    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
   // __cdbgui_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

extern "C"
sapp_desc sokol_main(int argc, char* argv[])
{
    std::string app_path(argv[0]);
    size_t index = app_path.rfind('/');
    if (index == std::string::npos)
        index = app_path.rfind('\\');
    g_app_path = app_path.substr(0, index);

    sapp_desc desc = { };
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = event;
    desc.width = 1200;
    desc.height = 1000;
    desc.gl_force_gles2 = true;
    desc.window_title = "LabFont";
    desc.ios_keyboard_resizes_canvas = false;

    return desc;
}
