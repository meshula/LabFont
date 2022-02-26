
#include "../LabMicroUI.h"
#include "../LabZep.h"
#include "LabDirectories.h"
#include "microui_demo.h"

#include "../LabFont.h"

#include <stddef.h>

#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#define MTLFONTSTASH_IMPLEMENTATION
#include "mtlfontstash.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <queue>
#include <string>

extern "C" void add_quit_menu();

void fontDemo(float& dx, float& dy, float sx, float sy);


unsigned int packRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (r) | (g << 8) | (b << 16) | (a << 24);
}

void line(FONScontext *stash, float sx, float sy, float ex, float ey) {
    mtlfonsDrawLine(stash, sx, sy, ex, ey);//, packRGBA(0, 0, 0, 255));
}

void dash(FONScontext *stash, float dx, float dy) {
    line(stash, dx-5, dy, dx-10, dy);
}

std::string g_app_path;


@interface MetalView : NSView
@end

@interface MetalView () {
    FONScontext *fs;
    int fontNormal;
    int fontItalic;
    int fontBold;
    int fontJapanese;
}
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, assign) MTLPixelFormat colorPixelFormat;
@end




@implementation MetalView

- (instancetype)initWithFrame:(NSRect)frameRect {
    if (self = [super initWithFrame:frameRect]) {
        [self commonMetalViewInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if (self = [super initWithCoder:coder]) {
        [self commonMetalViewInit];
    }
    return self;
}

- (void)dealloc {
    mtlfonsDelete(fs);
    [super dealloc];
}

- (CALayer *)makeBackingLayer {
    return [CAMetalLayer layer];
}

- (CAMetalLayer *)metalLayer {
    return (CAMetalLayer *)self.layer;
}

- (void)commonMetalViewInit {
    self.device = MTLCreateSystemDefaultDevice();
    self.commandQueue = [self.device newCommandQueue];
    self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.layer = [self makeBackingLayer];

    fs = mtlfonsCreate(self.device, 1024, 512, FONS_ZERO_TOPLEFT);
    if (fs == NULL) {
        printf("Could not create stash.\n");
    }

    std::string rsrc = lab_application_resource_path(g_app_path.c_str(),
                "share/lab_font_demo");
    
    FILE* ffile;
    size_t sz;
    uint8_t* fdata;
    ffile = fopen((rsrc + "/DroidSerif-Regular.ttf").c_str(), "rb");
    fseek(ffile, 0, SEEK_END);
    sz = ftell(ffile);
    fseek(ffile, 0, SEEK_SET);
    fdata = (uint8_t*) malloc(sz);
    fread(fdata, sz, 1, ffile);
    fclose(ffile);
    fontNormal = fonsAddFontMem(fs, "sans", fdata, sz, 1);
    if (fontNormal == FONS_INVALID) {
        printf("Could not add font normal.\n");
    }
    
    ffile = fopen((rsrc + "/DroidSerif-Italic.ttf").c_str(), "rb");
    fseek(ffile, 0, SEEK_END);
    sz = ftell(ffile);
    fseek(ffile, 0, SEEK_SET);
    fdata = (uint8_t*) malloc(sz);
    fread(fdata, sz, 1, ffile);
    fclose(ffile);
    fontItalic = fonsAddFontMem(fs, "sans-italic", fdata, sz, 1);
    if (fontItalic == FONS_INVALID) {
        printf("Could not add font italic.\n");
    }
    
    ffile = fopen((rsrc + "/DroidSerif-Bold.ttf").c_str(), "rb");
    fseek(ffile, 0, SEEK_END);
    sz = ftell(ffile);
    fseek(ffile, 0, SEEK_SET);
    fdata = (uint8_t*) malloc(sz);
    fread(fdata, sz, 1, ffile);
    fclose(ffile);
    fontBold = fonsAddFontMem(fs, "sans-bold", fdata, sz, 1);
    if (fontBold == FONS_INVALID) {
        printf("Could not add font bold.\n");
    }

    ffile = fopen((rsrc + "/DroidSansJapanese.ttf").c_str(), "rb");
    fseek(ffile, 0, SEEK_END);
    sz = ftell(ffile);
    fseek(ffile, 0, SEEK_SET);
    fdata = (uint8_t*) malloc(sz);
    fread(fdata, sz, 1, ffile);
    fclose(ffile);
    fontJapanese = fonsAddFontMem(fs, "sans-jp", fdata, sz, 1);
    if (fontJapanese == FONS_INVALID) {
        printf("Could not add font japanese.\n");
    }

    mtlfonsSetRenderTargetPixelFormat(fs, self.colorPixelFormat);
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    [self setNeedsDisplay:YES];
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self setNeedsDisplay:YES];
}

- (void)setNeedsDisplay:(BOOL)needsDisplay {
    [super setNeedsDisplay:needsDisplay];
    if (needsDisplay && self.window != nil) {
        if (self.metalLayer.device == nil) {
            self.metalLayer.device = self.device;
            self.metalLayer.pixelFormat = self.colorPixelFormat;
        }
        CGFloat scale = fmax(1, self.window.backingScaleFactor);
        CGSize boundsSize = self.bounds.size;
        CGSize drawableSize = CGSizeMake(boundsSize.width * scale, boundsSize.height * scale);
        self.metalLayer.drawableSize = drawableSize;

        [self drawRect:self.bounds];
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    id<CAMetalDrawable> drawable = [self.metalLayer nextDrawable];
    if (drawable == nil) {
        return;
    }

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.3, 0.3, 0.32, 1.0);
    pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].texture = drawable.texture;

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> renderCommandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];

    CGSize drawableSize = self.metalLayer.drawableSize;
    int width = drawableSize.width, height = drawableSize.height;

    MTLViewport viewport = {
        .originX = 0, .originY = 0,
        .height = static_cast<double>(height), .width = static_cast<double>(width) };
    mtlfonsSetRenderCommandEncoder(fs, renderCommandEncoder, viewport);

    unsigned int white = mtlfonsRGBA(255,255,255,255);
    unsigned int brown = mtlfonsRGBA(192,128,0,128);
    unsigned int blue = mtlfonsRGBA(0,192,255,255);
    unsigned int black = mtlfonsRGBA(0,0,0,255);

    float sx, sy, dx, dy, lh = 0;
    sx = 50; sy = 50;
    dx = sx; dy = sy;

    dash(fs, dx,dy);

    fonsClearState(fs);

    float scale = fmax(1, self.window.backingScaleFactor);

    fonsSetSize(fs, scale * 124.0f);
    fonsSetFont(fs, fontNormal);
    fonsVertMetrics(fs, NULL, NULL, &lh);
    dx = sx;
    dy += lh;
    dash(fs, dx,dy);

    fonsSetSize(fs, scale * 124.0f);
    fonsSetFont(fs, fontNormal);
    fonsSetColor(fs, white);
    dx = fonsDrawText(fs, dx,dy,"The quick ",NULL);

    fonsSetSize(fs, scale * 48.0f);
    fonsSetFont(fs, fontItalic);
    fonsSetColor(fs, brown);
    dx = fonsDrawText(fs, dx,dy,"brown ",NULL);

    fonsSetSize(fs, scale * 24.0f);
    fonsSetFont(fs, fontNormal);
    fonsSetColor(fs, white);
    dx = fonsDrawText(fs, dx,dy,"fox ",NULL);

    fonsVertMetrics(fs, NULL, NULL, &lh);
    dx = sx;
    dy += lh*1.2f;
    dash(fs, dx,dy);
    fonsSetFont(fs, fontItalic);
    dx = fonsDrawText(fs, dx,dy,"jumps over ",NULL);
    fonsSetFont(fs, fontBold);
    dx = fonsDrawText(fs, dx,dy,"the lazy ",NULL);
    fonsSetFont(fs, fontNormal);
    dx = fonsDrawText(fs, dx,dy,"dog.",NULL);

    dx = sx;
    dy += lh*1.2f;
    dash(fs, dx,dy);
    fonsSetSize(fs, scale * 12.0f);
    fonsSetFont(fs, fontNormal);
    fonsSetColor(fs, blue);
    fonsDrawText(fs, dx,dy,"Now is the time for all good men to come to the aid of the party.",NULL);

    fonsVertMetrics(fs, NULL,NULL,&lh);
    dx = sx;
    dy += lh*1.2f*2;
    dash(fs, dx,dy);
    fonsSetSize(fs, scale * 18.0f);
    fonsSetFont(fs, fontItalic);
    fonsSetColor(fs, white);
    fonsDrawText(fs, dx,dy,"Ég get etið gler án þess að meiða mig.",NULL);

    fonsVertMetrics(fs, NULL,NULL,&lh);
    dx = sx;
    dy += lh*1.2f;
    dash(fs, dx,dy);
    fonsSetFont(fs, fontJapanese);
    fonsDrawText(fs, dx,dy,"私はガラスを食べられます。それは私を傷つけません。",NULL);

    // Font alignment
    fonsSetSize(fs, scale * 18.0f);
    fonsSetFont(fs, fontNormal);
    fonsSetColor(fs, white);

    dx = scale * 50; dy = scale * 350;
    line(fs, dx-10,dy,dx+scale * 250,dy);
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    dx = fonsDrawText(fs, dx,dy,"Top",NULL);
    dx += scale * 10;
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_MIDDLE);
    dx = fonsDrawText(fs, dx,dy,"Middle",NULL);
    dx += scale * 10;
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
    dx = fonsDrawText(fs, dx,dy,"Baseline",NULL);
    dx += scale * 10;
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);
    fonsDrawText(fs, dx,dy,"Bottom",NULL);

    dx = scale * 150; dy = scale * 400;
    line(fs, dx,dy-scale * 30,dx,dy+scale * 80.0f);
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
    fonsDrawText(fs, dx,dy,"Left",NULL);
    dy += scale * 30;
    fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_BASELINE);
    fonsDrawText(fs, dx,dy,"Center",NULL);
    dy += scale * 30;
    fonsSetAlign(fs, FONS_ALIGN_RIGHT | FONS_ALIGN_BASELINE);
    fonsDrawText(fs, dx,dy,"Right",NULL);

    // Blur
    dx = scale * 500; dy = scale * 350;
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);

    fonsSetSize(fs, scale * 60.0f);
    fonsSetFont(fs, fontItalic);
    fonsSetColor(fs, white);
    fonsSetSpacing(fs, scale * 5.0f);
    fonsSetBlur(fs, scale * 10.0f);
    fonsDrawText(fs, dx,dy,"Blurry...",NULL);

    dy += scale * 50.0f;

    fonsSetSize(fs, scale * 18.0f);
    fonsSetFont(fs, fontBold);
    fonsSetColor(fs, black);
    fonsSetSpacing(fs, 0.0f);
    fonsSetBlur(fs, scale * 3.0f);
    fonsDrawText(fs, dx,dy+(scale * 2),"DROP THAT SHADOW",NULL);

    fonsSetColor(fs, white);
    fonsSetBlur(fs, 0);
    fonsDrawText(fs, dx,dy,"DROP THAT SHADOW",NULL);

    [renderCommandEncoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

@end




@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    id appName = [[NSProcessInfo processInfo] processName];
    NSMenu* menu_bar = [[NSMenu alloc] init];
    NSMenuItem* app_menu_item = [[NSMenuItem alloc] init];
    [menu_bar addItem:app_menu_item];
    NSApp.mainMenu = menu_bar;
    NSMenu* app_menu = [[NSMenu alloc] init];
    NSString* window_title_as_nsstring = [[NSProcessInfo processInfo] processName];
    // `quit_title` memory will be owned by the NSMenuItem, so no need to release it ourselves
    NSString* quit_title =  [@"Quit " stringByAppendingString:window_title_as_nsstring];
    NSMenuItem* quit_menu_item = [[NSMenuItem alloc]
        initWithTitle:quit_title
        action:@selector(terminate:)
        keyEquivalent:@"q"];
    [app_menu addItem:quit_menu_item];
    app_menu_item.submenu = app_menu;
    
    // create a window
    NSRect win_sz = NSMakeRect(0, 0, 1024, 768);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:win_sz
                                            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
    [window setTitle:appName];
    [window makeKeyAndOrderFront:nil];
    
    id view = [[MetalView alloc] initWithFrame:win_sz];
    [[window contentView] addSubview:view];
}

@end


























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

#if 0
/* initialization */
static void init() 
{
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
#endif


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

#if 0
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
#endif
}

int main(int argc, const char * argv[]) {
    
    g_app_path = lab_application_executable_path(argv[0]);
    
    // ensure app can be brought to the front
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSApplication * application = [NSApplication sharedApplication];
    AppDelegate * appDelegate = [[AppDelegate alloc] init];
    [application setDelegate:appDelegate];
    [application run];
    return EXIT_SUCCESS;
}
