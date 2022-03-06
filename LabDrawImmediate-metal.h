
//-----------------------------------------------------------------------------
// LabDrawImmediate-metal.h
//
// This file is heavily modified from mtlfontstash by Warren Moore.
// Modifications are (c) 2022 Nick Porcino, and continue the use of the
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

typedef struct {
    _Nullable id<MTLDevice> device;
    _Nullable id<MTLRenderPipelineState> renderPipelineState;
    _Nullable id<MTLBuffer> vertexBuffer;
    _Nullable id<MTLTexture> atlasTexture;
    _Nullable id<MTLRenderCommandEncoder> currentRenderCommandEncoder;
    MTLViewport viewport;
    int currentVertexBufferOffset;
    MTLPixelFormat pixelFormat;
} LabImmDrawContext;


//-----------------------------------------------------------------------------
// context management
LabImmDrawContext* _Nullable 
LabImmDrawContextCreate(
    id<MTLDevice> _Nonnull);

void 
LabImmDrawContextDestroy(
    LabImmDrawContext* _Nonnull);

// the default format is BGRA8Unorm, setting it here will also cause the
// render pipeline to be regenerated.
void LabImmDrawSetRenderTargetPixelFormat(LabImmDrawContext* _Nonnull mtl, 
        MTLPixelFormat pixelFormat);

void LabImmDrawSetRenderCommandEnconder(LabImmDrawContext* _Nonnull mtl,
        id<MTLRenderCommandEncoder> _Nullable);

void LabImmDrawSetViewport(LabImmDrawContext* _Nonnull mtl,
        float originX, float originY, float w, float h);
//-----------------------------------------------------------------------------
// drawing
void
LabImmDrawLine(
    LabImmDrawContext* _Nonnull, 
    float x0, float y0, float x1, float y1);
void LabImmDrawBatch(
    LabImmDrawContext* _Nonnull,
    LabImmContext* _Nonnull);
void LabImmDrawArrays(
    LabImmDrawContext* _Nonnull,
    const float* _Nonnull verts,
    const float* _Nonnull tcoords,
    const unsigned int* _Nonnull colors,
    int nverts);
//-----------------------------------------------------------------------------
// font/sprite atlas texture management
// If an atlas has been previously created, calling this will replace it
int
LabImmCreateAtlas(
    LabImmDrawContext* _Nonnull,
    int width, int height);
// given an in memory copy of the atlas, stored at data, Update will mark the
// region from srcx, srcy to srcx+w, srcy+h as needing a GPU refresh
void
LabImmAtlasUpdate(
    LabImmDrawContext* _Nonnull,
    int srcx, int srcy, int w, int h, const uint8_t* _Nonnull data);
#endif

#ifdef LABIMMDRAW_METAL_IMPLEMENTATION

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
    "fragment half4 mtlfontstash_fragment(FragmentIn in [[stage_in]],\n"
    "                                     texture2d<half, access::sample> atlasTexture [[texture(0)]])\n"
    "{\n"
    "    constexpr sampler linearSampler(coord::normalized, filter::linear, address::clamp_to_edge);\n"
    "    half mask = atlasTexture.sample(linearSampler, in.texCoords).r;\n"
    "    half4 color = in.color * mask;\n"
    "    return color;\n"
    "}";


typedef struct {
    float x, y;
    float tx, ty;
    unsigned int rgba;
} MTLFONSvertex;

static simd_float4x4 float4x4_ortho_projection(float left, float top, float right, float bottom, float near, float far)
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

LabImmDrawContext* _Nullable 
LabImmDrawContextCreate(
    id<MTLDevice> _Nonnull device)
{
    LabImmDrawContext *mtl = (LabImmDrawContext*)calloc(1, sizeof(LabImmDrawContext));
    if (mtl == NULL) {
        return NULL;
    }
    // make a humorously small viewport default
    mtl->viewport = { 0, 0, 640, 480 };
    mtl->device = device;
    mtl->pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtl->vertexBuffer = [device newBufferWithLength:MTLFONS_BUFFER_LENGTH 
                                            options:MTLResourceStorageModeShared];
    return mtl;
}

void LabImmDrawContextDestroy(LabImmDrawContext* _Nonnull mtl) {
    mtl->device = nil;
    mtl->renderPipelineState = nil;
    mtl->vertexBuffer = nil;
    mtl->atlasTexture = nil;
    mtl->currentRenderCommandEncoder = nil;
    free(mtl);
}

void LabImmDrawSetRenderTargetPixelFormat(LabImmDrawContext* _Nonnull mtl, 
        MTLPixelFormat pixelFormat) {
    if (mtl == NULL || mtl->pixelFormat == pixelFormat) {
        return;
    }
    mtl->pixelFormat = (MTLPixelFormat)pixelFormat;
    mtl->renderPipelineState = nil; // must recreate the pipeline
}

void LabImmDrawSetRenderCommandEnconder(LabImmDrawContext* _Nonnull mtl,
        id<MTLRenderCommandEncoder> _Nullable commandEncoder) {
    if (!mtl)
        return;
    mtl->currentRenderCommandEncoder = commandEncoder;
}

void LabImmDrawSetViewport(LabImmDrawContext* _Nonnull mtl,
        float originX, float originY, float w, float h) {
    mtl->viewport.originX = originX;
    mtl->viewport.originY = originY;
    mtl->viewport.width = w;
    mtl->viewport.height = h;
}


int LabImmCreateAtlas(LabImmDrawContext* _Nonnull mtl, int width, int height)
{
    MTLTextureDescriptor *descriptor = 
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    mtl->atlasTexture = [mtl->device newTextureWithDescriptor:descriptor];
    if (mtl->atlasTexture == nil) {
        return 0;
    }
    return 1;
}

void LabImmAtlasUpdate(LabImmDrawContext* _Nonnull mtl, 
    int srcx, int srcy, int w, int h, const uint8_t* data)
{
    if (mtl->atlasTexture == nil) {
        return;
    }

    MTLRegion region = MTLRegionMake2D(srcx, srcy, w, h);
    int atlasWidth = (int)mtl->atlasTexture.width;
    unsigned char const* regionData = (data + (srcy * atlasWidth)) + srcx;
    [mtl->atlasTexture replaceRegion:region 
                         mipmapLevel:0 
                           withBytes:regionData 
                         bytesPerRow:atlasWidth];
}

// create the render pipeline, on the fly, if necessary
static
id<MTLRenderPipelineState> _Nullable 
LabImmDraw__renderPipelineState(LabImmDrawContext* mtl)  {
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

void LabImmDrawArrays(LabImmDrawContext* mtl, 
        LabPrim prim,
        const float* verts, const float* tcoords, const unsigned int* colors, 
        int nverts)
{
    if (mtl->atlasTexture == 0 || !mtl->currentRenderCommandEncoder) {
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
    [renderCommandEncoder setFragmentTexture:mtl->atlasTexture atIndex:0];
    [renderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangle 
                             vertexStart:0 vertexCount:nverts];
    mtl->currentVertexBufferOffset += bufferLength; // todo: align up
}

void LabImmDrawBatch(
    LabImmDrawContext* _Nonnull mtl,
    LabImmContext* _Nonnull batch)
{
    if (!mtl || !batch || batch->interleaved ||
        mtl->atlasTexture == 0 || !mtl->currentRenderCommandEncoder) 
    {
        return;
    }

    if (batch->interleaved) {
        LabImmDrawArrays(mtl, 
            batch->prim, 
            batch->data_pos, batch->data_st, batch->data_rgba, batch->count); 
    }
}

void LabImmDrawLine(
        LabImmDrawContext* _Nonnull mtl,
        float x0, float y0, float x1, float y1) 
{
    if (mtl->atlasTexture == 0) {
        return;
    }

    id<MTLRenderCommandEncoder> renderCommandEncoder = mtl->currentRenderCommandEncoder;

    id<MTLRenderPipelineState> pipelineState = LabImmDraw__renderPipelineState(mtl);
    [renderCommandEncoder setRenderPipelineState:pipelineState];

    MTLFONSvertex vertexData[] = {
        { .x = x0, .y = y0, .tx = 0, .ty = 0, .rgba = mtlfonsRGBA(0, 0, 0, 255) },
        { .x = x1, .y = y1, .tx = 0, .ty = 0, .rgba = mtlfonsRGBA(0, 0, 0, 255) }
    };

    [renderCommandEncoder setVertexBytes:vertexData 
                                  length:sizeof(vertexData) atIndex:0];

    MTLViewport viewport = mtl->viewport;
    simd_float4x4 projectionMatrix = float4x4_ortho_projection(
                                        viewport.originX, viewport.originY,
                                        viewport.width, viewport.height,
                                        0, 1);
    [renderCommandEncoder setVertexBytes:&projectionMatrix length:sizeof(simd_float4x4) atIndex:1];
    [renderCommandEncoder setFragmentTexture:mtl->atlasTexture atIndex:0];
    [renderCommandEncoder drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:2];
}

NS_ASSUME_NONNULL_END

#endif

