// License: MIT
#ifndef LABIMMDRAW_H
#define LABIMMDRAW_H

#ifdef __cplusplus
#define LAB_EXTERNC extern "C"
#else
#define LAB_EXTERNC
#endif

typedef enum {
    labprim_lines,
    labprim_quads,
    labprim_tristrip,
    labprim_linestrip,
    labprim_triangles,
} LabPrim;

typedef struct {
    float* data_pos;
    float* data_st;
    float* data_rgba;
    int stride;

    int sz;
    int count;

    LabPrim prim;
    bool interleaved;
    float s, t, r, g, b, a;
} LabImmContext;

LAB_EXTERNC size_t lab_imm_size_bytes(int vert_count);
LAB_EXTERNC void lab_imm_begin(LabImmContext*, int sz, LabPrim prim, bool interleaved, float* data);
LAB_EXTERNC void lab_imm_end(LabImmContext*);

LAB_EXTERNC void lab_imm_batch_v2f(LabImmContext*, int count, float* v2f);
LAB_EXTERNC void lab_imm_batch_v2f_st(LabImmContext*, int count, float* v2f, float* st);
LAB_EXTERNC void lab_imm_batch_v2f_st_rgba(LabImmContext*, int count, float* v2f, float* st, float* rgba);

LAB_EXTERNC void lab_imm_v2f(LabImmContext*, float x, float y);
LAB_EXTERNC void lab_imm_v2f_st(LabImmContext*, float x, float y, float s, float t);
LAB_EXTERNC void lab_imm_v2f_st_rgba(LabImmContext*, float x, float y, float s, float t, float r, float g, float b, float a);

#endif

#ifdef LABIMMDRAW_IMPL

#include <string.h>

LAB_EXTERNC size_t lab_imm_size_bytes(int vert_count) {
    return sizeof(float) * vert_count * (3 + 2 + 4);
}

LAB_EXTERNC void lab_imm_begin(LabImmContext* lc, int sz, LabPrim prim, bool interleaved, float* data) {
    memset(lc, 0, sizeof(LabImmContext));
    lc->sz = sz;
    lc->prim = prim;
    lc->interleaved = interleaved;
    lc->r = 1.f;
    lc->g = 1.f;
    lc->b = 1.f;
    lc->a = 1.f;
    lc->data_pos = data;
    if (interleaved) {
        lc->stride = 8;
        lc->data_st = data + 2;
        lc->data_rgba = data + (3 + 2);
    }
    else {
        lc->stride = 0;
        lc->data_st = data + sz * 3;
        lc->data_rgba = data + sz * (3 + 2); 
    }
}

LAB_EXTERNC void lab_imm_end(LabImmContext*) {
}

LAB_EXTERNC void lab_imm_batch_v2f(LabImmContext* lc, int count, float* v2f) {
    if (!lc->interleaved) {
        float* start = lc->data_pos + sizeof(float) * 2 * lc->count;
        memcpy(start, v2f, sizeof(float) * 2 * count);
        start = lc->data_st + sizeof(float) * 2 * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = lc->s;
            *start++ = lc->t;
        }
        start = lc->data_rgba + sizeof(float) * 4 * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = lc->r;
            *start++ = lc->g;
            *start++ = lc->b;
            *start++ = lc->a;
        }
    }
    else {
        float* start_pos = lc->data_pos + lc->count;
        float* start_st = lc->data_st + lc->count;
        float* start_rgba = lc->data_rgba + lc->count;
        for (int i = 0; i < count; ++i) {
            start_pos[0] = *v2f++;
            start_pos[1] = *v2f++;
            start_st[0] = lc->s;
            start_st[1] = lc->t;
            start_rgba[0] = lc->r;
            start_rgba[1] = lc->g;
            start_rgba[2] = lc->b;
            start_rgba[3] = lc->a;
            start_pos += lc->stride;
            start_st += lc->stride;
            start_rgba += lc->stride;
        }
    }
    lc->count += count;
}

LAB_EXTERNC void lab_imm_batch_v2f_st(LabImmContext* lc, int count, float* v2f, float* st) {
    if (!lc->interleaved) {
        float* start = lc->data_pos + sizeof(float) * 2 * lc->count;
        memcpy(start, v2f, sizeof(float) * 2 * count);
        start = lc->data_st + sizeof(float) * 2 * lc->count;
        memcpy(start, st, sizeof(float) * 2 * count);
        start = lc->data_rgba + sizeof(float) * 4 * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = lc->r;
            *start++ = lc->g;
            *start++ = lc->b;
            *start++ = lc->a;
        }
    }
    else {
        float* start = lc->data_pos + lc->stride * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = *v2f++;
            *start++ = *v2f++;
            *start++ = *st++;
            *start++ = *st++;
            *start++ = lc->r;
            *start++ = lc->g;
            *start++ = lc->b;
            *start++ = lc->a;            
        }
    }
    lc->count += count;
}

LAB_EXTERNC void lab_imm_batch_v2f_st_rgba(LabImmContext* lc, int count, float* v2f, float* st, float* rgba) {
    if (!lc->interleaved) {
        float* start = lc->data_pos + lc->count;
        memcpy(start, v2f, sizeof(float) * 2 * count);
        start = lc->data_st + sizeof(float) * 2 * lc->count;
        memcpy(start, st, sizeof(float) * 2 * count);
        start = lc->data_rgba + sizeof(float) * 4 * lc->count;
        memcpy(start, rgba, sizeof(float) * 4 * count);
    }
    else {
        float* start = lc->data_pos + lc->stride * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = *v2f++;
            *start++ = *v2f++;
            *start++ = *st++;
            *start++ = *st++;
            *start++ = *rgba++;
            *start++ = *rgba++;
            *start++ = *rgba++;
            *start++ = *rgba++;
        }
    }
    lc->count += count;
}

LAB_EXTERNC void lab_imm_v2f(LabImmContext* lc, float x, float y) {
    if (!lc->interleaved) {
        float* start = lc->data_pos + lc->count;
        start[0] = x;
        start[1] = y;
        start = lc->data_st + sizeof(float) * 2 * lc->count;
        start[0] = lc->s;
        start[1] = lc->t;
        start = lc->data_rgba + sizeof(float) * 4 * lc->count;
        start[0] = lc->r;
        start[1] = lc->g;
        start[2] = lc->b;
        start[3] = lc->a;
    }
    else {
        float* start = lc->data_pos + lc->stride * lc->count;
        *start++ = x;
        *start++ = y;
        *start++ = lc->s;
        *start++ = lc->t;
        *start++ = lc->r;
        *start++ = lc->g;
        *start++ = lc->b;
        *start++ = lc->a;
    }
    ++lc->count;
}

LAB_EXTERNC void lab_imm_v2f_st(LabImmContext* lc, float x, float y, float s, float t) {
    lc->s = s;
    lc->t = t;
    lab_imm_v2f(lc, x, y);
}

LAB_EXTERNC void lab_imm_v2f_st_rgba(LabImmContext* lc, float x, float y, float s, float t, float r, float g, float b, float a) {
    lc->s = s;
    lc->t = t;
    lc->r = r;
    lc->g = g;
    lc->b = b;
    lc->a = a;
    lab_imm_v2f(lc, x, y);
}

#endif

#ifdef LAB_EXTERNC
#undef LAB_EXTERNC
#endif

