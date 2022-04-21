
//-----------------------------------------------------------------------------
// LabDrawImmediate-sokol.h
// Copyright (c) 2022 Nick Porcino
//

#ifndef LABDRAWIMM_SOKOL_H
#define LABDRAWIMM_SOKOL_H

#import "LabDrawImmediate.h"

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

typedef struct LabImmPlatformContext {
    FONScontext* fonsContext;
    labimm_v4f viewport; // x,y,w,h
    sg_image atlasTexture[ATLAS_SLOT_MAX];
    bool atlasTexture_needs_refresh[ATLAS_SLOT_MAX];
    int next_texture_slot;
} LabImmPlatformContext;

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

//-----------------------------------------------------------------------------

#ifdef LABIMMDRAW_SOKOL_IMPLEMENTATION

#include "fontstash.h"
#include "src/labimm-shd.h"

typedef struct {
    float x, y;
    float tx, ty;
    unsigned int rgba;
} LabImmFONSvertex;

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
    LabImmPlatformContext *mtl = (LabImmPlatformContext*)calloc(1, sizeof(LabImmPlatformContext));
    if (mtl == nullptr) {
        return nullptr;
    }

    // make a humorously small viewport default
    mtl->viewport = { 0, 0, 640, 480 };

    // small white block in slot 1
    LabImmCreateAtlas(mtl, ATLAS_CLEAR_TEXTURE, 4, 4);
    static uint8_t clear[16] = {
        0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,  0xff, 0xff, 0xff, 0xff,
    };
    LabImmAtlasUpdate(mtl, ATLAS_CLEAR_TEXTURE, 0, 0, 4, 4, clear);

    // set up fonstash to render through this immediate mode
    FONSparams params;
    memset(&params, 0, sizeof(params));
    params.width = font_atlas_width;
    params.height = font_atlas_height;
    params.flags = (unsigned char) FONS_ZERO_TOPLEFT;
    params.userPtr = mtl;

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

    mtl->fonsContext = fonsCreateInternal(&params);
    mtl->next_texture_slot = 2;
    return mtl;
}

void LabImmPlatformContextDestroy(LabImmPlatformContext* mtl) {
    for (int i = 0; i < ATLAS_SLOT_MAX; ++i)
        if (mtl->atlasTexture[i].id != SG_INVALID_ID)
            sg_destroy_image(mtl->atlasTexture[i]);
    free(mtl);
}

void lab_imm_viewport_set(LabImmPlatformContext* mtl,
        float originX, float originY, float w, float h) {
    mtl->viewport.x = originX;
    mtl->viewport.y = originY;
    mtl->viewport.z = w;
    mtl->viewport.w = h;
}

int LabImmCreateAtlas(LabImmPlatformContext* mtl, int slot, int width, int height)
{
    if (mtl->atlasTexture[slot].id != SG_INVALID_ID)
        sg_destroy_image(mtl->atlasTexture[slot]);

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
    mtl->atlasTexture[slot] = sg_make_image(&image_desc);
    return mtl->atlasTexture[slot].id != SG_INVALID_ID ? 1 : 0;
}

void LabImmAtlasUpdate(LabImmPlatformContext* mtl,
    int slot, int srcx, int srcy, int w, int h, const uint8_t* data)
{
    if (mtl->atlasTexture[slot].id == SG_INVALID_ID) {
        return;
    }

    mtl->atlasTexture_needs_refresh[slot] = true;
}

// create the render pipeline, on the fly, if necessary
static
id<MTLRenderPipelineState> 
LabImmDraw__renderPipelineState(LabImmPlatformContext* mtl)  {
    if (mtl->renderPipelineState != nil) {
        return mtl->renderPipelineState;
    }

    NSError *error = nil;
    id<MTLLibrary> library = [mtl->device newLibraryWithSource:shaderSource options:nil error:&error];
    if (library == nil) {
        NSLog(@"Could not create Metal library from source: %@", error);
        return nil;
    }
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"mtlfontstash_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"mtlfontstash_fragment"];

    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = 8;
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4Normalized;
    vertexDescriptor.attributes[2].offset = 16;
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.layouts[0].stride = 20;

    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexDescriptor = vertexDescriptor;

    descriptor.colorAttachments[0].pixelFormat = mtl->pixelFormat;
    descriptor.colorAttachments[0].blendingEnabled = YES;
    descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    descriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    descriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    descriptor.vertexFunction = vertexFunction;
    descriptor.fragmentFunction = fragmentFunction;

    error = nil;
    id<MTLRenderPipelineState> pipelineState = [mtl->device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (pipelineState == nil) {
        NSLog(@"Error when compiling mtlfontstash render pipeline state: %@", error);
    }

    mtl->renderPipelineState = pipelineState;

    return pipelineState;
}

void LabImmDrawArrays(LabImmPlatformContext* mtl,
        int texture_slot, bool sample_nearest,
        LabPrim prim,
        const float* verts, const float* tcoords, const unsigned int* colors, 
        int nverts)
{
    if (mtl->atlasTexture[texture_slot] == 0 || !mtl->currentRenderCommandEncoder) {
        return;
    }

    id<MTLRenderCommandEncoder> renderCommandEncoder = mtl->currentRenderCommandEncoder;

    id<MTLRenderPipelineState> pipelineState = LabImmDraw__renderPipelineState(mtl);
    [renderCommandEncoder setRenderPipelineState:pipelineState];

    int bufferLength = sizeof(MTLFONSvertex) * nverts;
    if (mtl->currentVertexBufferOffset + bufferLength > MTLFONS_BUFFER_LENGTH) {
        // Wrap the vertex buffer to the beginning, treating it as a circular buffer.
        mtl->currentVertexBufferOffset = 0;
    }

    MTLFONSvertex *vertexData = 
        (MTLFONSvertex*) ((uint8_t*) mtl->vertexBuffer.contents +
                                     mtl->currentVertexBufferOffset);
    
    for (int i = 0; i < nverts; ++i) {
        memcpy(&vertexData[i].x, &verts[i * 2], sizeof(float) * 2);
        memcpy(&vertexData[i].tx, &tcoords[i * 2], sizeof(float) * 2);
        memcpy(&vertexData[i].rgba, &colors[i], sizeof(unsigned int));
    }

    [renderCommandEncoder setVertexBuffer:mtl->vertexBuffer 
                                   offset:mtl->currentVertexBufferOffset 
                                  atIndex:0];

    MTLViewport viewport = mtl->viewport;
    simd_float4x4 projectionMatrix = 
        float4x4_ortho_projection(viewport.originX, viewport.originY,
                                  viewport.width, viewport.height,
                                  0, 1);
    
    MTLPrimitiveType mtl_prim;
    switch(prim) {
        case labprim_lines: mtl_prim = MTLPrimitiveTypeLine; break;
        case labprim_linestrip: mtl_prim = MTLPrimitiveTypeLineStrip; break;
        case labprim_triangles: mtl_prim = MTLPrimitiveTypeTriangle; break;
        case labprim_tristrip: mtl_prim = MTLPrimitiveTypeTriangleStrip; break;
    }
    [renderCommandEncoder setVertexBytes:&projectionMatrix
                                  length:sizeof(simd_float4x4) atIndex:1];
    [renderCommandEncoder setFragmentBytes:&texture_slot
                                  length:sizeof(int) atIndex:1];
    
    float closest_linear = sample_nearest ? 0.f : 1.f;
    [renderCommandEncoder setFragmentBytes:&closest_linear
                                  length:sizeof(float) atIndex:2];
    [renderCommandEncoder setFragmentTextures:mtl->atlasTexture withRange:NSMakeRange(0,ATLAS_SLOT_MAX)];
    [renderCommandEncoder drawPrimitives:mtl_prim
                             vertexStart:0 vertexCount:nverts];
    mtl->currentVertexBufferOffset += bufferLength; // todo: align up
}

void lab_imm_batch_draw(
    LabImmContext* _Nonnull batch,
    int texture_slot, bool sample_nearest)
{
    if (!batch || !batch->platform || batch->interleaved ||
        batch->platform->atlasTexture[texture_slot] == 0 || !batch->platform->currentRenderCommandEncoder)
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
