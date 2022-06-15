
#include "../LabMicroUI.h"
#include "../LabZep.h"
#include "LabDirectories.h"
#include "microui_demo.h"

#include "../LabFont.h"

#include <stddef.h>

#include "fontstash.h"
//#include "mtlfontstash.h"

#define LABIMMDRAW_IMPL
#include "../LabDrawImmediate.h"
#define LABIMMDRAW_METAL_IMPLEMENTATION
#include "../LabDrawImmediate-metal.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <IOKit/hid/IOHIDLib.h>

#include <atomic>
#include <queue>
#include <string>
#include <vector>
using std::vector;

extern "C" void add_quit_menu();

void fontDemo(LabFontDrawState* ds, float& dx, float& dy, float sx, float sy);
void font_demo_init(const char* path_);

std::string g_app_path;
static LabFontState* microui_font_bake = nullptr;



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

static LabZep* zep = nullptr;

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

static void zep_window(mu_Context* ctx)
{
    if (mu_begin_window_ex(ctx, "Zep", mu_rect(100, 100, 800, 600), MU_OPT_NOFRAME)) {

        int w1[] = { -1 };

        static int zep_secret = 0x1337babe;
        state.zep_id = mu_get_id(ctx, &zep_secret, sizeof(zep_secret));

        mu_layout_row(ctx, 1, w1, -1);
        mu_Rect rect = mu_layout_next(ctx);
        mu_update_control(ctx, state.zep_id, rect, MU_OPT_HOLDFOCUS);

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

@interface LabFontDemoRenderer : NSObject
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, assign) MTLPixelFormat colorPixelFormat;
@end

@implementation LabFontDemoRenderer {
    FONScontext* fs;
    int fontNormal;
    int fontItalic;
    int fontBold;
    int fontJapanese;
    LabImmPlatformContext* imm_ctx;
    std::atomic<int> _lock;
}

-(void)dealloc {
    self.device = nil;
    self.commandQueue = nil;
    [super dealloc];
}

- (instancetype)init {
    if (self = [super init]) {
        self->_lock = 0;
        self.device = MTLCreateSystemDefaultDevice();
        self.commandQueue = [self.device newCommandQueue];
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;

        // font atlas size 1024x1024
        self->imm_ctx = LabImmPlatformContextCreate(self.device, 1024, 1024);
        LabFontInitMetal(self->imm_ctx, self.colorPixelFormat);

        const char* rsrc_str = lab_application_resource_path(g_app_path.c_str(),
                                          "share/lab_font_demo");
        if (!rsrc_str) {
            printf("resource path not found relative to %s\n", g_app_path.c_str());
            exit(0);
        }
        std::string rsrc(rsrc_str);

        LabImmDrawSetRenderTargetPixelFormat(self->imm_ctx, MTLPixelFormatBGRA8Unorm);
        font_demo_init(rsrc.c_str());
            
        static std::string asset_root(rsrc_str);
        static std::string r18_path = asset_root + "/hauer-12.png";// "/robot-18.png";
        static LabFont* font_robot18 = LabFontLoad("robot-18", r18_path.c_str(), LabFontType{ LabFontTypeQuadplay });
        int fontPixelHeight = 18;
        microui_font_bake = LabFontStateBake(font_robot18,
            (float)fontPixelHeight, { {255, 255, 255, 255} },
            LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);

        // Zep
        fontPixelHeight = 18;
        //static LabFontState* zep_st = LabFontStateBake(state.font_cousine, (float) fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
        static LabFontState* zep_st = LabFontStateBake(font_robot18, (float)fontPixelHeight, { {255, 255, 255, 255} }, LabFontAlign{ LabFontAlignLeft | LabFontAlignTop }, 0.f, 0.f);
        zep = LabZep_create(self->imm_ctx, zep_st, "Shader.frag", shader.c_str());
    }
    return self;
}

-(void)drawRect:(NSRect)dirtyRect drawableSize:(CGSize)rect drawable:(id<CAMetalDrawable>)drawable {
    // reentrancy guard
    {
        int locked = 0;
        if (!self->_lock.compare_exchange_strong(locked, 1))
            return;
    }
    
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.3, 0.3, 0.32, 1.0);
    pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].texture = drawable.texture;

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> renderCommandEncoder =
                    [commandBuffer renderCommandEncoderWithDescriptor:pass];

    int width = rect.width;
    int height = rect.height;
    
    LabFontDrawBeginMetal(renderCommandEncoder);
    lab_imm_MTLRenderCommandEncoder_set(self->imm_ctx, renderCommandEncoder);
    auto ds = LabFontDrawBegin(0, 0, width, height);

    float dx = 0;
    float dy = 50;
    float sx = 150;
    float sy = 150;
    fontDemo(ds, dx, dy, sx, sy);

    lab_imm_viewport_set(self->imm_ctx, 0, 0, width, height);
    size_t buff_size = lab_imm_size_bytes(256);
    static vector<float> buff;
    buff.resize(buff_size);
    LabImmContext lic;
    
    lab_imm_batch_begin(&lic, self->imm_ctx, 257, labprim_linestrip, false, buff.data());
    lab_imm_c4f(&lic, 1.f, 1.f, 0.f, 1.f);
    for (int i = 0; i < 257; ++i) {
        float th = 6.282f * (float) i / 256.f;
        float s = sinf(th);
        float c = cosf(th);
        lab_imm_v2f(&lic, 250 + 150 * s, 250 + 150 * c);
    }
    lab_imm_batch_draw(&lic, 1, true);

    lab_imm_batch_begin(&lic, self->imm_ctx, 257, labprim_triangles, false, buff.data());
    lab_imm_c4f(&lic, 1.f, 1.f, 0.f, 1.f);
    lab_imm_line(&lic, 200,500, 300, 600, 20);
    lab_imm_line(&lic, 200,600, 300, 500, 20);
    lab_imm_batch_draw(&lic, 1, true);     // texture slot 1 ~~ solid color
    
    // microui layer
    if (!state.mu_ctx) {
        state.mu_ctx = lab_microui_init(self->imm_ctx, microui_font_bake);
        state.mu_ctx->style->size.y = 18; // widget height
    }
    if (state.mu_ctx) {
        /* UI definition */
        mu_begin(state.mu_ctx);

        microui_test_window(state.mu_ctx);
        log_window(state.mu_ctx);
        zep_window(state.mu_ctx);
        microui_style_window(state.mu_ctx);
        mu_end(state.mu_ctx);

        lab_microui_render(ds, width, height, zep);
    }
    
    LabFontDrawEnd(ds);
    
    [renderCommandEncoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
    
    self->_lock = 0;
}

@end

/* __     ___                */
/* \ \   / (_) _____      __ */
/*  \ \ / /| |/ _ \ \ /\ / / */
/*   \ V / | |  __/\ V  V /  */
/*    \_/  |_|\___| \_/\_/   */

@interface LabFontDemoView : MTKView
@end


@implementation LabFontDemoView
{
    LabFontDemoRenderer* _renderer;
    bool _command_down;
    bool _shift_down;
    bool _control_down;
    bool _option_down;
}

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
    //mtlfonsDelete(fs);
    [super dealloc];
}

- (CALayer *)makeBackingLayer {
    return [CAMetalLayer layer];
}

- (CAMetalLayer *)metalLayer {
    return (CAMetalLayer *)self.layer;
}

- (void)commonMetalViewInit {
    self->_renderer = [LabFontDemoRenderer new];
    self.device = self->_renderer.device;
    self.layer = [self makeBackingLayer];
    self.layer.magnificationFilter = kCAFilterNearest;
    self.translatesAutoresizingMaskIntoConstraints = false;

    [self updateTrackingAreas];
    self.preferredFramesPerSecond = 60;
    self.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    self.sampleCount = 1;
    self.autoResizeDrawable = false;

    self->_shift_down = false;
    self->_option_down = false;
    self->_control_down = false;
    self->_command_down = false;
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
            self.metalLayer.device = _renderer.device;
            self.metalLayer.pixelFormat = _renderer.colorPixelFormat;
        }
        CGFloat scale = fmax(1, self.window.backingScaleFactor);
        CGSize boundsSize = self.bounds.size;
        CGSize drawableSize = CGSizeMake(boundsSize.width * scale, boundsSize.height * scale);
        self.metalLayer.drawableSize = drawableSize;
    }
    //[self drawRect:self.bounds];
}

- (void)timer_fired:(id)sender {
    [self setNeedsDisplay: YES];
}

- (BOOL)isOpaque { return YES; }
- (BOOL)canBecomeKeyView { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (NSPoint)eventPosToCanvas:(NSEvent*) event {
    NSPoint event_location = [event locationInWindow];
    NSPoint local_point = [self convertPoint:event_location fromView:nil];
    NSRect bounds = [self bounds];
    float h = (float) bounds.size.height;
    float x = (float) local_point.x;
    float y = h - (float) local_point.y;
    float dpi = 2.0f;
    return { x * dpi, y * dpi };
}

- (void)mouseDown:(NSEvent*)event {
    NSPoint pos = [self eventPosToCanvas:event];
    printf("Mouse down %f %f\n", (float) pos.x, (float)pos.y);
    mu_input_mousedown(state.mu_ctx, (int)pos.x, (int)pos.y, 1);
}

- (void)mouseUp:(NSEvent*)event {
    NSPoint pos = [self eventPosToCanvas:event];
    printf("Mouse up %f %f\n", (float) pos.x, (float)pos.y);
    mu_input_mouseup(state.mu_ctx, (int)pos.x, (int)pos.y, 1);
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint pos = [self eventPosToCanvas:event];
    printf("Mouse move %f %f\n", (float) pos.x, (float)pos.y);
    mu_input_mousemove(state.mu_ctx, (int)pos.x, (int)pos.y);
}
- (void)mouseDragged:(NSEvent*)event {
    NSPoint pos = [self eventPosToCanvas:event];
    printf("Mouse drag %f %f\n", (float) pos.x, (float)pos.y);
    mu_input_mousemove(state.mu_ctx, (int)pos.x, (int)pos.y);
}


/*
* The two functions, tsConvertUtf16ToUtf8 and tsConvertUtf8ToUtf16 are
* originally from the Effekseer library, and bear the following license:
The MIT License (MIT)
Copyright (c) 2011 Effekseer Project
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/**
    @brief    Convert UTF16 into UTF8
    @param    dst    a pointer to destination buffer
    @param    dst_size    a length of destination buffer
    @param    src            a source buffer
    @return    length except 0
*/
int32_t tsConvertUtf16ToUtf8(char* dst, int32_t dst_size, const char16_t* src)
{
    int32_t cnt = 0;
    const char16_t* wp = src;
    char* cp = dst;

    if (dst_size == 0)
        return 0;

    dst_size -= 3;

    for (cnt = 0; cnt < dst_size;)
    {
        char16_t wc = *wp++;
        if (wc == 0)
        {
            break;
        }
        if ((wc & ~0x7f) == 0)
        {
            if (cp)
                *cp++ = wc & 0x7f;
            cnt += 1;
        }
        else if ((wc & ~0x7ff) == 0)
        {
            if (cp) {
                *cp++ = ((wc >> 6) & 0x1f) | 0xc0;
                *cp++ = ((wc) & 0x3f) | 0x80;
            }
            cnt += 2;
        }
        else
        {
            if (cp) {
                *cp++ = ((wc >> 12) & 0xf) | 0xe0;
                *cp++ = ((wc >> 6) & 0x3f) | 0x80;
                *cp++ = ((wc) & 0x3f) | 0x80;
            }
            cnt += 3;
        }
    }
    *cp = '\0';
    return cnt;
}


uint8_t translateKey(NSEvent* event) {
    TISInputSourceRef inputSource = TISCopyCurrentKeyboardLayoutInputSource();
    if (!inputSource) {
        printf("There's no keyboard layout??\n");
        return 0;
    }
    NSData *uchr = (NSData*) TISGetInputSourceProperty(inputSource, kTISPropertyUnicodeKeyLayoutData);
    if (!uchr)
    {
        printf("There's no keyboard layout data??\n");
        CFRelease(inputSource);
        return 0;
    }
    UInt32 deadKeys = 0;
    UniChar buffer[8] = {0,0,0,0,0,0,0,0};
    UniCharCount actualStringLength = 0;

    OSStatus err = UCKeyTranslate(
        (const UCKeyboardLayout*) [uchr bytes],
        event.keyCode,
        kUCKeyActionDisplay,
        event.modifierFlags, // modifier flags
        LMGetKbdType(),
        kUCKeyTranslateNoDeadKeysBit,
        &deadKeys,
        8, // length of buffer
        &actualStringLength,
        buffer);
    if (err != noErr) {
        printf("Could not translate key\n");
        return 0;
    }
    
    char dest[16];
    /*int len =*/ tsConvertUtf16ToUtf8(dest, 16, (const char16_t*) buffer);
    return dest[0];
}

- (void)keyDown:(NSEvent*)event {
    unsigned short c = event.keyCode;
    if (c == 51) {
        mu_input_keydown(state.mu_ctx, MU_KEY_BACKSPACE);
    }
    else if (c == 36 || c == 76) {
        mu_input_keydown(state.mu_ctx, MU_KEY_RETURN);
    }
    else {
        // @TODO translateKey can return a unicode string,
        // and mu_input_text can accept it
        uint8_t c[2] = { translateKey(event), 0 };
        if (state.mu_ctx->focus == state.zep_id) {
            LabZep_input_sokol_key(zep, c[0], _shift_down, _control_down);
        }
        else {
            mu_input_text(state.mu_ctx, (const char*)&c);
        }
    }

    if (event.isARepeat)
        printf("Key Repeat\n");
    else
        printf("Key Down\n");
}
- (void)keyUp:(NSEvent*)event {
    unsigned short c = event.keyCode;
    if (c == 51) {
        mu_input_keyup(state.mu_ctx, MU_KEY_BACKSPACE);
    }
    else if (c == 36 || c == 76) {
        mu_input_keyup(state.mu_ctx, MU_KEY_RETURN);
    }
    else {
        /*uint8_t c =*/ translateKey(event);
    }
    printf("Key Up\n");
}

- (void)flagsChanged:(NSEvent*)event {
    bool command_down = event.modifierFlags & NSEventModifierFlagCommand;
    bool option_down =  event.modifierFlags & NSEventModifierFlagOption;
    bool control_down = event.modifierFlags & NSEventModifierFlagControl;
    bool shift_down =   event.modifierFlags & NSEventModifierFlagShift;

    if (command_down != _command_down) {
        _command_down = command_down;
        if (command_down)
            mu_input_keydown(state.mu_ctx, MU_KEY_ALT);
        else
            mu_input_keyup(state.mu_ctx, MU_KEY_ALT);
    }
    option_down = _option_down;     /// @TODO extend microui with MU_KEY_OPTION
    if (control_down != _control_down) {
        _control_down = control_down;
        if (control_down)
            mu_input_keydown(state.mu_ctx, MU_KEY_CTRL);
        else
            mu_input_keyup(state.mu_ctx, MU_KEY_CTRL);
    }
    if (shift_down != _shift_down) {
        _shift_down = shift_down;
        if (shift_down)
            mu_input_keydown(state.mu_ctx, MU_KEY_SHIFT);
        else
            mu_input_keyup(state.mu_ctx, MU_KEY_SHIFT);
    }
    
    printf("flags %x = \n", (uint32_t) event.modifierFlags);
    if (_control_down) {
        printf("  %s control\n", (event.modifierFlags & (1 << 13)) ? "right" : "left" );
    }
    if (_shift_down) {
        printf("  %s shift\n", (event.modifierFlags & 4) ? "right" : "left" );
    }
    if (_option_down) {
        printf("  %s option\n", (event.modifierFlags & 0x40) ? "right" : "left");
    }
    if (_command_down) {
        printf("  %s command\n", (event.modifierFlags & 0x10) ? "right" : "left");
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    id<CAMetalDrawable> drawable = [self.metalLayer nextDrawable];
    if (drawable == nil) {
        return;
    }

    // drawableSize has had the dpi multiplied in already, see above
    CGSize sz = self.metalLayer.drawableSize;
    [_renderer drawRect:dirtyRect drawableSize:sz drawable:drawable];
}

@end

@interface LabWindow : NSWindow
@end

@implementation LabWindow
- (instancetype)initWithContentRect:(NSRect)contentRect 
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag {
    if (self = [super initWithContentRect:contentRect
                                styleMask:style 
                                  backing:backingStoreType 
                                    defer:flag]) {
    }
    return self;
}



@end

/*     _                ____       _                  _        */
/*    / \   _ __  _ __ |  _ \  ___| | ___  __ _  __ _| |_ ___  */
/*   / _ \ | '_ \| '_ \| | | |/ _ \ |/ _ \/ _` |/ _` | __/ _ \ */
/*  / ___ \| |_) | |_) | |_| |  __/ |  __/ (_| | (_| | ||  __/ */
/* /_/   \_\ .__/| .__/|____/ \___|_|\___|\__, |\__,_|\__\___| */
/*         |_|   |_|                        |_/                */

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
    NSString* quit_title =  [@"Quit " stringByAppendingString:window_title_as_nsstring];
    NSMenuItem* quit_menu_item = [[NSMenuItem alloc]
        initWithTitle:quit_title
        action:@selector(terminate:)
        keyEquivalent:@"q"];
    [app_menu addItem:quit_menu_item];
    app_menu_item.submenu = app_menu;

    // create a window

    const NSUInteger style =
        NSWindowStyleMaskTitled |
        NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable;
    
    NSRect win_sz = NSMakeRect(0, 0, 1024, 768);
    NSWindow* window = [[LabWindow alloc] initWithContentRect:win_sz
                                                    styleMask:style
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
    [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
    [window setTitle:appName];
    [window makeKeyAndOrderFront:nil];
    window.releasedWhenClosed = NO; // clean up will occur in applicationWillTerminate
    window.acceptsMouseMovedEvents = YES;
    window.restorable = YES;

    id view = [[LabFontDemoView alloc] initWithFrame:win_sz];
    [[window contentView] addSubview:view];
    [window makeFirstResponder:view];

    NSTimer* frame_timer = [NSTimer timerWithTimeInterval:0.001
        target: view selector: @selector(timer_fired:)
        userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:frame_timer forMode:NSDefaultRunLoopMode];
    frame_timer = nil;
}

@end







#if 0


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


int main(int argc, const char * argv[]) {
    
    g_app_path = lab_application_executable_path(argv[0]);
    
    // ensure app can be brought to the front
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSApplication * application = [NSApplication sharedApplication];
    AppDelegate * appDelegate = [[AppDelegate alloc] init];
    [application setDelegate:appDelegate];

    NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
    [NSApp activateIgnoringOtherApps:YES];
    [NSEvent setMouseCoalescingEnabled:NO];
    [application run];
    [application setDelegate:nil];
    return EXIT_SUCCESS;
}
