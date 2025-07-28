/*
 * Denis Stepaniuk, 2025
 */

#include "common/types.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "metal/metal-cpp/Foundation/Foundation.hpp"
#include "metal/metal-cpp/Metal/Metal.hpp"
#include "metal/metal-cpp/QuartzCore/QuartzCore.hpp"
#include "metal/metal-cpp-extensions/AppKit/AppKit.hpp"
#include "metal/metal-cpp-extensions/MetalKit/MetalKit.hpp"

#include <simd/simd.h>

#include <stdio.h>
#include <string.h>

using NS::StringEncoding::UTF8StringEncoding;

static bool sdlInit()
{
   if (SDL_Init(SDL_INIT_VIDEO)) {
      return true;
   }
   fprintf(stderr, "Can't init SDL: %s", SDL_GetError());
   return false;
}

int main()
{
   if (!sdlInit()) {
      exit(1);
   }

   SDL_Window *mainWindow = SDL_CreateWindow(
      "metal_playground",
      960.0,
      960.0,
      SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE
   );
   if (mainWindow == NULL) {
      fprintf(stderr, "Failed to create SDL window: %s", SDL_GetError());
      exit(1);
   }

   NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();

   MTL::Device *device = MTL::CreateSystemDefaultDevice();
   SDL_MetalView sdlView = SDL_Metal_CreateView(mainWindow);
   CA::MetalLayer *layer = (CA::MetalLayer *) SDL_Metal_GetLayer(sdlView);
   layer->setDevice(device);
   layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);

   MTL::CommandQueue *commandQueue = device->newCommandQueue();

   /* load shaders */
   NS::Error *error = NULL;
   MTL::Library *library = device->newLibrary(NS::String::string("Shaders.metallib", UTF8StringEncoding), &error);
   if (!library) {
      fprintf(stderr, "%s", error->localizedDescription()->utf8String());
      exit(1);
   }

   MTL::Function *vertexFn = library->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
   MTL::Function *fragmentFn = library->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));
   
   MTL::RenderPipelineDescriptor *pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();
   pipelineDesc->setVertexFunction(vertexFn);
   pipelineDesc->setFragmentFunction(fragmentFn);
   pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

   MTL::RenderPipelineState *pipeline = device->newRenderPipelineState(pipelineDesc, &error);
   if (!pipeline) {
      fprintf(stderr, "%s", error->localizedDescription()->utf8String());
      exit(1);
   }

   /* setup buffers */
   constexpr u64 numVertices = 3;

   static constexpr simd::float3 positions[numVertices] =
   {
      { -0.8f,  0.8f, 0.0f },
      {  0.0f, -0.8f, 0.0f },
      { +0.8f,  0.8f, 0.0f }
   };

   static constexpr simd::float3 colors[numVertices] =
   {
      {  1.0, 0.3f, 0.2f },
      {  0.8f, 1.0, 0.0f },
      {  0.8f, 0.0f, 1.0 }
   };

   constexpr u64 posDataSize = numVertices * sizeof(simd::float3);
   constexpr u64 colDataSize = numVertices * sizeof(simd::float3);

   MTL::Buffer *vertexPosBuffer = device->newBuffer(posDataSize, MTL::ResourceStorageModeManaged);
   MTL::Buffer *vertexColBuffer = device->newBuffer(colDataSize, MTL::ResourceStorageModeManaged);

   memcpy(vertexPosBuffer->contents(), positions, posDataSize);
   memcpy(vertexColBuffer->contents(), colors, colDataSize);

   vertexPosBuffer->didModifyRange(NS::Range(0, vertexPosBuffer->length()));
   vertexPosBuffer->didModifyRange(NS::Range(0, vertexColBuffer->length()));

   /* draw */
   SDL_Event e;
   b32 quit = false;
   while (!quit) {
      while (SDL_PollEvent(&e)) {
         if (e.type == SDL_EVENT_QUIT) {
            quit = true;
         }
      }

      NS::AutoreleasePool *insidePool = NS::AutoreleasePool::alloc()->init();

      CA::MetalDrawable *drawable = layer->nextDrawable();

      MTL::CommandBuffer *commandBuffer = commandQueue->commandBuffer();
      MTL::RenderPassDescriptor *passDesc = MTL::RenderPassDescriptor::renderPassDescriptor();
      passDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
      passDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
      passDesc->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.2, 0.2, 0.2, 1.0));
      passDesc->colorAttachments()->object(0)->setTexture(drawable->texture());

      MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(passDesc);
      encoder->setRenderPipelineState(pipeline);
      encoder->setVertexBuffer(vertexPosBuffer, 0, 0);
      encoder->setVertexBuffer(vertexColBuffer, 0, 1);
      encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(numVertices));

      encoder->endEncoding();
      commandBuffer->presentDrawable(drawable);
      commandBuffer->commit();

      insidePool->release();
   }

   pool->release();
   
   SDL_Metal_DestroyView(sdlView);
   SDL_DestroyWindow(mainWindow);
   SDL_Quit();

   return 0;
}
