
//-----------------------------------------------------------------------------
// LabDrawImmediate-metal.h
// Copyright (c) 2022 Nick Porcino
//
// This file is heavily modified from mtlfontstash by Warren Moore.
// All the new stuff, and modifications are (c) 2022 Nick Porcino, 
// and continue the use of the
// zlib license, which appears on the original source code and is 
// reproduced below with the copyright notice for the original code.
//-----------------------------------------------------------------------------
// Copyright (c) 2022 Warren Moore wm@warrenmoore.net
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
    // Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//-----------------------------------------------------------------------------

#ifndef LABDRAWIMM_METAL_H
#define LABDRAWIMM_METAL_H

#import "LabDrawImmediate.h"

#import <Metal/Metal.h>

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
    _Nullable id<MTLDevice> device;
    _Nullable id<MTLRenderPipelineState> renderPipelineState;
    _Nullable id<MTLBuffer> vertexBuffer;
    _Nullable id<MTLTexture> atlasTexture[ATLAS_SLOT_MAX];
    _Nullable id<MTLRenderCommandEncoder> currentRenderCommandEncoder;
    MTLViewport viewport;
    int currentVertexBufferOffset;
    MTLPixelFormat pixelFormat;
    FONScontext* _Nullable fonsContext;
    int next_texture_slot;
} LabImmPlatformContext;


//-----------------------------------------------------------------------------
// context management
LabImmPlatformContext* _Nullable
LabImmPlatformContextCreate(
    id<MTLDevice> _Nonnull,
    int font_atlas_width, int font_atlas_height);

void 
LabImmPlatformContextDestroy(
    LabImmPlatformContext* _Nonnull);

// the default format is BGRA8Unorm, setting it here will also cause the
// render pipeline to be regenerated.
void
LabImmDrawSetRenderTargetPixelFormat(
    LabImmPlatformContext* _Nonnull mtl,
    MTLPixelFormat pixelFormat);

void
lab_imm_MTLRenderCommandEncoder_set(
    LabImmPlatformContext* _Nonnull mtl,
    id<MTLRenderCommandEncoder> _Nullable);

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

#ifdef LABIMMDRAW_METAL_IMPLEMENTATION

#include "fontstash.h"
#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

// The length of the circular vertex attribute buffer shared by
// glyphs rendered from this stash. 1MB equals about 3,000 glyphs.
#define MTLFONS_BUFFER_LENGTH (4 * 1024 * 1024)

static NSString *shaderSource = @""
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct Vertex {\n"
    "    float2 position  [[attribute(0)]];\n"
    "    float2 texCoords [[attribute(1)]];\n"
    "    half4 color      [[attribute(2)]];\n"
    "};\n"
    "\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoords;\n"
    "    half4 color;\n"
    "};\n"
    "\n"
    "vertex VertexOut mtlfontstash_vertex(Vertex in [[stage_in]],\n"
    "                                     constant float4x4 &projectionMatrix [[buffer(1)]])\n"
    "{\n"
    "    VertexOut out;\n"
    "    out.position = projectionMatrix * float4(in.position, 0.0, 1.0);\n"
    "    out.texCoords = in.texCoords;\n"
    "    out.color = in.color;\n"
    "    return out;\n"
    "}\n"
    "\n"
    "typedef VertexOut FragmentIn;\n"
    "\n"
    "fragment half4 mtlfontstash_fragment(\n"
    "                  FragmentIn in [[stage_in]],\n"
    "                  constant int& texture_slot [[buffer(1)]],\n"
    "                  constant float& closest_linear [[buffer(2)]],\n"
    "                  array<texture2d<half, access::sample>,16> atlasTexture [[texture(0)]])\n"
    "{\n"
    "    constexpr sampler linearSampler(coord::normalized, filter::linear, address::clamp_to_edge);\n"
    "    half mask_lin = atlasTexture[texture_slot].sample(linearSampler, in.texCoords).r;\n"
    "    constexpr sampler closestSampler(coord::normalized, filter::nearest, address::clamp_to_edge);\n"
    "    half mask_closest = atlasTexture[texture_slot].sample(closestSampler, in.texCoords).r;\n"
    "    half mask = mask_lin * closest_linear + mask_closest * (1.f - closest_linear);"
    "    half4 color = in.color * mask;\n"
    "    return color;\n"
    "}";


typedef struct {
    float x, y;
    float tx, ty;
    unsigned int rgba;
} MTLFONSvertex;

static simd_float4x4
float4x4_ortho_projection(float left, float top, float right, float bottom, float near, float far)
{
    float xs = 2 / (right - left);
    float ys = 2 / (top - bottom);
    float zs = 1 / (near - far);
    float tx = (left + right) / (left - right);
    float ty = (top + bottom) / (bottom - top);
    float tz = near / (near - far);
    simd_float4x4 P = {{
        { xs,  0,  0, 0 },
        {  0, ys,  0, 0 },
        {  0,  0, zs, 0 },
        { tx, ty, tz, 1 },
    }};
    return P;
}

LabImmPlatformContext* _Nullable
LabImmPlatformContextCreate(
    id<MTLDevice> _Nonnull device,
    int font_atlas_width, int font_atlas_height)
{
    LabImmPlatformContext *mtl = (LabImmPlatformContext*)calloc(1, sizeof(LabImmPlatformContext));
    if (mtl == NULL) {
        return NULL;
    }
    // make a humorously small viewport default
    mtl->viewport = { 0, 0, 640, 480 };
    mtl->device = device;
    mtl->pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtl->vertexBuffer = [device newBufferWithLength:MTLFONS_BUFFER_LENGTH 
                                            options:MTLResourceStorageModeShared];

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
        MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
                   texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                width:width
                                               height:height
                                            mipmapped:NO];
        descriptor.storageMode = MTLStorageModeShared;
        mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE] = [mtl->device newTextureWithDescriptor:descriptor];
        return mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE] == nil ? 0 : 1;
    };
    params.renderResize = params.renderCreate;

    params.renderUpdate = [](void* ptr, int* rect, const unsigned char* data) {
        LabImmPlatformContext* mtl = (LabImmPlatformContext*) ptr;
        int w = rect[2] - rect[0];
        int h = rect[3] - rect[1];

        if (mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE] != nil) {
            MTLRegion region = MTLRegionMake2D(rect[0], rect[1], w, h);
            int atlasWidth = (int)mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE].width;
            unsigned char const* regionData = (data + (rect[1] * atlasWidth)) + rect[0];
            [mtl->atlasTexture[ATLAS_FONSTASH_TEXTURE] replaceRegion:region
                                    mipmapLevel:0 
                                    withBytes:regionData 
                                    bytesPerRow:atlasWidth];
        }
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

void LabImmPlatformContextDestroy(LabImmPlatformContext* _Nonnull mtl) {
    mtl->device = nil;
    mtl->renderPipelineState = nil;
    mtl->vertexBuffer = nil;
    for (int i = 0; i < ATLAS_SLOT_MAX; ++i)
        mtl->atlasTexture[i] = nil;
    mtl->currentRenderCommandEncoder = nil;
    free(mtl);
}

void LabImmDrawSetRenderTargetPixelFormat(LabImmPlatformContext* _Nonnull mtl,
        MTLPixelFormat pixelFormat) {
    if (mtl == NULL || mtl->pixelFormat == pixelFormat) {
        return;
    }
    mtl->pixelFormat = (MTLPixelFormat)pixelFormat;
    mtl->renderPipelineState = nil; // must recreate the pipeline
}

void lab_imm_MTLRenderCommandEncoder_set(LabImmPlatformContext* _Nonnull mtl,
        id<MTLRenderCommandEncoder> _Nullable commandEncoder) {
    if (!mtl)
        return;
    mtl->currentRenderCommandEncoder = commandEncoder;
}

void lab_imm_viewport_set(LabImmPlatformContext* _Nonnull mtl,
        float originX, float originY, float w, float h) {
    mtl->viewport.originX = originX;
    mtl->viewport.originY = originY;
    mtl->viewport.width = w;
    mtl->viewport.height = h;
}


int LabImmCreateAtlas(LabImmPlatformContext* _Nonnull mtl, int slot, int width, int height)
{
    MTLTextureDescriptor *descriptor = 
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    mtl->atlasTexture[slot] = [mtl->device newTextureWithDescriptor:descriptor];
    if (mtl->atlasTexture[slot] == nil) {
        return 0;
    }
    return 1;
}

void LabImmAtlasUpdate(LabImmPlatformContext* _Nonnull mtl,
    int slot, int srcx, int srcy, int w, int h, const uint8_t* data)
{
    if (mtl->atlasTexture[slot] == nil) {
        return;
    }

    MTLRegion region = MTLRegionMake2D(srcx, srcy, w, h);
    int atlasWidth = (int)mtl->atlasTexture[slot].width;
    unsigned char const* regionData = (data + (srcy * atlasWidth)) + srcx;
    [mtl->atlasTexture[slot] replaceRegion:region
                         mipmapLevel:0 
                           withBytes:regionData 
                         bytesPerRow:atlasWidth];
}

// create the render pipeline, on the fly, if necessary
static
id<MTLRenderPipelineState> _Nullable 
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



NS_ASSUME_NONNULL_END

#endif

