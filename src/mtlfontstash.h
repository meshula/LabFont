//
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
//

#ifndef MTLFONTSTASH_H
#define MTLFONTSTASH_H

#import <Metal/Metal.h>

FONScontext* _Nullable mtlfonsCreate(id<MTLDevice> _Nonnull device, int width, int height, int flags);
void mtlfonsSetRenderTargetPixelFormat(FONScontext* _Nonnull ctx, MTLPixelFormat pixelFormat);
void mtlfonsSetRenderCommandEncoder(FONScontext* _Nonnull ctx, id<MTLRenderCommandEncoder> _Nullable commandEncoder, MTLViewport viewport);
void mtlfonsDelete(FONScontext* _Nonnull ctx);

void mtlfonsDrawLine(FONScontext* _Nonnull stash, float x0, float y0, float x1, float y1);
unsigned int mtlfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#endif

#ifdef MTLFONTSTASH_IMPLEMENTATION

#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

// The length of the circular vertex attribute buffer shared by
// glyphs rendered from this stash. 1MB equals about 3,000 glyphs.
#define MTLFONS_BUFFER_LENGTH (1024 * 1024)

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

struct MTLFONScontext {
    id<MTLDevice> device;
    id<MTLRenderPipelineState> renderPipelineState;
    id<MTLBuffer> vertexBuffer;
    id<MTLTexture> atlasTexture;
    id<MTLRenderCommandEncoder> currentRenderCommandEncoder;
    MTLViewport currentViewport;
    int currentVertexBufferOffset;
    MTLPixelFormat pixelFormat;
};
typedef struct MTLFONScontext MTLFONScontext;

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

static int mtlfons__renderCreate(void* userPtr, int width, int height)
{
    MTLFONScontext* mtl = (MTLFONScontext*)userPtr;

    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                          width:width
                                                                                         height:height
                                                                                      mipmapped:NO];
    mtl->atlasTexture = [mtl->device newTextureWithDescriptor:descriptor];
    if (mtl->atlasTexture == nil) {
        return 0;
    }
    return 1;
}

static int mtlfons__renderResize(void* userPtr, int width, int height)
{
    return mtlfons__renderCreate(userPtr, width, height);
}

static void mtlfons__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
    MTLFONScontext* mtl = (MTLFONScontext*)userPtr;
    int w = rect[2] - rect[0];
    int h = rect[3] - rect[1];

    if (mtl->atlasTexture == nil) {
        return;
    }

    MTLRegion region = MTLRegionMake2D(rect[0], rect[1], w, h);
    int atlasWidth = (int)mtl->atlasTexture.width;
    unsigned char const* regionData = (data + (rect[1] * atlasWidth)) + rect[0];
    [mtl->atlasTexture replaceRegion:region mipmapLevel:0 withBytes:regionData bytesPerRow:atlasWidth];
}

static
id<MTLRenderPipelineState> _Nullable mtlfons__renderPipelineState(MTLFONScontext* mtl)  {
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

static void mtlfons__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
    MTLFONScontext* mtl = (MTLFONScontext*)userPtr;
    if (mtl->atlasTexture == 0) {
        return;
    }

    id<MTLRenderCommandEncoder> renderCommandEncoder = mtl->currentRenderCommandEncoder;

    id<MTLRenderPipelineState> pipelineState = mtlfons__renderPipelineState(mtl);
    [renderCommandEncoder setRenderPipelineState:pipelineState];

    int bufferLength = sizeof(MTLFONSvertex) * nverts;
    if (mtl->currentVertexBufferOffset + bufferLength > MTLFONS_BUFFER_LENGTH) {
        // Wrap the vertex buffer to the beginning, treating it as a circular buffer.
        mtl->currentVertexBufferOffset = 0;
    }

    MTLFONSvertex *vertexData = (MTLFONSvertex*) ((uint8_t*) mtl->vertexBuffer.contents + mtl->currentVertexBufferOffset);
    for (int i = 0; i < nverts; ++i) {
        memcpy(&vertexData[i].x, &verts[i * 2], sizeof(float) * 2);
        memcpy(&vertexData[i].tx, &tcoords[i * 2], sizeof(float) * 2);
        memcpy(&vertexData[i].rgba, &colors[i], sizeof(unsigned int));
    }

    [renderCommandEncoder setVertexBuffer:mtl->vertexBuffer offset:mtl->currentVertexBufferOffset atIndex:0];

    MTLViewport viewport = mtl->currentViewport;
    simd_float4x4 projectionMatrix = float4x4_ortho_projection(viewport.originX, viewport.originY,
                                                               viewport.width, viewport.height,
                                                               0, 1);
    [renderCommandEncoder setVertexBytes:&projectionMatrix length:sizeof(simd_float4x4) atIndex:1];

    [renderCommandEncoder setFragmentTexture:mtl->atlasTexture atIndex:0];

    [renderCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:nverts];

    mtl->currentVertexBufferOffset += bufferLength; // todo: align up
}

static void mtlfons__renderDelete(void* userPtr)
{
    MTLFONScontext* mtl = (MTLFONScontext*)userPtr;
    mtl->atlasTexture = nil;
    free(mtl);
}

FONScontext* _Nullable mtlfonsCreate(id<MTLDevice> device, int width, int height, int flags)
{
    MTLFONScontext *mtl = (MTLFONScontext *)calloc(1, sizeof(MTLFONScontext));
    if (mtl == NULL) {
        return NULL;
    }

    mtl->device = device;
    mtl->pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtl->vertexBuffer = [device newBufferWithLength:MTLFONS_BUFFER_LENGTH options:MTLResourceStorageModeShared];

    FONSparams params;
    memset(&params, 0, sizeof(params));
    params.width = width;
    params.height = height;
    params.flags = (unsigned char)flags;
    params.renderCreate = mtlfons__renderCreate;
    params.renderResize = mtlfons__renderResize;
    params.renderUpdate = mtlfons__renderUpdate;
    params.renderDraw = mtlfons__renderDraw;
    params.renderDelete = mtlfons__renderDelete;
    params.userPtr = mtl;

    return fonsCreateInternal(&params);
}

void mtlfonsSetRenderTargetPixelFormat(FONScontext* _Nonnull ctx, MTLPixelFormat pixelFormat) {
    MTLFONScontext* mtl = (MTLFONScontext *)ctx->params.userPtr;
    if (mtl == NULL) {
        return;
    }
    mtl->pixelFormat = (MTLPixelFormat)pixelFormat;
}

void mtlfonsSetRenderCommandEncoder(FONScontext* _Nonnull ctx, id<MTLRenderCommandEncoder> _Nullable commandEncoder, MTLViewport viewport) {
    MTLFONScontext* mtl = (MTLFONScontext *)ctx->params.userPtr;
    if (mtl == NULL) {
        return;
    }
    mtl->currentRenderCommandEncoder = commandEncoder;
    mtl->currentViewport = viewport;
}

void mtlfonsDelete(FONScontext* ctx)
{
    fonsDeleteInternal(ctx);
}

unsigned int mtlfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (r) | (g << 8) | (b << 16) | (a << 24);
}

void mtlfonsDrawLine(FONScontext* _Nonnull ctx, float x0, float y0, float x1, float y1) {
    MTLFONScontext* mtl = (MTLFONScontext *)ctx->params.userPtr;
    if (mtl->atlasTexture == 0) {
        return;
    }

    id<MTLRenderCommandEncoder> renderCommandEncoder = mtl->currentRenderCommandEncoder;

    id<MTLRenderPipelineState> pipelineState = mtlfons__renderPipelineState(mtl);
    [renderCommandEncoder setRenderPipelineState:pipelineState];

    MTLFONSvertex vertexData[] = {
        { .x = x0, .y = y0, .tx = 0, .ty = 0, .rgba = mtlfonsRGBA(0, 0, 0, 255) },
        { .x = x1, .y = y1, .tx = 0, .ty = 0, .rgba = mtlfonsRGBA(0, 0, 0, 255) }
    };

    [renderCommandEncoder setVertexBytes:vertexData length:sizeof(vertexData) atIndex:0];

    MTLViewport viewport = mtl->currentViewport;
    simd_float4x4 projectionMatrix = float4x4_ortho_projection(viewport.originX, viewport.originY,
                                                               viewport.width, viewport.height,
                                                               0, 1);
    [renderCommandEncoder setVertexBytes:&projectionMatrix length:sizeof(simd_float4x4) atIndex:1];

    [renderCommandEncoder setFragmentTexture:mtl->atlasTexture atIndex:0];

    [renderCommandEncoder drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:2];
}

NS_ASSUME_NONNULL_END

#endif

