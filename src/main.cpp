
#include "../LabSokol.h"
#include "../LabMicroUI.h"
#include "../LabZep.h"
#include "../LabFontDemo.h"
#include "microui_demo.h"

#include "../LabFont.h"

#include <queue>
#include <string>

#ifdef _MSC_VER
void add_quit_menu() {}
#else
extern "C" void add_quit_menu();
#endif

void fontDemo(float& dx, float& dy, float sx, float sy);


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



typedef struct {
    float r, g, b;
} color_t;

static struct {
    mu_Context* mu_ctx = nullptr;
    char logbuf[64000];
    int logbuf_updated;
    color_t bg = { 90.f, 95.f, 100.f };
    float dpi_scale;

    mu_Id zep_id;

    LabFont* font_japanese = nullptr;
    LabFont* font_normal = nullptr;
    LabFont* font_italic = nullptr;
    LabFont* font_bold = nullptr;
    LabFont* font_cousine = nullptr;
    LabFont* font_robot18 = nullptr;
} state;

static LabZep* zep = nullptr;


static void zep_window(mu_Context* ctx) 
{
    if (mu_begin_window_ex(ctx, "Zep", mu_rect(100, 100, 800, 600), MU_OPT_NOFRAME)) {

        int w1[] = { -1 };

        static int zep_secret = 0x1337babe;
        mu_Id zep_id = mu_get_id(ctx, &zep_secret, sizeof(zep_secret));

        mu_layout_row(ctx, 1, w1, -1);
        mu_Rect rect = mu_layout_next(ctx);
        mu_update_control(ctx, zep_id, rect, 0);

        mu_layout_set_next(ctx, rect, 0);
        LabZep_position_editor(zep, rect.x, rect.y, rect.w, rect.h);

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


static void line(float sx, float sy, float ex, float ey)
{
    sgl_begin_lines();
    sgl_c4b(255, 255, 0, 128);
    sgl_v2f(sx, sy);
    sgl_v2f(ex, ey);
    sgl_end();
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
    static std::string r18_path = std::string(lab_font_demo_asset_base) + "hauer-12.png";// "robot-18.png";
    state.font_robot18 = LabFontLoad("robot-18", r18_path.c_str(), LabFontType{ LabFontTypeQuadplay });
    add_quit_menu();

    int fontPixelHeight = 18;
    static LabFontState* microui_st = LabFontStateBake(state.font_robot18,
        (float)fontPixelHeight, { {255, 255, 255, 255} },
        LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);

    state.mu_ctx = lab_microui_init(microui_st);

    // Zep
    fontPixelHeight = 18;
    //static LabFontState* zep_st = LabFontStateBake(state.font_cousine, (float) fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
    static LabFontState* zep_st = LabFontStateBake(state.font_robot18, (float)fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
    zep = LabZep_create(zep_st, "Shader.frag", shader.c_str());
}

static void event(const sapp_event* ev) {

    // local tracking of modifier keys

    static struct {
        float mouse_pos_x, mouse_pos_y;
        bool lmb_clicked = false;
        bool lmb_released = false;
        bool rmb_clicked = false;
        bool rmb_released = false;
        bool ctrl_l = false;
        bool ctrl_r = false;
        bool shift_l = false;
        bool shift_r = false;
    } zi;

    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.lmb_clicked = true;
            zi.lmb_released = false;
            mu_input_mousedown(state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            zi.lmb_clicked = false;
            zi.lmb_released = true;
            mu_input_mouseup(state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
            break;

        case SAPP_EVENTTYPE_MOUSE_MOVE:
            zi.mouse_pos_x = ev->mouse_x;
            zi.mouse_pos_y = ev->mouse_y;
            mu_input_mousemove(state.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y);
            break;

        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            mu_input_scroll(state.mu_ctx, 0, (int)ev->scroll_y);
            break;

        case SAPP_EVENTTYPE_KEY_DOWN:
            switch (ev->key_code & 511) {
            case SAPP_KEYCODE_LEFT_SHIFT: zi.shift_l = true; break;
            case SAPP_KEYCODE_RIGHT_SHIFT: zi.shift_r = true; break;
            case SAPP_KEYCODE_LEFT_CONTROL: zi.ctrl_l = true; break;
            case SAPP_KEYCODE_RIGHT_CONTROL: zi.ctrl_r = true; break;
            default:
            {
                if (state.mu_ctx->focus == state.zep_id && ev->key_code >= SAPP_KEYCODE_ESCAPE) {
                    LabZep_input_sokol_key(zep, ev->key_code, zi.shift_l || zi.shift_r, zi.ctrl_l || zi.ctrl_r);
                }
                break;
            }
            }
            lab_microui_input_sokol_keydown(ev->key_code);
            break;

            case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code & 511) {
            case SAPP_KEYCODE_LEFT_SHIFT: zi.shift_l = false; break;
            case SAPP_KEYCODE_RIGHT_SHIFT: zi.shift_r = false; break;
            case SAPP_KEYCODE_LEFT_CONTROL: zi.ctrl_l = false; break;
            case SAPP_KEYCODE_RIGHT_CONTROL: zi.ctrl_r = false; break;
            }
            lab_microui_input_sokol_keydown(ev->key_code);
            break;

        case SAPP_EVENTTYPE_CHAR:
            if (state.mu_ctx->focus == state.zep_id) {
                LabZep_input_sokol_key(zep, ev->char_code, zi.shift_l || zi.shift_r, zi.ctrl_l || zi.ctrl_r);
            }
            else {
                lab_microui_input_sokol_text(ev->char_code);
            }
            break;

        default:
            break;
    }
    LabZep_process_events(zep,
        zi.mouse_pos_x, zi.mouse_pos_y, zi.lmb_clicked, zi.lmb_released,
        zi.rmb_clicked, zi.rmb_released);
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
    sz = 18.f * dpis;
    static LabFontState* p2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignTop }, 0, 0);
    dx = 350 * dpis; 
    dy = 450 * dpis;
    line(dx - 10 * dpis, dy, dx + 250 * dpis, dy);
    dx = LabFontDraw("Top", dx, dy, p2_st);
    static LabFontState* g2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignMiddle }, 0, 0);
    dx += 10 * dpis;
    dx = LabFontDraw("Middle", dx, dy, g2_st);
    dx += 10 * dpis;
    static LabFontState* q2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBaseline }, 0, 0);
    dx = LabFontDraw("Baseline", dx, dy, q2_st);
    dx += 10 * dpis;
    static LabFontState* h2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignBottom }, 0, 0);
    LabFontDraw("Bottom", dx, dy, h2_st);
    dx = 450 * dpis; 
    dy = 500 * dpis;
    line(dx, dy - 30 * dpis, dx, dy + 80.0f * dpis);
    static LabFontState* i2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignBaseline }, 0, 0);
    static LabFontState* k2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignCenter | LabFontAlignBaseline }, 0, 0);
    static LabFontState* l2_st = LabFontStateBake(state.font_robot18, sz, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignRight | LabFontAlignBaseline }, 0, 0);
    LabFontDraw("Left", dx, dy, i2_st);
    dy += 30 * dpis;
    LabFontDraw("Center", dx, dy, k2_st);
    dy += 30 * dpis;
    LabFontDraw("Right", dx, dy, l2_st);
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

extern sg_image debug_texture;

/* do one frame */
void frame(void) {

    //-------------------------------------------------------------------------
    // Pre-sgl render pass

    //-------------------------------------------------------------------------
    // sgl render pass

    /* text rendering via fontstash.h */
    float sx, sy, dx, dy, lh = 0.0f;
    const float dpis = state.dpi_scale;
    sx = 50*dpis; sy = 50*dpis;
    dx = sx; dy = sy;


    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, (float) sapp_width(), (float) sapp_height(), 0.0f, -1.0f, +1.0f);
    sgl_scissor_rect(0, 0, sapp_width(), sapp_height(), true);

    if (false) {
        sgl_enable_texture();
        sgl_texture(debug_texture);
        sgl_begin_quads();
        float x0 = 100; float x1 = 800;
        float y0 = 100; float y1 = 600;
        float u0 = 0; float u1 = 1;
        float v0 = 0; float v1 = 1;
        sgl_c4b(255, 255, 255, 255);
        sgl_v2f_t2f(x0, y0, u0, v0);
        sgl_v2f_t2f(x1, y0, u1, v0);
        sgl_v2f_t2f(x1, y1, u1, v1);
        sgl_v2f_t2f(x0, y1, u0, v1);
        sgl_end();
    }

    if (true) {
        fontDemo(dx, dy, sx, sy);
    }

    //---- sgl UI layer
    if (true) {
        /* UI definition */
        mu_begin(state.mu_ctx);
        microui_test_window(state.mu_ctx);
        log_window(state.mu_ctx);
        zep_window(state.mu_ctx);
        microui_style_window(state.mu_ctx);
        mu_end(state.mu_ctx);

        lab_microui_render(sapp_width(), sapp_height(), zep);
    }

    LabFontCommitTexture();

    // begin sokol GL rendering with a clear pass
    sg_pass_action pass_action;
    memset(&pass_action, 0, sizeof(pass_action));
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].value = { state.bg.r / 255.0f, state.bg.g / 255.0f, state.bg.b / 255.0f, 1.0f };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());

    // draw all the sgl stuff
    sgl_draw();

    sg_end_pass();

    //-------------------------------------------------------------------------
    // Post-sgl render pass goes here

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
