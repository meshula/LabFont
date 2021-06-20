
#ifndef LABZEP_H
#define LABZEP_H

#ifdef __cplusplus
#define LZ_EXTERNC extern "C"
#else
#define LZ_EXTERNC
#endif

struct LabFontState;
struct LabZep;
LZ_EXTERNC LabZep* LabZep_create(LabFontState*, const char* filename, const char* text);
LZ_EXTERNC void LabZep_free(LabZep*);
LZ_EXTERNC void LabZep_input_sokol_key(LabZep*, int key, bool shift, bool ctrl);
LZ_EXTERNC void LabZep_render(LabZep*);
LZ_EXTERNC void LabZep_position_editor(LabZep*, int x, int y, int w, int h);
LZ_EXTERNC void LabZep_process_events(LabZep*,
    float mouse_x, float mouse_y,
    bool lmb_clicked, bool lmb_released, bool rmb_clicked, bool rmb_released);
LZ_EXTERNC bool LabZep_update_required(LabZep*);
LZ_EXTERNC void LabZep_open(LabZep*, const char* filename);

#undef LZ_EXTERNC
#endif
