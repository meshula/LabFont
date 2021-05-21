#ifndef MICROUI_RENDERER_H
#define MICROUI_RENDERER_H

#include "microui.h"

struct FONScontext;

extern "C" void r_begin(int disp_width, int disp_height);
extern "C" void r_end(void);
extern "C" void r_draw(void);
extern "C" void r_init(FONScontext* font, int font_index, float font_size);
extern "C" void r_draw_rect(mu_Rect rect, mu_Color color);
extern "C" void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
extern "C" void r_draw_icon(int id, mu_Rect rect, mu_Color color);
extern "C" int r_get_text_width(const char *text, int len);
extern "C" int r_get_text_height(void);
extern "C" void r_set_clip_rect(mu_Rect rect);
//extern "C" void r_clear(mu_Color color);
//extern "C" void r_present();

#endif

