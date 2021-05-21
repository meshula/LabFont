
#include "lab_sokol.h"
#include "microui.h"
#include "microui_renderer.h"
#include "fontstash.h"
#include "zep_display.h"
#include "../MeshulaFont.h"

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


uint32_t sappZepKey(int c)
{
    switch (c) 
    {
    case SAPP_KEYCODE_TAB: return Zep::ExtKeys::TAB;
    case SAPP_KEYCODE_ESCAPE: return Zep::ExtKeys::ESCAPE;
    case SAPP_KEYCODE_ENTER: return Zep::ExtKeys::RETURN;
    case SAPP_KEYCODE_DELETE: return Zep::ExtKeys::DEL;
    case SAPP_KEYCODE_HOME: return Zep::ExtKeys::HOME;
    case SAPP_KEYCODE_END: return Zep::ExtKeys::END;
    case SAPP_KEYCODE_BACKSPACE: return Zep::ExtKeys::BACKSPACE;
    case SAPP_KEYCODE_RIGHT: return Zep::ExtKeys::RIGHT;
    case SAPP_KEYCODE_LEFT: return Zep::ExtKeys::LEFT;
    case SAPP_KEYCODE_UP: return Zep::ExtKeys::UP;
    case SAPP_KEYCODE_DOWN: return Zep::ExtKeys::DOWN;
    case SAPP_KEYCODE_PAGE_DOWN: return Zep::ExtKeys::PAGEDOWN;
    case SAPP_KEYCODE_PAGE_UP: return Zep::ExtKeys::PAGEUP;
    case SAPP_KEYCODE_F1: return Zep::ExtKeys::F1;
    case SAPP_KEYCODE_F2: return Zep::ExtKeys::F2;
    case SAPP_KEYCODE_F3: return Zep::ExtKeys::F3;
    case SAPP_KEYCODE_F4: return Zep::ExtKeys::F4;
    case SAPP_KEYCODE_F5: return Zep::ExtKeys::F5;
    case SAPP_KEYCODE_F6: return Zep::ExtKeys::F6;
    case SAPP_KEYCODE_F7: return Zep::ExtKeys::F7;
    case SAPP_KEYCODE_F8: return Zep::ExtKeys::F8;
    case SAPP_KEYCODE_F9: return Zep::ExtKeys::F9;
    case SAPP_KEYCODE_F10: return Zep::ExtKeys::F10;
    case SAPP_KEYCODE_F11: return Zep::ExtKeys::F11;
    case SAPP_KEYCODE_F12: return Zep::ExtKeys::F12;
    default: return c;
    }
}




typedef struct {
    float r, g, b;
} color_t;

static struct {
    mu_Context mu_ctx;
    char logbuf[64000];
    int logbuf_updated;
    color_t bg = { 90.f, 95.f, 100.f };

    FONScontext* fons;
    float dpi_scale;
    int font_normal;
    int font_italic;
    int font_bold;
    int font_japanese;
    int font_cousine;
    uint8_t font_normal_data[256 * 1024];
    uint8_t font_italic_data[256 * 1024];
    uint8_t font_bold_data[256 * 1024];
    uint8_t font_cousine_data[256 * 1024];
    uint8_t font_japanese_data[2 * 1024 * 1024];

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
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            char buf[32];
            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}


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
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}


static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
  static float tmp;
  mu_push_id(ctx, &value, sizeof(value));
  tmp = *value;
  int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
  *value = tmp;
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
    int sw = mu_get_current_container(ctx)->body.w * 0.14;
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


/* sokol-fetch load callbacks */
static void font_normal_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_normal = fonsAddFontMem(state.fons, "sans", (unsigned char*) response->buffer_ptr, (int)response->fetched_size,  false);
    }
    if (response->finished) {
        // the 'finished'-flag is the catch-all flag for when the request
        // is finished, no matter if loading was successful or failed,
        // so any cleanup-work should happen here...
        if (response->failed) {
            // 'failed' is true in (addition to 'finished') if something
            // went wrong (file doesn't exist, or less bytes could be
            // read from the file than expected)
            printf("Normal font not found\n");
        }
    }
}

static void font_italic_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_italic = fonsAddFontMem(state.fons, "sans-italic", (unsigned char*) response->buffer_ptr, (int)response->fetched_size, false);
    }
    if (response->finished) {
        if (response->failed) {
            printf("Italic font not found\n");
        }
    }
}

static void font_bold_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_bold = fonsAddFontMem(state.fons, "sans-bold", (unsigned char*) response->buffer_ptr, (int)response->fetched_size, false);
    }
    if (response->finished) {
        if (response->failed) {
            printf("Bold Italic font not found\n");
        }
    }
}

static void font_japanese_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_japanese = fonsAddFontMem(state.fons, "sans-japanese", (unsigned char*) response->buffer_ptr, (int)response->fetched_size, false);
    }
    if (response->finished) {
        if (response->failed) {
            printf("Japanese font not found\n");
        }
    }
}

static void font_cousine_loaded(const sfetch_response_t* response) {
    if (response->fetched) {
        state.font_cousine = fonsAddFontMem(state.fons, "sans-cousine", (unsigned char*) response->buffer_ptr, (int)response->fetched_size, false);
    }
    if (response->finished) {
        if (response->failed) {
            printf("Cousine font not found\n");
        }
    }
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

    /* make sure the fontstash atlas width/height is pow-2 */
    const int atlas_dim = round_pow2(512.0f * state.dpi_scale);
    FONScontext* fons_context = sfons_create(atlas_dim, atlas_dim, FONS_ZERO_TOPLEFT);
    state.fons = fons_context;
    state.font_normal = FONS_INVALID;
    state.font_italic = FONS_INVALID;
    state.font_bold = FONS_INVALID;
    state.font_japanese = FONS_INVALID;
    state.font_cousine = FONS_INVALID;

    /* use sokol_fetch for loading the TTF font files */
    sfetch_desc_t sfetch_desc;
    memset(&sfetch_desc, 0, sizeof(sfetch_desc));
    sfetch_desc.num_channels = 1;
    sfetch_desc.num_lanes = 4;
    sfetch_setup(&sfetch_desc);
    sfetch_request_t sfetch_request;
    memset(&sfetch_request, 0, sizeof(sfetch_request));

    static std::string dsr_path = std::string(meshula_font_asset_base) + "DroidSerif-Regular.ttf";
    sfetch_request.path = dsr_path.c_str();
    sfetch_request.callback = font_normal_loaded;
    sfetch_request.buffer_ptr = state.font_normal_data;
    sfetch_request.buffer_size = sizeof(state.font_normal_data);
    sfetch_send(&sfetch_request);

    static std::string dsi_path = std::string(meshula_font_asset_base) + "DroidSerif-Italic.ttf";
    sfetch_request.path = dsi_path.c_str();
    sfetch_request.callback = font_italic_loaded;
    sfetch_request.buffer_ptr = state.font_italic_data;
    sfetch_request.buffer_size = sizeof(state.font_italic_data);
    sfetch_send(&sfetch_request);

    static std::string dsb_path = std::string(meshula_font_asset_base) + "DroidSerif-Bold.ttf";
    sfetch_request.path = dsb_path.c_str();
    sfetch_request.callback = font_bold_loaded;
    sfetch_request.buffer_ptr = state.font_bold_data;
    sfetch_request.buffer_size = sizeof(state.font_bold_data);
    sfetch_send(&sfetch_request);

    static std::string dsj_path = std::string(meshula_font_asset_base) + "DroidSansJapanese.ttf";
    sfetch_request.path = dsj_path.c_str();
    sfetch_request.callback = font_japanese_loaded;
    sfetch_request.buffer_ptr = state.font_japanese_data;
    sfetch_request.buffer_size = sizeof(state.font_japanese_data);
    sfetch_send(&sfetch_request);

    static std::string csr_path = std::string(meshula_font_asset_base) + "Cousine-Regular.ttf";
    sfetch_request.path = csr_path.c_str();
    sfetch_request.callback = font_cousine_loaded;
    sfetch_request.buffer_ptr = state.font_cousine_data;
    sfetch_request.buffer_size = sizeof(state.font_cousine_data);
    sfetch_send(&sfetch_request);
    
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
    return c & 511;
}

struct ZepInputs {
    float mouse_delta_x; float mouse_delta_y;
    float mouse_pos_x; float mouse_pos_y;
    bool lmb_clicked; bool lmb_released;
    bool rmb_clicked; bool rmb_released;
    bool ctrl_down; bool shift_down;

    std::deque<uint32_t> keys;
};

static ZepInputs zi;

static void event(const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.mouse_delta_x = ev->mouse_dx;
            zi.mouse_delta_y = ev->mouse_dy;
            zi.lmb_clicked = true;
            zi.lmb_released = false;
            
            mu_input_mousedown(&state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.mouse_delta_x = ev->mouse_dx;
            zi.mouse_delta_y = ev->mouse_dy;
            zi.lmb_clicked = false;
            zi.lmb_released = true;
            
            mu_input_mouseup(&state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.mouse_delta_x = ev->mouse_dx;
            zi.mouse_delta_y = ev->mouse_dy;
            
            mu_input_mousemove(&state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y);
            break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            mu_input_scroll(&state.mu_ctx, 0, (int)ev->scroll_y);
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
            switch (ev->key_code & 511) {
            case SAPP_KEYCODE_LEFT_SHIFT: zi.shift_down = true; break;
            case SAPP_KEYCODE_RIGHT_SHIFT: zi.shift_down = true; break;
            case SAPP_KEYCODE_LEFT_CONTROL: zi.ctrl_down = true; break;
            case SAPP_KEYCODE_RIGHT_CONTROL: zi.ctrl_down = true; break;
                //case SAPP_KEYCODE_LEFT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_RIGHT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_ENTER: return MU_KEY_RETURN;
                //case SAPP_KEYCODE_BACKSPACE: return MU_KEY_BACKSPACE;
            default:
            {
                uint32_t test_key = sappZepKey(ev->key_code);
                if (test_key < 32) {
                    uint32_t key = zi.shift_down ? Zep::ModifierKey::Shift : (zi.ctrl_down ? Zep::ModifierKey::Ctrl : 0);
                    zi.keys.push_back(key | test_key);
                }
                break;
            }
            }

            mu_input_keydown(&state.mu_ctx, key_map(ev->key_code));
            break;

            case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code & 511) {
            case SAPP_KEYCODE_LEFT_SHIFT: zi.shift_down = false; break;
            case SAPP_KEYCODE_RIGHT_SHIFT: zi.shift_down = false; break;
            case SAPP_KEYCODE_LEFT_CONTROL: zi.ctrl_down = false; break;
            case SAPP_KEYCODE_RIGHT_CONTROL: zi.ctrl_down = false; break;
                //case SAPP_KEYCODE_LEFT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_RIGHT_ALT: return MU_KEY_ALT;
                //case SAPP_KEYCODE_ENTER: return MU_KEY_RETURN;
                //case SAPP_KEYCODE_BACKSPACE: return MU_KEY_BACKSPACE;
            }

            mu_input_keyup(&state.mu_ctx, key_map(ev->key_code));
            break;

        case SAPP_EVENTTYPE_CHAR:
            {
                zi.keys.push_back(ev->char_code);

                char txt[2] = { (char)(ev->char_code & 255), 0 };
                mu_input_text(&state.mu_ctx, txt);
            }
            break;

        default:
            break;
    }
}

/* do one frame */
void frame(void) {

    /* pump sokol_fetch message queues */
    sfetch_dowork();

    static bool init_zep = true;
    static std::shared_ptr<Zep::ZepContainer_MeshulaFont> zep;
    if (init_zep && state.font_cousine >= 0)
    {
        std::string startupFilePath;
        std::string configPath;
        int fontPixelHeight = 20;
        zep = std::make_shared<Zep::ZepContainer_MeshulaFont>(startupFilePath, configPath, state.fons, state.font_cousine, fontPixelHeight);
        zep->GetEditor().SetGlobalMode(Zep::ZepMode_Vim::StaticName());
        zep->GetEditor().GetTheme().SetThemeType(Zep::ThemeType::Dark);

        /* setup microui renderer */
        r_init(state.fons, state.font_cousine, fontPixelHeight);

        /* setup microui */
        mu_init(&state.mu_ctx);
        state.mu_ctx.text_width = text_width_cb;
        state.mu_ctx.text_height = text_height_cb;

        init_zep = false;
    }

    if (init_zep)
        return;
    
    /* text rendering via fontstash.h */
    float sx, sy, dx, dy, lh = 0.0f;
    const float dpis = state.dpi_scale;
    sx = 50*dpis; sy = 50*dpis;
    dx = sx; dy = sy;

    uint32_t white = sfons_rgba(255, 255, 255, 255);
    uint32_t black = sfons_rgba(0, 0, 0, 255);
    uint32_t brown = sfons_rgba(192, 128, 0, 128);
    uint32_t blue  = sfons_rgba(0, 192, 255, 255);
    fonsClearState(state.fons);

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, sapp_width(), sapp_height(), 0.0f, -1.0f, +1.0f);
    sgl_scissor_rect(0, 0, sapp_width(), sapp_height(), true);

    FONScontext* fs = state.fons;
    if (state.font_normal != FONS_INVALID) {
        fonsSetFont(fs, state.font_normal);
        fonsSetSize(fs, 124.0f*dpis);
        fonsVertMetrics(fs, NULL, NULL, &lh);
        dx = sx;
        dy += lh;
        fonsSetColor(fs, white);
        dx = fonsDrawText(fs, dx, dy, "The quick ", NULL);
    }
    if (state.font_italic != FONS_INVALID) {
        fonsSetFont(fs, state.font_italic);
        fonsSetSize(fs, 48.0f*dpis);
        fonsSetColor(fs, brown);
        dx = fonsDrawText(fs, dx, dy, "brown ", NULL);
    }
    if (state.font_normal != FONS_INVALID) {
        fonsSetFont(fs, state.font_normal);
        fonsSetSize(fs, 24.0f*dpis);
        fonsSetColor(fs, white);
        dx = fonsDrawText(fs, dx, dy,"fox ", NULL);
    }
    if ((state.font_normal != FONS_INVALID) && (state.font_italic != FONS_INVALID) && (state.font_bold != FONS_INVALID)) {
        fonsVertMetrics(fs, NULL, NULL, &lh);
        dx = sx;
        dy += lh*1.2f;
        fonsSetFont(fs, state.font_italic);
        dx = fonsDrawText(fs, dx, dy, "jumps over ",NULL);
        fonsSetFont(fs, state.font_bold);
        dx = fonsDrawText(fs, dx, dy, "the lazy ",NULL);
        fonsSetFont(fs, state.font_normal);
        dx = fonsDrawText(fs, dx, dy, "dog.",NULL);
    }
    if (state.font_normal != FONS_INVALID) {
        dx = sx;
        dy += lh*1.2f;
        fonsSetSize(fs, 12.0f*dpis);
        fonsSetFont(fs, state.font_normal);
        fonsSetColor(fs, blue);
        fonsDrawText(fs, dx,dy,"Now is the time for all good men to come to the aid of the party.",NULL);
    }
    if (state.font_italic != FONS_INVALID) {
        fonsVertMetrics(fs, NULL, NULL, &lh);
        dx = sx;
        dy += lh*1.2f*2;
        fonsSetSize(fs, 18.0f*dpis);
        fonsSetFont(fs, state.font_italic);
        fonsSetColor(fs, white);
        fonsDrawText(fs, dx, dy, "Ég get etið gler án þess að meiða mig.", NULL);
    }
    if (state.font_japanese != FONS_INVALID) {
        fonsVertMetrics(fs, NULL,NULL,&lh);
        dx = sx;
        dy += lh*1.2f;
        fonsSetFont(fs, state.font_japanese);
        fonsDrawText(fs, dx,dy,"いろはにほへと ちりぬるを わかよたれそ つねならむ うゐのおくやま けふこえて あさきゆめみし ゑひもせす　京（ん）",NULL);
    }

    /* Font alignment */
    if (state.font_normal != FONS_INVALID) {
        fonsSetSize(fs, 18.0f*dpis);
        fonsSetFont(fs, state.font_normal);
        fonsSetColor(fs, white);
        dx = 50*dpis; dy = 350*dpis;
        line(dx-10*dpis,dy,dx+250*dpis,dy);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        dx = fonsDrawText(fs, dx,dy,"Top",NULL);
        dx += 10*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_MIDDLE);
        dx = fonsDrawText(fs, dx,dy,"Middle",NULL);
        dx += 10*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
        dx = fonsDrawText(fs, dx,dy,"Baseline",NULL);
        dx += 10*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);
        fonsDrawText(fs, dx,dy,"Bottom",NULL);
        dx = 150*dpis; dy = 400*dpis;
        line(dx,dy-30*dpis,dx,dy+80.0f*dpis);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
        fonsDrawText(fs, dx,dy,"Left",NULL);
        dy += 30*dpis;
        fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_BASELINE);
        fonsDrawText(fs, dx,dy,"Center",NULL);
        dy += 30*dpis;
        fonsSetAlign(fs, FONS_ALIGN_RIGHT | FONS_ALIGN_BASELINE);
        fonsDrawText(fs, dx,dy,"Right",NULL);
    }

    /* Blur */
    if (state.font_italic != FONS_INVALID) {
        dx = 500*dpis; dy = 350*dpis;
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
        fonsSetSize(fs, 60.0f*dpis);
        fonsSetFont(fs, state.font_italic);
        fonsSetColor(fs, white);
        fonsSetSpacing(fs, 5.0f*dpis);
        fonsSetBlur(fs, 10.0f);
        fonsDrawText(fs, dx,dy,"Blurry...",NULL);
    }

    if (state.font_bold != FONS_INVALID) {
        dy += 50.0f*dpis;
        fonsSetSize(fs, 18.0f*dpis);
        fonsSetFont(fs, state.font_bold);
        fonsSetColor(fs, black);
        fonsSetSpacing(fs, 0.0f);
        fonsSetBlur(fs, 3.0f);
        fonsDrawText(fs, dx,dy+2,"DROP THAT SHADOW",NULL);
        fonsSetColor(fs, white);
        fonsSetBlur(fs, 0);
        fonsDrawText(fs, dx,dy,"DROP THAT SHADOW",NULL);
    }
    
    
    //---- zep
    
#if 0
    // if there was nothing else going on, this could be used to skip
    // rendering - it would only be true when the cursor blink needs updating
    if (!zep.spEditor->RefreshRequired())
    {
        continue;
    }
#endif

    if (zep)
    {
        sgl_push_scissor_rect();
        zep->spEditor->SetDisplayRegion(Zep::NVec2f(100,100), Zep::NVec2f(500,500));

        // Display the editor inside this window
        zep->spEditor->Display();
        const Zep::ZepBuffer& buffer = zep->spEditor->GetActiveTabWindow()->GetActiveWindow()->GetBuffer();

        zep->spEditor->HandleMouseInput(buffer, zi.mouse_delta_x, zi.mouse_delta_y,
            zi.mouse_pos_x, zi.mouse_pos_y, zi.lmb_clicked, zi.lmb_released,
            zi.rmb_clicked, zi.rmb_released);

        while (!zi.keys.empty()) {
            uint32_t key = zi.keys.front();
            zi.keys.pop_front();

            printf("Key: %d %c\n", key, key & 0xff);

            zep->spEditor->HandleKeyInput(buffer, 
                key & 0xff, 
                key & (Zep::ModifierKey::Ctrl << 8),
                key & (Zep::ModifierKey::Shift << 8));
        }

        sgl_pop_scissor_rect();
    }


    //---- UI layer
    if (true) {
        /* UI definition */
        mu_begin(&state.mu_ctx);
        //test_window(&state.mu_ctx);
        log_window(&state.mu_ctx);
        //style_window(&state.mu_ctx);
        mu_end(&state.mu_ctx);
    
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
            }
        }
        r_end();
    }

    /* flush fontstash's font atlas to sokol-gfx texture */
    sfons_flush(fs);

    sg_pass_action pass_action;
    memset(&pass_action, 0, sizeof(pass_action));
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].value = { state.bg.r / 255.0f, state.bg.g / 255.0f, state.bg.b / 255.0f, 1.0f };


    /* render the sokol-gfx default pass */
    sg_begin_default_pass(&pass_action,
        sapp_width(), sapp_height());

    // draw all the sgl stuff
    sgl_draw();

    sg_end_pass();
    sg_commit();

#if 0
    this is how to open a file
    if (ImGui::MenuItem("Open"))
    {
        auto openFileName = tinyfd_openFileDialog(
            "Choose a file",
            "",
            0,
            nullptr,
            nullptr,
            0);
        if (openFileName != nullptr)
        {
            auto pBuffer = zep.GetEditor().GetFileBuffer(openFileName);
            zep.GetEditor().GetActiveTabWindow()->GetActiveWindow()->SetBuffer(pBuffer);
        }
    }
    ImGui::EndMenu();

#endif
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
    desc.window_title = "LabSound GraphToy";
    desc.ios_keyboard_resizes_canvas = false;

    return desc;
}
