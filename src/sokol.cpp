

#ifdef HAVE_IMGUI
# include "imgui.h"
//#define SOKOL_GLCORE33 --- needs to be defined globally
//#define HAVE_SOKOL_DBGUI

# ifndef HAVE_SOKOL_DBGUI
# define SOKOL_IMGUI_IMPL
# define SOKOL_GFX_IMGUI_IMPL
# endif
#endif

#define SOKOL_GL_IMPL
#define SOKOL_IMPL
#define SOKOL_TRACE_HOOKS
//#define SOKOL_WIN32_FORCE_MAIN

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_time.h"
#include "sokol_gl.h"
#ifdef HAVE_IMGUI
# include "sokol_imgui.h"
# include "sokol_gfx_imgui.h"
# ifdef HAVE_SOKOL_DBGUI
#  include "dbgui.h"
# endif
#endif

#define FONTSTASH_IMPLEMENTATION
#if defined(_MSC_VER )
#pragma warning(disable:4996)   // strncpy use in fontstash.h
#pragma warning(disable:4505)   // unreferenced local function has been removed
#pragma warning(disable:4100)   // unreferenced formal parameter
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <stdlib.h>     // malloc/free
#include "fontstash.h"
#define SOKOL_IMPL
#include "sokol_fontstash.h"

#ifdef SOKOL_GL_IMPL_INCLUDED
#endif
