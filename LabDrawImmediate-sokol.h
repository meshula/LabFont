
//-----------------------------------------------------------------------------
// LabDrawImmediate-sokol.h
// Copyright (c) 2022 Nick Porcino
//

#ifndef LABDRAWIMM_SOKOL_H
#define LABDRAWIMM_SOKOL_H

#include "LabDrawImmediate.h"
#include "sokol_gfx.h"

inline
unsigned int packRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (r) | (g << 8) | (b << 16) | (a << 24);
}

#define ATLAS_CLEAR_TEXTURE 0
#define ATLAS_FONSTASH_TEXTURE 1
#define ATLAS_SLOT_MAX 16

struct FONSContext;
typedef struct FONScontext FONScontext;
typedef struct LabImmPlatformContext LabImmPlatformContext;

//-----------------------------------------------------------------------------
// context management
LabImmPlatformContext*
LabImmPlatformContextCreate(
    int font_atlas_width, int font_atlas_height);

void 
LabImmPlatformContextDestroy(
    LabImmPlatformContext*);

void
lab_imm_viewport_set(
    LabImmPlatformContext* _Nonnull mtl,
    float originX, float originY, float w, float h);

//-----------------------------------------------------------------------------
// drawing

void LabImmDrawArrays(
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
LabImmCreateAtlas(
    LabImmPlatformContext* _Nonnull,
    int texture_slot, int width, int height);

// given an in memory copy of the atlas, stored at data, Update will mark the
// region from srcx, srcy to srcx+w, srcy+h as needing a GPU refresh
void
LabImmAtlasUpdate(
    LabImmPlatformContext* _Nonnull,
    int texture_slot, int srcx, int srcy, int w, int h, const uint8_t* _Nonnull data);


#endif

//--------------------------
//  ___                 _
// |_ _|_ __ ___  _ __ | |
//  | || '_ ` _ \| '_ \| |
//  | || | | | | | |_) | |
// |___|_| |_| |_| .__/|_|
//               |_|
#ifdef LABIMMDRAW_SOKOL_IMPLEMENTATION

#include "fontstash.h"
#include "src/labimm-shd.h"


typedef struct {
    float x, y;
    float tx, ty;
    uint32_t rgba;
} LabImmFONSvertex;

typedef struct LabImmPlatformContext {
    FONScontext* fonsContext;

    sg_image atlasTexture[ATLAS_SLOT_MAX];
    sg_shader shader;
    bool atlasTexture_needs_refresh[ATLAS_SLOT_MAX];
    int next_texture_slot;

    labimm_v4f viewport; // x,y,w,h

    sg_pass_action pass_action;
    sg_pipeline pip_lines;
    sg_pipeline pip_line_strips;
    sg_pipeline pip_triangles;
    sg_pipeline pip_triangle_strips;
    sg_bindings bind;
    labimm_vs_params_t vs_params;
    labimm_fs_params_t fs_params;
    
    int currentVertexBufferOffset;
    int vertexBufferLength;
    sg_buffer vertexBuffer;
    
    LabImmFONSvertex* vertexData;
} LabImmPlatformContext;


void
float4x4_ortho_projection(float* mat, float left, float top, float right, float bottom, float near, float far)
{
    if (!mat)
        return;

    float xs = 2 / (right - left);
    float ys = 2 / (top - bottom);
    float zs = 1 / (near - far);
    float tx = (left + right) / (left - right);
    float ty = (top + bottom) / (bottom - top);
    float tz = near / (near - far);
    mat[0] = xs; mat[1] = 0; mat[2] = 0; mat[3] = 0;
    mat[4] = 0;  mat[5] = ys; mat[6] = 0; mat[7] = 0;
    mat[8] = 0;  mat[9] = 0;  mat[10] = zs; mat[11] = 0;
    mat[12] = tx; mat[13] = ty; mat[14] = tz; mat[15] = 1;
}

LabImmPlatformContext*
LabImmPlatformContextCreate(
    int font_atlas_width, int font_atlas_height)
{
    LabImmPlatformContext *ctx = (LabImmPlatformContext*)calloc(1, sizeof(LabImmPlatformContext));
    if (ctx == nullptr) {
        return nullptr;
    }

    // make a humorously small viewport default
    ctx->viewport = { 0, 0, 640, 480 };

    // small white block in slot 1
    LabImmCreateAtlas(ctx, ATLAS_CLEAR_TEXTURE, 4, 4);
    static uint8_t clear[16] = {
        0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    };
    LabImmAtlasUpdate(ctx, ATLAS_CLEAR_TEXTURE, 0, 0, 4, 4, clear);

    // set up fonstash to render through this immediate mode
    FONSparams params;
    memset(&params, 0, sizeof(params));
    params.width = font_atlas_width;
    params.height = font_atlas_height;
    params.flags = (unsigned char) FONS_ZERO_TOPLEFT;
    params.userPtr = ctx;

    params.renderCreate = [](void* ptr, int width, int height) -> int {
        LabImmPlatformContext* mtl = (LabImmPlatformContext*) ptr;
        sg_image_desc image_desc;
        memset(&image_desc, 0, sizeof(image_desc));
        image_desc.width = width;
        image_desc.height = height;
        image_desc.pixel_format = SG_PIXELFORMAT_R8;
        image_desc.usage = SG_USAGE_STREAM;
        image_desc.min_filter = SG_FILTER_LINEAR;
        image_desc.mag_filter = SG_FILTER_LINEAR;
        image_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        image_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE] = sg_make_image(&image_desc);
        return mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE].id == SG_INVALID_ID ? 0 : 1;
    };
    params.renderResize = params.renderCreate;

    params.renderUpdate = [](void* ptr, int* rect, const unsigned char* data) {
        LabImmPlatformContext* mtl = (LabImmPlatformContext*) ptr;
        mtl->atlasTexture_needs_refresh[ATLAS_FONSTASH_TEXTURE] = true;
    };

    params.renderDraw = [](void* ptr, 
        const float* verts, const float* tcoords, const unsigned int* colors, 
        int nverts) {
        LabImmPlatformContext* mtl = (LabImmPlatformContext*) ptr;
        // fonstash texture atlas in slot 2
        LabImmDrawArrays(mtl, ATLAS_FONSTASH_TEXTURE, false,
                         labprim_triangles,
                         verts, tcoords, colors, nverts);
    };

    // the atlas could be deleted on demand by fonstash, but there's no need,
    // the atlas texture is managed by the LabImmPlatformContext itself
    params.renderDelete = [](void*) {};

    ctx->fonsContext = fonsCreateInternal(&params);
    ctx->next_texture_slot = 2;

    ctx->shader = sg_make_shader(labimm_labimm_shader_desc(sg_query_backend()));

    sg_pipeline_desc pip_desc;
    memset(&pip_desc, 0, sizeof(pip_desc));
    pip_desc.shader = ctx->shader;
    pip_desc.colors[0].blend.enabled = true;
    pip_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pip_desc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    pip_desc.layout.buffers[ATTR_labimm_vs_position].stride =
                                        sizeof(LabImmFONSvertex);
    pip_desc.layout.attrs[ATTR_labimm_vs_position].format =
                                        SG_VERTEXFORMAT_FLOAT2; // pos
    pip_desc.layout.attrs[ATTR_labimm_vs_position].offset =
                                        offsetof(LabImmFONSvertex, x);
    pip_desc.layout.attrs[ATTR_labimm_vs_texcoord0].format =
                                        SG_VERTEXFORMAT_FLOAT2; // tc
    pip_desc.layout.attrs[ATTR_labimm_vs_texcoord0].offset =
                                        offsetof(LabImmFONSvertex, tx);
    pip_desc.layout.attrs[ATTR_labimm_vs_color0].format =
                                        SG_VERTEXFORMAT_UBYTE4N;  // col
    pip_desc.layout.attrs[ATTR_labimm_vs_color0].offset =
                                        offsetof(LabImmFONSvertex, rgba);
    
    pip_desc.index_type = SG_INDEXTYPE_UINT32;
    pip_desc.primitive_type = SG_PRIMITIVETYPE_LINES;
    ctx->pip_lines = sg_make_pipeline(&pip_desc);
    pip_desc.primitive_type = SG_PRIMITIVETYPE_LINE_STRIP;
    ctx->pip_line_strips = sg_make_pipeline(&pip_desc);
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    ctx->pip_triangles = sg_make_pipeline(&pip_desc);
    pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
    ctx->pip_triangle_strips = sg_make_pipeline(&pip_desc);

    /// @TODO the count should be an initialization parameter
    ctx->vertexBufferLength = 65536 * sizeof(LabImmFONSvertex);
    ctx->vertexData = (LabImmFONSvertex*) calloc(1, ctx->vertexBufferLength * 65536);
    
    sg_buffer_desc vbuf_desc;
    memset(&vbuf_desc, 0, sizeof(vbuf_desc));
    vbuf_desc.size = ctx->vertexBufferLength;
    vbuf_desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    vbuf_desc.usage = SG_USAGE_STREAM;
    vbuf_desc.label = "labimm-vertex-buffer";
    ctx->vertexBuffer = sg_make_buffer(&vbuf_desc);

    if (ctx->vertexBuffer.id == SG_INVALID_ID) {
        //printf("Couldn't create vertex buffer\n");
    }
    return ctx;
}

void LabImmPlatformContextDestroy(LabImmPlatformContext* ctx) {
    for (int i = 0; i < ATLAS_SLOT_MAX; ++i)
        if (ctx->atlasTexture[i].id != SG_INVALID_ID)
            sg_destroy_image(ctx->atlasTexture[i]);
    free(ctx);
}

void lab_imm_viewport_set(LabImmPlatformContext* mtl,
        float originX, float originY, float w, float h) {
    mtl->viewport.x = originX;
    mtl->viewport.y = originY;
    mtl->viewport.z = w;
    mtl->viewport.w = h;
}

int LabImmCreateAtlas(LabImmPlatformContext* ctx, int slot, int width, int height)
{
    if (ctx->atlasTexture[slot].id != SG_INVALID_ID)
        sg_destroy_image(ctx->atlasTexture[slot]);

    sg_image_desc image_desc;
    memset(&image_desc, 0, sizeof(image_desc));
    image_desc.width = width;
    image_desc.height = height;
    image_desc.pixel_format = SG_PIXELFORMAT_R8;
    image_desc.usage = SG_USAGE_STREAM;
    image_desc.min_filter = SG_FILTER_LINEAR;
    image_desc.mag_filter = SG_FILTER_LINEAR;
    image_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    image_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    ctx->atlasTexture[slot] = sg_make_image(&image_desc);
    return ctx->atlasTexture[slot].id != SG_INVALID_ID ? 1 : 0;
}

void LabImmAtlasUpdate(LabImmPlatformContext* mtl,
    int slot, int srcx, int srcy, int w, int h, const uint8_t* data)
{
    if (mtl->atlasTexture[slot].id == SG_INVALID_ID) {
        return;
    }

    mtl->atlasTexture_needs_refresh[slot] = true;
}

void LabImmDrawArrays(LabImmPlatformContext* ctx,
        int texture_slot, bool sample_nearest,
        LabPrim prim,
        const float* verts, const float* tcoords, const unsigned int* colors, 
        int nverts)
{
    if (ctx->atlasTexture[texture_slot].id == SG_INVALID_ID ||
        ctx->vertexBufferLength == 0) {
        return;
    }
    
    sg_push_debug_group("LabImmDrawArrays");
    
    switch (prim) {
        case labprim_lines:
            sg_apply_pipeline(ctx->pip_lines); break;
        case labprim_linestrip:
            sg_apply_pipeline(ctx->pip_line_strips); break;
        case labprim_triangles:
            sg_apply_pipeline(ctx->pip_triangles); break;
        case labprim_tristrip:             sg_apply_pipeline(ctx->pip_triangle_strips); break;
    }
    
    // set up vertex bindings
    int bufferLength = sizeof(LabImmFONSvertex) * nverts;
    if (ctx->currentVertexBufferOffset + bufferLength > ctx->vertexBufferLength) {
        // Wrap the vertex buffer to the beginning, treating it as a circular buffer.
        ctx->currentVertexBufferOffset = 0;
    }

    LabImmFONSvertex* vertexData = ctx->vertexData + ctx->currentVertexBufferOffset;
    
    for (int i = 0; i < nverts; ++i) {
        memcpy(&vertexData[i].x, &verts[i * 2], sizeof(float) * 2);
        memcpy(&vertexData[i].tx, &tcoords[i * 2], sizeof(float) * 2);
        memcpy(&vertexData[i].rgba, &colors[i], sizeof(unsigned int));
    }
    
    sg_range vert_range;
    vert_range.ptr = vertexData;
    vert_range.size = sizeof(LabImmFONSvertex) * nverts;
    sg_update_buffer(ctx->vertexBuffer, &vert_range);

    sg_apply_bindings(ctx->bind);
    
    // set up the uniforms
    float proj[16];
    float4x4_ortho_projection((float*)&ctx->vs_params.projectionMatrix,
            ctx->viewport.x, ctx->viewport.y, // left top
            ctx->viewport.x + ctx->viewport.z, // right
            ctx->viewport.y + ctx->viewport.w, // bottom
            0, 1);  // near far

    sg_apply_uniforms(SG_SHADERSTAGE_VS,
                      SLOT_labimm_vs_params,
                      SG_RANGE(ctx->vs_params));

    ctx->fs_params.texture_slot = texture_slot;
    sg_apply_uniforms(SG_SHADERSTAGE_FS,
                      SLOT_labimm_fs_params,
                      SG_RANGE(ctx->fs_params));
    sg_draw(0, nverts, 1);
    
    ctx->currentVertexBufferOffset += bufferLength; // todo: align up
    sg_pop_debug_group();
}

void lab_imm_batch_draw(
    LabImmContext* _Nonnull batch,
    int texture_slot, bool sample_nearest)
{
    if (!batch || !batch->platform || batch->interleaved ||
        batch->platform->atlasTexture[texture_slot].id == SG_INVALID_ID)
    {
        return;
    }

    if (!batch->interleaved) {
        LabImmDrawArrays(batch->platform,
            texture_slot, sample_nearest,
            batch->prim, 
            batch->data_pos, batch->data_st, batch->data_rgba, batch->count); 
    }
}


#endif
