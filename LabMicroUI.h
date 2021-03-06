#pragma once

#ifndef LAB_MICROUI_H
#define LAB_MICROUI_H

#include "microui.h"

#ifdef __cplusplus
#define LAB_EXTERNC extern "C"
#else
#define LAB_EXTERNC
#endif

struct LabFontDrawState;
struct LabFontState;
struct LabImmPlatformContext;
struct LabZep;

LAB_EXTERNC mu_Context* lab_microui_init(LabImmPlatformContext* imm_ctx, LabFontState*);
LAB_EXTERNC void lab_microui_render(LabFontDrawState*, int w, int h, LabZep*);
LAB_EXTERNC void lab_microui_input_sokol_keydown(int c);
LAB_EXTERNC void lab_microui_input_sokol_keyup(int c);
LAB_EXTERNC void lab_microui_input_sokol_text(int c);

#undef LAB_EXTERNC

#endif
