
#include "../LabMicroUI.h"
#include "../LabZep.h"
#include "LabDirectories.h"
#include "microui_demo.h"

#include "../LabFont.h"

#include <stddef.h>

#define VK_USE_PLATFORM_WIN32_KHR

#include "vkh.h"
#include "vkh_app.h"
#include "vkh_phyinfo.h"

#include "fontstash.h"

#define LABIMMDRAW_IMPL
#include "../LabDrawImmediate.h"
#define LABIMMDRAW_VULKAN_IMPLEMENTATION
#include "../LabDrawImmediate-vulkan.h"

#define LAB_USE_CONSOLE

#include <vulkan/vulkan.h>


#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#if defined(LAB_USE_CONSOLE)
    #pragma comment (linker, "/subsystem:console")
#else
    #pragma comment (linker, "/subsystem:windows")
#endif
#include <stdio.h>  /* freopen_s() */
#include <wchar.h>  /* wcslen() */

#pragma comment (lib, "kernel32")
#pragma comment (lib, "user32")
#pragma comment (lib, "shell32")    /* CommandLineToArgvW, DragQueryFileW, DragFinished */
#pragma comment (lib, "gdi32")

#include <atomic>
#include <queue>
#include <string>
#include <vector>
using std::vector;

void fontDemo(LabFontDrawState* ds, float& dx, float& dy, float sx, float sy);
void font_demo_init(const char* path_);

std::string g_app_path;
static LabFontState* microui_font_bake = nullptr;


const std::string shader = R"R(
#version 330 core
// Sample program only
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

// PROCESS_DPI_AWARENESS, and all structs named _sapp are borrowed from sokol
// which is published under the zlib license 

#ifndef DPI_ENUMS_DECLARED
typedef enum PROCESS_DPI_AWARENESS
{
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif /*DPI_ENUMS_DECLARED*/

typedef struct {
    bool aware;
    float content_scale;
    float window_scale;
    float mouse_scale;
} _sapp_win32_dpi_t;

typedef struct {
    HWND hwnd;
    HMONITOR hmonitor;
    HDC dc;
    HICON big_icon;
    HICON small_icon;
    UINT orig_codepage;
    LONG mouse_locked_x, mouse_locked_y;
    RECT stored_window_rect; // used to restore window pos/size when toggling fullscreen => windowed
    
    float window_width;
    float window_height;
    int framebuffer_width;
    int framebuffer_height;

    bool is_win10_or_greater;
    bool in_create_window;
    bool quit_ordered;
    bool quit_requested;
    bool full_screen;
    bool iconified;
    bool resized;
    bool mouse_tracked;
    uint8_t mouse_capture_mask;
    _sapp_win32_dpi_t dpi;
    bool raw_input_mousepos_valid;
    LONG raw_input_mousepos_x;
    LONG raw_input_mousepos_y;
    uint8_t raw_input_data[256];

    VkhApp vkh_app;
    VkhPresenter vkh_presenter;
    VkhDevice vkh_dev;
} _sapp_win32_t;
_sapp_win32_t app;

static void _sapp_win32_init_dpi(bool set_hidpi) {

    DECLARE_HANDLE(DPI_AWARENESS_CONTEXT_T);
    typedef BOOL(WINAPI * SETPROCESSDPIAWARE_T)(void);
    typedef bool (WINAPI * SETPROCESSDPIAWARENESSCONTEXT_T)(DPI_AWARENESS_CONTEXT_T); // since Windows 10, version 1703
    typedef HRESULT(WINAPI * SETPROCESSDPIAWARENESS_T)(PROCESS_DPI_AWARENESS);
    typedef HRESULT(WINAPI * GETDPIFORMONITOR_T)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

    SETPROCESSDPIAWARE_T fn_setprocessdpiaware = 0;
    SETPROCESSDPIAWARENESS_T fn_setprocessdpiawareness = 0;
    GETDPIFORMONITOR_T fn_getdpiformonitor = 0;
    SETPROCESSDPIAWARENESSCONTEXT_T fn_setprocessdpiawarenesscontext =0;

    HINSTANCE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        fn_setprocessdpiaware = (SETPROCESSDPIAWARE_T)(void*) GetProcAddress(user32, "SetProcessDPIAware");
        fn_setprocessdpiawarenesscontext = (SETPROCESSDPIAWARENESSCONTEXT_T)(void*) GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    }
    HINSTANCE shcore = LoadLibraryA("shcore.dll");
    if (shcore) {
        fn_setprocessdpiawareness = (SETPROCESSDPIAWARENESS_T)(void*) GetProcAddress(shcore, "SetProcessDpiAwareness");
        fn_getdpiformonitor = (GETDPIFORMONITOR_T)(void*) GetProcAddress(shcore, "GetDpiForMonitor");
    }
    /*
        NOTE on SetProcessDpiAware() vs SetProcessDpiAwareness() vs SetProcessDpiAwarenessContext():

        These are different attempts to get DPI handling on Windows right, from oldest
        to newest. SetProcessDpiAwarenessContext() is required for the new
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 method.
    */
    if (fn_setprocessdpiawareness) {
        if (set_hidpi) {
            /* app requests HighDPI rendering, first try the Win10 Creator Update per-monitor-dpi awareness,
               if that fails, fall back to system-dpi-awareness
            */
            app.dpi.aware = true;
            DPI_AWARENESS_CONTEXT_T per_monitor_aware_v2 = (DPI_AWARENESS_CONTEXT_T)-4;
            if (!(fn_setprocessdpiawarenesscontext && fn_setprocessdpiawarenesscontext(per_monitor_aware_v2))) {
                // fallback to system-dpi-aware
                fn_setprocessdpiawareness(PROCESS_SYSTEM_DPI_AWARE);
            }
        }
        else {
            /* if the app didn't request HighDPI rendering, let Windows do the upscaling */
            app.dpi.aware = false;
            fn_setprocessdpiawareness(PROCESS_DPI_UNAWARE);
        }
    }
    else if (fn_setprocessdpiaware) {
        // fallback for Windows 7
        app.dpi.aware = true;
        fn_setprocessdpiaware();
    }
    /* get dpi scale factor for main monitor */
    if (fn_getdpiformonitor && app.dpi.aware) {
        POINT pt = { 1, 1 };
        HMONITOR hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        UINT dpix, dpiy;
        HRESULT hr = fn_getdpiformonitor(hm, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
        /* clamp window scale to an integer factor */
        app.dpi.window_scale = (float)dpix / 96.0f;
    }
    else {
        app.dpi.window_scale = 1.0f;
    }
    if (set_hidpi) {
        app.dpi.content_scale = app.dpi.window_scale;
        app.dpi.mouse_scale = 1.0f;
    }
    else {
        app.dpi.content_scale = 1.0f;
        app.dpi.mouse_scale = 1.0f / app.dpi.window_scale;
    }
    //app.dpi_scale = app.dpi.content_scale;
    if (user32) {
        FreeLibrary(user32);
    }
    if (shcore) {
        FreeLibrary(shcore);
    }
}

/* updates current window and framebuffer size from the window's client rect, returns true if size has changed */
static bool _sapp_win32_update_dimensions(void) {
    RECT rect;
    if (GetClientRect(app.hwnd, &rect)) {
        app.window_width = (int)((float)(rect.right - rect.left) / app.dpi.window_scale);
        app.window_height = (int)((float)(rect.bottom - rect.top) / app.dpi.window_scale);
        int fb_width = (int)((float)app.window_width * app.dpi.content_scale);
        int fb_height = (int)((float)app.window_height * app.dpi.content_scale);
        /* prevent a framebuffer size of 0 when window is minimized */
        if (0 == fb_width) {
            fb_width = 1;
        }
        if (0 == fb_height) {
            fb_height = 1;
        }
        if ((fb_width != app.framebuffer_width) || (fb_height != app.framebuffer_height)) {
            app.framebuffer_width = fb_width;
            app.framebuffer_height = fb_height;
            return true;
        }
    }
    else {
        app.window_width = app.window_height = 1;
        app.framebuffer_width = app.framebuffer_height = 1;
    }
    return false;
}

static LRESULT CALLBACK 
_sapp_win32_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!app.in_create_window) {
        switch (uMsg) {
            case WM_CLOSE:
                /* only give user a chance to intervene when sapp_quit() wasn't already called */
                if (!app.quit_ordered) {
                    /* if window should be closed and event handling is enabled, give user code
                        a change to intervene via sapp_cancel_quit()
                    */
                    app.quit_requested = true;
                    //_sapp_win32_uwp_app_event(SAPP_EVENTTYPE_QUIT_REQUESTED);
                    /* if user code hasn't intervened, quit the app */
                    if (app.quit_requested) {
                        app.quit_ordered = true;
                    }
                }
                if (app.quit_ordered) {
                    PostQuitMessage(0);
                }
                return 0;
            case WM_SYSCOMMAND:
                switch (wParam & 0xFFF0) {
                    case SC_SCREENSAVE:
                    case SC_MONITORPOWER:
                        if (app.full_screen) {
                            /* disable screen saver and blanking in fullscreen mode */
                            return 0;
                        }
                        break;
                    case SC_KEYMENU:
                        /* user trying to access menu via ALT */
                        return 0;
                }
                break;
            case WM_ERASEBKGND:
                return 1;
            case WM_SIZE:
                {
                    const bool iconified = wParam == SIZE_MINIMIZED;
                    if (iconified != app.iconified) {
                        app.iconified = iconified;
                        if (iconified) {
                            //_sapp_win32_uwp_app_event(SAPP_EVENTTYPE_ICONIFIED);
                        }
                        else {
                           // _sapp_win32_uwp_app_event(SAPP_EVENTTYPE_RESTORED);
                        }
                    }
                    if (!iconified) {
                        app.resized = true;
                    }
                }
                break;
            case WM_SETFOCUS:
                //_sapp_win32_uwp_app_event(SAPP_EVENTTYPE_FOCUSED);
                break;
            case WM_KILLFOCUS:
                /* if focus is lost for any reason, and we're in mouse locked mode, disable mouse lock */
                //if (app.mouse.locked) {
                    //_sapp_win32_lock_mouse(false);
                //}
                //_sapp_win32_uwp_app_event(SAPP_EVENTTYPE_UNFOCUSED);
                break;
            case WM_SETCURSOR:
                //if (_sapp.desc.user_cursor) {
                //    if (LOWORD(lParam) == HTCLIENT) {
                //        _sapp_win32_uwp_app_event(SAPP_EVENTTYPE_UPDATE_CURSOR);
                //        return 1;
                //    }
               // }
                break;
            case WM_DPICHANGED:
            {
                /* Update window's DPI and size if its moved to another monitor with a different DPI
                   Only sent if DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is used.
                */
                //_sapp_win32_dpi_changed(hWnd, (LPRECT)lParam);
                break;
            }
            case WM_LBUTTONDOWN:
                //_sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_DOWN, SAPP_MOUSEBUTTON_LEFT);
                //_sapp_win32_capture_mouse(1<<SAPP_MOUSEBUTTON_LEFT);
                break;
            case WM_RBUTTONDOWN:
                //_sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_DOWN, SAPP_MOUSEBUTTON_RIGHT);
                //_sapp_win32_capture_mouse(1<<SAPP_MOUSEBUTTON_RIGHT);
                break;
            case WM_MBUTTONDOWN:
                //_sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_DOWN, SAPP_MOUSEBUTTON_MIDDLE);
                //_sapp_win32_capture_mouse(1<<SAPP_MOUSEBUTTON_MIDDLE);
                break;
            case WM_LBUTTONUP:
                //_sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_UP, SAPP_MOUSEBUTTON_LEFT);
                //_sapp_win32_release_mouse(1<<SAPP_MOUSEBUTTON_LEFT);
                break;
            case WM_RBUTTONUP:
                //_sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_UP, SAPP_MOUSEBUTTON_RIGHT);
                //_sapp_win32_release_mouse(1<<SAPP_MOUSEBUTTON_RIGHT);
                break;
            case WM_MBUTTONUP:
                //_sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_UP, SAPP_MOUSEBUTTON_MIDDLE);
                //_sapp_win32_release_mouse(1<<SAPP_MOUSEBUTTON_MIDDLE);
                break;
            case WM_MOUSEMOVE:
#if 0
                if (!_sapp.mouse.locked) {
                    const float new_x  = (float)GET_X_LPARAM(lParam) * _sapp.win32.dpi.mouse_scale;
                    const float new_y = (float)GET_Y_LPARAM(lParam) * _sapp.win32.dpi.mouse_scale;
                    /* don't update dx/dy in the very first event */
                    if (_sapp.mouse.pos_valid) {
                        _sapp.mouse.dx = new_x - _sapp.mouse.x;
                        _sapp.mouse.dy = new_y - _sapp.mouse.y;
                    }
                    _sapp.mouse.x = new_x;
                    _sapp.mouse.y = new_y;
                    _sapp.mouse.pos_valid = true;
                    if (!_sapp.win32.mouse_tracked) {
                        _sapp.win32.mouse_tracked = true;
                        TRACKMOUSEEVENT tme;
                        memset(&tme, 0, sizeof(tme));
                        tme.cbSize = sizeof(tme);
                        tme.dwFlags = TME_LEAVE;
                        tme.hwndTrack = _sapp.win32.hwnd;
                        TrackMouseEvent(&tme);
                        _sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_ENTER, SAPP_MOUSEBUTTON_INVALID);
                    }
                    _sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_MOVE, SAPP_MOUSEBUTTON_INVALID);
                }
#endif
                break;
            case WM_INPUT:
#if 0
                /* raw mouse input during mouse-lock */
                if (_sapp.mouse.locked) {
                    HRAWINPUT ri = (HRAWINPUT) lParam;
                    UINT size = sizeof(_sapp.win32.raw_input_data);
                    // see: https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata
                    if ((UINT)-1 == GetRawInputData(ri, RID_INPUT, &_sapp.win32.raw_input_data, &size, sizeof(RAWINPUTHEADER))) {
                        SOKOL_LOG("GetRawInputData() failed\n");
                        break;
                    }
                    const RAWINPUT* raw_mouse_data = (const RAWINPUT*) &_sapp.win32.raw_input_data;
                    if (raw_mouse_data->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
                        /* mouse only reports absolute position
                           NOTE: THIS IS UNTESTED, it's unclear from reading the
                           Win32 RawInput docs under which circumstances absolute
                           positions are sent.
                        */
                        if (_sapp.win32.raw_input_mousepos_valid) {
                            LONG new_x = raw_mouse_data->data.mouse.lLastX;
                            LONG new_y = raw_mouse_data->data.mouse.lLastY;
                            _sapp.mouse.dx = (float) (new_x - _sapp.win32.raw_input_mousepos_x);
                            _sapp.mouse.dy = (float) (new_y - _sapp.win32.raw_input_mousepos_y);
                            _sapp.win32.raw_input_mousepos_x = new_x;
                            _sapp.win32.raw_input_mousepos_y = new_y;
                            _sapp.win32.raw_input_mousepos_valid = true;
                        }
                    }
                    else {
                        /* mouse reports movement delta (this seems to be the common case) */
                        _sapp.mouse.dx = (float) raw_mouse_data->data.mouse.lLastX;
                        _sapp.mouse.dy = (float) raw_mouse_data->data.mouse.lLastY;
                    }
                    _sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_MOVE, SAPP_MOUSEBUTTON_INVALID);
                }
#endif
                break;

            case WM_MOUSELEAVE:
#if 0
                if (!_sapp.mouse.locked) {
                    _sapp.win32.mouse_tracked = false;
                    _sapp_win32_mouse_event(SAPP_EVENTTYPE_MOUSE_LEAVE, SAPP_MOUSEBUTTON_INVALID);
                }
#endif
                break;
            case WM_MOUSEWHEEL:
                //_sapp_win32_scroll_event(0.0f, (float)((SHORT)HIWORD(wParam)));
                break;
            case WM_MOUSEHWHEEL:
                //_sapp_win32_scroll_event((float)((SHORT)HIWORD(wParam)), 0.0f);
                break;
            case WM_CHAR:
                //_sapp_win32_char_event((uint32_t)wParam, !!(lParam&0x40000000));
                break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                //_sapp_win32_key_event(SAPP_EVENTTYPE_KEY_DOWN, (int)(HIWORD(lParam)&0x1FF), !!(lParam&0x40000000));
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                //_sapp_win32_key_event(SAPP_EVENTTYPE_KEY_UP, (int)(HIWORD(lParam)&0x1FF), false);
                break;
            case WM_ENTERSIZEMOVE:
                //SetTimer(_sapp.win32.hwnd, 1, USER_TIMER_MINIMUM, NULL);
                break;
            case WM_EXITSIZEMOVE:
                //KillTimer(_sapp.win32.hwnd, 1);
                break;
            case WM_TIMER:
#if 0
                _sapp_win32_timing_measure();
                _sapp_frame();
                #if defined(SOKOL_D3D11)
                    // present with DXGI_PRESENT_DO_NOT_WAIT
                    _sapp_d3d11_present(true);
                #endif
                #if defined(SOKOL_GLCORE33)
                    _sapp_wgl_swap_buffers();
                #endif
                /* NOTE: resizing the swap-chain during resize leads to a substantial
                   memory spike (hundreds of megabytes for a few seconds).

                if (_sapp_win32_update_dimensions()) {
                    #if defined(SOKOL_D3D11)
                    _sapp_d3d11_resize_default_render_target();
                    #endif
                    _sapp_win32_uwp_app_event(SAPP_EVENTTYPE_RESIZED);
                }
                */
#endif
                break;
            case WM_NCLBUTTONDOWN:
#if 0
                /* workaround for half-second pause when starting to move window
                    see: https://gamedev.net/forums/topic/672094-keeping-things-moving-during-win32-moveresize-events/5254386/
                */
                if (SendMessage(_sapp.win32.hwnd, WM_NCHITTEST, wParam, lParam) == HTCAPTION) {
                    POINT point;
                    GetCursorPos(&point);
                    ScreenToClient(_sapp.win32.hwnd, &point);
                    PostMessage(_sapp.win32.hwnd, WM_MOUSEMOVE, 0, ((uint32_t)point.x)|(((uint32_t)point.y) << 16));
                }
                break;
#endif
            case WM_DROPFILES:
                //_sapp_win32_files_dropped((HDROP)wParam);
                break;
            case WM_DISPLAYCHANGE:
                // refresh rate might have changed
                //_sapp_timing_reset(&_sapp.timing);
                break;

            default:
                break;
        }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int main(int argc, const char * argv[]) {
    // app set up
    g_app_path = lab_application_executable_path(argv[0]);

    // console set up
    bool created_console = AllocConsole(); // AttachConsole(ATTACH_PARENT_PROCESS);
    if (created_console) {
        FILE* pf = nullptr;
        errno_t err = freopen_s(&pf, "CON", "w", stdout);
        err = freopen_s(&pf, "CON", "w", stderr);
    }

    app.orig_codepage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    // discover dpi scaling factors
    printf("App path: %s\n", g_app_path.c_str()); 
    _sapp_win32_init_dpi(true);
    printf("dpi: %f\n", app.dpi.content_scale);

    // create window
    WNDCLASSW wndclassw;
    memset(&wndclassw, 0, sizeof(wndclassw));
    wndclassw.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wndclassw.lpfnWndProc = (WNDPROC) _sapp_win32_wndproc;
    wndclassw.hInstance = GetModuleHandleW(NULL);
    wndclassw.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclassw.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wndclassw.lpszClassName = L"LABFONTDEMO_APP";
    RegisterClassW(&wndclassw);
 
    app.in_create_window = true;
    /* NOTE: regardless whether fullscreen is requested or not, a regular
       windowed-mode window will always be created first (however in hidden
       mode, so that no windowed-mode window pops up before the fullscreen window)
    */
    const DWORD win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    RECT rect = { 0, 0, 0, 0 };
    DWORD win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;


    app.window_width = 1024;
    app.window_height = 768;


    rect.right = (int) (app.window_width * app.dpi.window_scale);
    rect.bottom = (int) (app.window_height * app.dpi.window_scale);
    const bool use_default_width = 0 == app.window_width;
    const bool use_default_height = 0 == app.window_height;
    AdjustWindowRectEx(&rect, win_style, FALSE, win_ex_style);
    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;
    app.in_create_window = true;
    app.hwnd = CreateWindowExW(
        win_ex_style,               // dwExStyle
        L"LABFONTDEMO_APP",         // lpClassName
        L"LabFont Demo",            // lpWindowName
        win_style,                  // dwStyle
        CW_USEDEFAULT,              // X
        SW_HIDE,                    // Y (NOTE: CW_USEDEFAULT is not used for position here, but internally calls ShowWindow!
        use_default_width ? CW_USEDEFAULT : win_width, // nWidth
        use_default_height ? CW_USEDEFAULT : win_height, // nHeight (NOTE: if width is CW_USEDEFAULT, height is actually ignored)
        NULL,                       // hWndParent
        NULL,                       // hMenu
        GetModuleHandle(NULL),      // hInstance
        NULL);                      // lParam
    app.in_create_window = false;
    app.dc = GetDC(app.hwnd);
    app.hmonitor = MonitorFromWindow(app.hwnd, MONITOR_DEFAULTTONULL);

    _sapp_win32_update_dimensions();
    if (app.full_screen) {
        //_sapp_win32_set_fullscreen(_sapp.fullscreen, SWP_HIDEWINDOW);
        _sapp_win32_update_dimensions();
    }
    printf("Window (%d, %d), fb (%d, %d)\n", (int)app.window_width,
            (int)app.window_height, app.framebuffer_width, app.framebuffer_height);


    //-- Vulkan set up

    const char* enabledLayers[32];
    const char* enabledExts[32];
    uint32_t enabledExtsCount = 0, enabledLayersCount = 0;

    vkh_layers_check_init();
#ifdef VKVG_USE_VALIDATION
    if (vkh_layer_is_present("VK_LAYER_KHRONOS_validation"))
        enabledLayers[enabledLayersCount++] = "VK_LAYER_KHRONOS_validation";
#endif
#ifdef VKVG_USE_MESA_OVERLAY
    if (vkh_layer_is_present("VK_LAYER_MESA_overlay"))
        enabledLayers[enabledLayersCount++] = "VK_LAYER_MESA_overlay";
#endif

#ifdef VKVG_USE_RENDERDOC
    if (vkh_layer_is_present("VK_LAYER_RENDERDOC_Capture"))
        enabledLayers[enabledLayersCount++] = "VK_LAYER_RENDERDOC_Capture";
#endif
    vkh_layers_check_release();

#if defined(_WIN32)
    enabledLayers[enabledLayersCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__ANDROID__)
    enabledLayers[enabledLayersCount++] = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
#elif defined(__linux__)
    enabledLayers[enabledLayersCount++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#endif

    enabledExts[enabledExtsCount++] = "VK_EXT_debug_utils";

    app.vkh_app = vkh_app_create(1, 2, "LabFont Demo", enabledLayersCount, enabledLayers, enabledExtsCount, enabledExts);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
    vkh_app_enable_debug_messenger(e->app
        , VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        //| VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        //| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        , VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        //| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
        //| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        , NULL);
#endif

    // Create Vulkan surface
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo;
    memset(&surfaceCreateInfo, 0, sizeof(surfaceCreateInfo));
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
    surfaceCreateInfo.hwnd = app.hwnd;

    VkSurfaceKHR surf;
    VkResult result =
        vkCreateWin32SurfaceKHR(app.vkh_app->inst, &surfaceCreateInfo, NULL, &surf);

    if (result != VK_SUCCESS) {}

    // VkhPhyInfo** stores common useful physical device informations, queues flags, 
    // memory properties in a single call for all the devices present on the machine.
    uint32_t phyCount = 0;
    VkhPhyInfo * phys = vkh_app_get_phyinfos(app.vkh_app, &phyCount, surf);

    // sample how to fetch the properties:
    for (uint32_t i = 0; i < phyCount; i++) {
        //check VkPhysicalDeviceProperties
        VkPhysicalDeviceProperties pdp = vkh_phyinfo_get_properties(phys[i]);
        //get VkPhysicalDeviceMemoryProperties
        VkPhysicalDeviceMemoryProperties mp = vkh_phyinfo_get_memory_properties(phys[i]);
        //get queue properties
        uint32_t qCount;
        VkQueueFamilyProperties* queues_props = vkh_phyinfo_get_queues_props(phys[i], &qCount);
    }

    float qPriorities[] = { 0.f };
    VkDeviceQueueCreateInfo pQueueInfos[3];
    memset(pQueueInfos, 0, sizeof(pQueueInfos));
    int presentable_queue = -1;
    int compute_queue = -1;
    int transfer_queue = -1;
    int qCount = 0;
    if (vkh_phyinfo_create_presentable_queues(*phys, 1, qPriorities, &pQueueInfos[qCount])) {
        presentable_queue = qCount;
        qCount++;
    }
    if (vkh_phyinfo_create_compute_queues(*phys, 1, qPriorities, &pQueueInfos[qCount])) {
        compute_queue = qCount;
        qCount++;
    }
    if (vkh_phyinfo_create_transfer_queues(*phys, 1, qPriorities, &pQueueInfos[qCount])) {
        transfer_queue = qCount;
        qCount++;
    }

    // create logical device
    char const* dex[] = { "VK_KHR_swapchain" };
    VkPhysicalDeviceFeatures enabledFeatures;
    memset(&enabledFeatures, 0, sizeof(enabledFeatures));
    enabledFeatures.fillModeNonSolid = true;
    VkDeviceCreateInfo device_info;
    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = qCount;
    device_info.pQueueCreateInfos = (VkDeviceQueueCreateInfo*)&pQueueInfos;
    device_info.enabledExtensionCount = enabledExtsCount;
    device_info.ppEnabledExtensionNames = dex;
    device_info.pEnabledFeatures = &enabledFeatures;
    app.vkh_dev = vkh_device_create(app.vkh_app, *phys, &device_info);

    // phys will has no more use after initialization of the queues.
    vkh_app_free_phyinfos(phyCount, phys);

    app.vkh_presenter = vkh_presenter_create(app.vkh_dev, presentable_queue, surf,
        app.window_width, app.window_height, VK_FORMAT_B8G8R8A8_UNORM, VK_PRESENT_MODE_MAILBOX_KHR);
    
    // create a blitting command buffer per swapchain images with
  //  vkh_presenter_build_blit_cmd(app.vkh_presenter, 
  //      vkvg_surface_get_vk_image(surf), app.window_width, app.window_height);



#if 0
    memset(&app.vk_appInfo, 0, sizeof(app.vk_appInfo));
    app.vk_appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.vk_appInfo.pApplicationName = "LabFontDemo";
    app.vk_appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.vk_appInfo.pEngineName = "LabCart";
    app.vk_appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app.vk_appInfo.apiVersion = VK_API_VERSION_1_0;
    std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

#if defined(_WIN32)
    enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
    enabledExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &app.vk_appInfo;
    instanceCreateInfo.enabledExtensionCount = enabledExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    // the second nullptr is to use the default allocator
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &app.vk_instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        printf(
            "Cannot find a compatible Vulkan installable client "
            "driver (ICD). Please make sure your driver supports "
            "Vulkan before continuing. The call to vkCreateInstance failed.");
        return EXIT_FAILURE;
    }
    else if (result != VK_SUCCESS) {
        printf(
            "The call to vkCreateInstance failed. Please make sure "
            "you have a Vulkan installable client driver (ICD) before "
            "continuing.");
        return EXIT_FAILURE;
    }
#endif

    // show the window
    ShowWindow(app.hwnd, SW_SHOW);
    DragAcceptFiles(app.hwnd, 1);

    // Run main loop
    bool done = false;
    while (!(done || app.quit_ordered)) {
        //_sapp_win32_timing_measure();
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (WM_QUIT == msg.message) {
                done = true;
                continue;
            }
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        //_sapp_frame();
        if (app.resized) {
            _sapp_win32_update_dimensions();
        }
        // if (_sapp_win32_update_monitor()) {} // deal with window moved to another monitor
        if (app.quit_requested) {
            PostMessage(app.hwnd, WM_CLOSE, 0, 0);
        }
    }

    vkh_presenter_destroy(app.vkh_presenter);
    vkDestroySurfaceKHR(app.vkh_app->inst, surf, NULL);

    vkh_device_destroy(app.vkh_dev);

    // destroy window
    DestroyWindow(app.hwnd);
    app.hwnd = 0;
    UnregisterClassW(L"LABFONTDEMO_APP", GetModuleHandleW(NULL));

    vkh_app_destroy(app.vkh_app);

    // console tear down
    SetConsoleOutputCP(app.orig_codepage);
    return EXIT_SUCCESS;
}













#if 0


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
    LabImmDrawContext* imm_ctx;
    std::atomic<int> _lock;
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

@interface LabFontDemoView : NSView
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
    self.layer = [self makeBackingLayer];
    self.layer.magnificationFilter = kCAFilterNearest;
    self.translatesAutoresizingMaskIntoConstraints = false;

    [self updateTrackingAreas];
    //self.preferredFramesPerSecond = 60;
    //self.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    //self.sampleCount = 1;
    //self.autoResizeDrawable = false;

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
    [self drawRect:self.bounds];
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
    int len = tsConvertUtf16ToUtf8(dest, 16, (const char16_t*) buffer);
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
        uint8_t c = translateKey(event);
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

    state.mu_ctx = lab_microui_init(&lic_imu, microui_st);

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

    LabFontDrawState* ds = LabFontDrawBegin((void*) command_encoder);
    
    if (true) {
        fontDemo(ds, dx, dy, sx, sy);
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

    LabFontDrawEnd(ds);

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
#endif

