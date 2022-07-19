// License: MIT
#ifndef LABIMMDRAW_H
#define LABIMMDRAW_H

#ifdef __cplusplus
#define LAB_EXTERNC extern "C"
#else
#define LAB_EXTERNC
#endif

#define fu_xc

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

#ifdef __clang__
_Pragma("clang assume_nonnull begin")
#endif

// nb: if points are to be supported, the associated vertex shader needs
// to explicitly output [[point_size]] under Metal or the behavior is undefined
// points not supported since there's no data_point_size attribute on the 
// Batch struct at the moment.

// nb: quads are not a hardware primitive. To render quads, as required by a UI
// for example, emit this quad 
// 0 ---- 1
// |      |
// 3 ---- 2 as 0-1-2, 0-2-3.

typedef enum {
    labprim_lines,
    labprim_linestrip,
    labprim_triangles,
    labprim_tristrip,
} LabPrim;

typedef struct { float x, y, z, w; } labimm_v4f;

struct LabImmPlatformContext;
typedef struct LabImmPlatformContext LabImmPlatformContext;

#ifdef __OBJC__
//-----------------------------------------------------------------------------
// context management
LAB_EXTERNC
LabImmPlatformContext* _Nullable
LabImmPlatformContextCreate(
    id<MTLDevice> _Nonnull,
    int font_atlas_width, int font_atlas_height);

LAB_EXTERNC
void
LabImmPlatformContextDestroy(
    LabImmPlatformContext* _Nonnull);

// the default format is BGRA8Unorm, setting it here will also cause the
// render pipeline to be regenerated.
LAB_EXTERNC
void
LabImmDrawSetRenderTargetPixelFormat(
    LabImmPlatformContext* _Nonnull mtl,
    MTLPixelFormat pixelFormat);

LAB_EXTERNC
void
lab_imm_MTLRenderCommandEncoder_set(
    LabImmPlatformContext* _Nonnull mtl,
    id<MTLRenderCommandEncoder> _Nullable);

typedef struct {
    float x, y, w, h;
} LabImmViewport;

LAB_EXTERNC
LabImmViewport lab_imm_Viewport(LabImmPlatformContext* _Nonnull mtl);
LAB_EXTERNC
void lab_imm_SetClipRect(LabImmPlatformContext* _Nonnull mtl, LabImmViewport* vp);

LAB_EXTERNC
id<MTLDevice> lab_imm_device(LabImmPlatformContext* _Nonnull mtl);

LAB_EXTERNC
int lab_imm_assign_texture_slot(LabImmPlatformContext* _Nonnull mtl, id<MTLTexture>);

//-----------------------------------------------------------------------------
#endif

typedef struct FONScontext FONScontext;
LAB_EXTERNC
FONScontext* lab_imm_FONScontext(LabImmPlatformContext* _Nonnull mtl);

/*
    LabImmContext
 
    holds vertex and primitive data for rendering
    holds a pointer to an un-owned associated platform context
 */

typedef struct {
    float* data_pos;
    float* data_st;
    uint32_t* data_rgba;
    int stride;

    int sz;
    int count;

    LabPrim prim;
    bool interleaved;
    float s, t;
    uint32_t rgba;
    
    LabImmPlatformContext* platform;
} LabImmContext;

LAB_EXTERNC size_t lab_imm_size_bytes(int vert_count);

/*
    lab_imm_batch_begin
 
    Set up an immediate batch on the specified context
    The data block is un-owned.
 */
LAB_EXTERNC void lab_imm_batch_begin(
                    LabImmContext*,
                    LabImmPlatformContext*,
                    int sz, LabPrim prim, bool interleaved, float* data);

void lab_imm_batch_draw(
                    LabImmContext*,
                    int texture_slot, bool sample_nearest);

LAB_EXTERNC void lab_imm_batch_v2f(LabImmContext*, 
                    int count, float* v2f);
LAB_EXTERNC void lab_imm_batch_v2f_st(LabImmContext*, 
                    int count, float* v2f, float* st);
LAB_EXTERNC void lab_imm_batch_v2f_st_rgba(LabImmContext*, 
                    int count, float* v2f, float* st, uint32_t* rgba);

LAB_EXTERNC void lab_imm_v2f(LabImmContext*,
                    float x, float y);
LAB_EXTERNC void lab_imm_v2f_st(LabImmContext*, 
                    float x, float y, float s, float t);
LAB_EXTERNC void lab_imm_v2f_st_rgba(LabImmContext*,
                    float x, float y, float s, float t, 
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a);

LAB_EXTERNC void lab_imm_c4f(LabImmContext*,
                    float r, float g, float b, float a);

LAB_EXTERNC void lab_imm_line(LabImmContext* lc,
                    float x0, float y0, float x1, float y1, float w);

void
lab_imm_viewport_set(
    LabImmPlatformContext* _Nonnull mtl,
    float originX, float originY, float w, float h);

//-----------------------------------------------------------------------------
// drawing

void lab_imm_draw_arrays(
    LabImmPlatformContext* _Nonnull,
    int texture_slot, bool sample_nearest,
    LabPrim,
    const float* _Nonnull verts,
    const float* _Nonnull tcoords,
    const unsigned int* _Nonnull colors,
    int nverts);

//-----------------------------------------------------------------------------
// font/sprite atlas texture management
// If an atlas has been previously created, calling this will replace it
int
lab_imm_create_atlas(
    LabImmPlatformContext* _Nonnull,
    int texture_slot, int width, int height);

// given an in memory copy of the atlas, stored at data, Update will mark the
// region from srcx, srcy to srcx+w, srcy+h as needing a GPU refresh
void
lab_imm_update_atlas(
    LabImmPlatformContext* _Nonnull,
    int texture_slot, int srcx, int srcy, int w, int h, const uint8_t* _Nonnull data);


//-----------------------------------------------------------------------------
// utility
// [32 ABGR 0]

inline uint32_t lab_imm_pack_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (r) | (g << 8) | (b << 16) | (a << 24);
}

inline
unsigned int packRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (r) | (g << 8) | (b << 16) | (a << 24);
}

#ifdef __clang__
_Pragma("clang assume_nonnull end")
#endif

#endif

//-----------------------------------------------------------------------------

#ifdef LABIMMDRAW_IMPL

#include <string.h>

#ifdef __clang__
_Pragma("clang assume_nonnull begin")
#endif

// there's a really nice joined line with mitered or round ends here, under bsd-2.
// https://github.com/mapbox/mapbox-gl-native/blob/b8edc2399b9640498ccbbbb5b8f058c63d070933/src/mbgl/renderer/buckets/line_bucket.cpp

LAB_EXTERNC void lab_imm_line(LabImmContext* lc, float x0, float y0, float x1, float y1, float w) {
    float dy = x1 - x0;
    float dx = y1 - y0;
    float r = sqrtf(dx*dx + dy*dy);
    if (r < 0.001f)
        return;
    
    dy = w * dy / r;
    dx = w * dx / r;
    
    lab_imm_v2f(lc, x0+dx, y0-dy);
    lab_imm_v2f(lc, x1-dx, y1+dy);
    lab_imm_v2f(lc, x0-dx, y0+dy);

    lab_imm_v2f(lc, x0+dx, y0-dy);
    lab_imm_v2f(lc, x1+dx, y1-dy);
    lab_imm_v2f(lc, x1-dx, y1+dy);
}

LAB_EXTERNC void lab_imm_c4f(LabImmContext* lc,
                             float r, float g, float b, float a)
{
    uint8_t ri = (uint8_t) (r * 255.f);
    uint8_t gi = (uint8_t) (g * 255.f);
    uint8_t bi = (uint8_t) (b * 255.f);
    uint8_t ai = (uint8_t) (a * 255.f);
    lc->rgba = lab_imm_pack_RGBA(ri, gi, bi, ai);
}

LAB_EXTERNC size_t lab_imm_size_bytes(int vert_count) {
    return sizeof(float) * vert_count * (3 + 2 + 1);
}

LAB_EXTERNC void lab_imm_batch_begin(LabImmContext* lc,
                                     LabImmPlatformContext* pc,
        int sz, LabPrim prim, bool interleaved, float* data) 
{
    memset(lc, 0, sizeof(LabImmContext));
    lc->platform = pc;
    lc->sz = sz;
    lc->prim = prim;
    lc->interleaved = interleaved;
    lc->rgba = 0xffffffff;
    lc->data_pos = data;
    if (interleaved) {
        lc->stride = 5;
        lc->data_st = data + 2;
        lc->data_rgba = (uint32_t*) (data + (2 + 2));
    }
    else {
        lc->stride = 0;
        lc->data_st = data + sz * 2;
        lc->data_rgba = (uint32_t*) (lc->data_st + sz * 2);
    }
}


LAB_EXTERNC void lab_imm_batch_v2f(LabImmContext* lc, int count, float* v2f) {
    uint32_t c = lc->rgba;
    if (!lc->interleaved) {
        float* start = lc->data_pos + 2 * lc->count;
        memcpy(start, v2f, sizeof(float) * 2 * count);
        start = lc->data_st + 2 * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = lc->s;
            *start++ = lc->t;
        }
        uint32_t* cstart = lc->data_rgba + lc->count;
        for (int i = 0; i < count; ++i) {
            *cstart++ = c;
        }
    }
    else {
        float* start_pos = lc->data_pos + lc->count;
        float* start_st = lc->data_st + lc->count;
        uint32_t* start_rgba = (uint32_t*)(lc->data_rgba + lc->count);
        for (int i = 0; i < count; ++i) {
            start_pos[0] = *v2f++;
            start_pos[1] = *v2f++;
            start_st[0] = lc->s;
            start_st[1] = lc->t;
            start_rgba[0] = c;
            start_pos += lc->stride;
            start_st += lc->stride;
            start_rgba += lc->stride;
        }
    }
    lc->count += count;
}

LAB_EXTERNC void lab_imm_batch_v2f_st(LabImmContext* lc, int count, 
    float* v2f, float* st) 
{
    uint32_t c = lc->rgba;
    if (!lc->interleaved) {
        float* start = lc->data_pos + 2 * lc->count;
        memcpy(start, v2f, sizeof(float) * 2 * count);
        start = lc->data_st + 2 * lc->count;
        memcpy(start, st, sizeof(float) * 2 * count);
        uint32_t* cstart = lc->data_rgba + lc->count;
        for (int i = 0; i < count; ++i) {
            *cstart++ = c;
        }
    }
    else {
        float* start = lc->data_pos + lc->stride * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = *v2f++;
            *start++ = *v2f++;
            *start++ = *st++;
            *start++ = *st++;
            uint32_t* cstart = (uint32_t*) start;
            *cstart = c;
            start++;
        }
    }
    lc->count += count;
}

LAB_EXTERNC void lab_imm_batch_v2f_st_rgba(LabImmContext* lc, int count, 
    float* v2f, float* st, uint32_t* rgba) 
{
    if (!lc->interleaved) {
        float* start = lc->data_pos + 2 * lc->count;
        memcpy(start, v2f, sizeof(float) * 2 * count);
        start = lc->data_st + 2 * lc->count;
        memcpy(start, st, sizeof(float) * 2 * count);
        uint32_t* cstart = lc->data_rgba + lc->count;
        memcpy(cstart, rgba, sizeof(uint32_t) * count);
    }
    else {
        float* start = lc->data_pos + lc->stride * lc->count;
        for (int i = 0; i < count; ++i) {
            *start++ = *v2f++;
            *start++ = *v2f++;
            *start++ = *st++;
            *start++ = *st++;
            uint32_t* cstart = (uint32_t*) start;
            *cstart = *rgba++;
            start++;
        }
    }
    lc->count += count;
}

LAB_EXTERNC void lab_imm_v2f(LabImmContext* lc, float x, float y) {
    uint32_t c = lc->rgba;
    if (!lc->interleaved) {
        float* start = lc->data_pos + 2 * lc->count;
        start[0] = x;
        start[1] = y;
        start = lc->data_st + 2 * lc->count;
        start[0] = lc->s;
        start[1] = lc->t;
        uint32_t* cstart = lc->data_rgba + lc->count;
        *cstart = c;
    }
    else {
        float* start = lc->data_pos + lc->stride * lc->count;
        *start++ = x;
        *start++ = y;
        *start++ = lc->s;
        *start++ = lc->t;
        uint32_t* cstart = (uint32_t*) start;
        *cstart = c;
        ++start;
    }
    ++lc->count;
}

LAB_EXTERNC void lab_imm_v2f_st(LabImmContext* lc, 
        float x, float y, float s, float t) 
{
    lc->s = s;
    lc->t = t;
    lab_imm_v2f(lc, x, y);
}

LAB_EXTERNC void lab_imm_v2f_st_rgba(LabImmContext* lc, 
    float x, float y, float s, float t, uint8_t r, uint8_t g, uint8_t b, uint8_t a) 
{
    lc->s = s;
    lc->t = t;
    lc->rgba = lab_imm_pack_RGBA(r, g, b, a);
    lab_imm_v2f(lc, x, y);
}

#ifdef __clang__
_Pragma("clang assume_nonnull end")
#endif

#endif

#ifdef LAB_EXTERNC
#undef LAB_EXTERNC
#endif

