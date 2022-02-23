
#include "../LabSokol.h"
#include "../LabMicroUI.h"
#include "../LabZep.h"
#include "LabDirectories.h"
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
static void init() 
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

    add_quit_menu();

    const char* dir = lab_application_resource_path(g_app_path.c_str(), 
                "share/lab_font_demo");
    if (!dir) {
        printf("Could not find share/lab_font_demo\n");
        exit(0);
    }
    static std::string asset_root(dir);

    static std::string r18_path = asset_root + "/hauer-12.png";// "/robot-18.png";
    static LabFont* font_robot18 = LabFontLoad("robot-18", r18_path.c_str(), LabFontType{ LabFontTypeQuadplay });

    int fontPixelHeight = 18;
    static LabFontState* microui_st = LabFontStateBake(font_robot18,
        (float)fontPixelHeight, { {255, 255, 255, 255} },
        LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);

    state.mu_ctx = lab_microui_init(microui_st);

    // Zep
    fontPixelHeight = 18;
    //static LabFontState* zep_st = LabFontStateBake(state.font_cousine, (float) fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
    static LabFontState* zep_st = LabFontStateBake(font_robot18, (float)fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
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
    g_app_path = std::string(argv[0]);

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
