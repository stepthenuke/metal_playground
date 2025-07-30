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

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "common/shader_types.h"

using NS::StringEncoding::UTF8StringEncoding;

constexpr u32 maxFramesInFlights = 3;
simd_uint2 viewportSize = {960, 960};

const simd_float4 red = { 1.0, 0.0, 0.0, 1.0 };
const simd_float4 green = { 0.0, 1.0, 0.0, 1.0 };
const simd_float4 blue = { 0.0, 0.0, 1.0, 1.0 };

typedef struct TriangleData {
    VertexData vertex0;
    VertexData vertex1;
    VertexData vertex2;
}
TriangleData;

void triangleRedGreenBlue(float radius,
                          float rotationInDegrees,
                          TriangleData *triangleData)
{
    const float angle0 = (float)rotationInDegrees * M_PI / 180.0f;
    const float angle1 = angle0 + (2.0f * M_PI  / 3.0f);
    const float angle2 = angle0 + (4.0f * M_PI  / 3.0f);

    simd_float2 position0 = {
        radius * cosf(angle0),
        radius * sinf(angle0)
    };

    simd_float2 position1 = {
        radius * cosf(angle1),
        radius * sinf(angle1)
    };

    simd_float2 position2 = {
        radius * cosf(angle2),
        radius * sinf(angle2)
    };

    triangleData->vertex0.color = red;
    triangleData->vertex0.position = position0;

    triangleData->vertex1.color = green;
    triangleData->vertex1.position = position1;

    triangleData->vertex2.color = blue;
    triangleData->vertex2.position = position2;
}

static bool sdlInit()
{
   if (SDL_Init(SDL_INIT_VIDEO)) {
      return true;
   }
   fprintf(stderr, "Can't init SDL: %s", SDL_GetError());
   return false;
}

static void dumpInfo(MTL::Device *device)
{
   printf("Device name: %s\n", device->name()->cString(UTF8StringEncoding));

   MTL::GPUFamily families[9] = {
      MTL::GPUFamilyApple9, MTL::GPUFamilyApple8, MTL::GPUFamilyApple7, 
      MTL::GPUFamilyApple6, MTL::GPUFamilyApple5, MTL::GPUFamilyApple4,
      MTL::GPUFamilyApple3, MTL::GPUFamilyApple2, MTL::GPUFamilyApple1,
   };
   for (int idx = 0; idx < 9; ++idx) {
      if (device->supportsFamily(families[idx])) {
         printf("Device family: Apple%d\n", 9 - idx);
         break;
      }
   }
   printf("Raytraycing support: %d\n", device->supportsRaytracing());
}

void setVertexData(MTL::Buffer *buffer, int frameNumber)
{
   constexpr float radius = 350.0f;
   u32 rotation = frameNumber % 360;

   TriangleData triangleData;
   triangleRedGreenBlue(radius, rotation, &triangleData);
   
   memcpy(buffer->contents(), &triangleData, sizeof(TriangleData));
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

   dumpInfo(device);

   SDL_MetalView sdlView = SDL_Metal_CreateView(mainWindow);
   CA::MetalLayer *layer = (CA::MetalLayer *) SDL_Metal_GetLayer(sdlView);
   layer->setDevice(device);
   layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

   /* init */
   int frameNumber = 0;

   MTL4::CommandQueue *commandQueue = device->newMTL4CommandQueue();
   MTL4::CommandBuffer *commandBuffer = device->newCommandBuffer();

   /* create render pipeline state */
   NS::Error *error = NULL;

   MTL::Library *library = device->newLibrary(NS::String::string("Shaders.metallib", UTF8StringEncoding), &error);
   if (!library) {
      fprintf(stderr, "%s", error ? error->localizedDescription()->utf8String() : "unknown");
      exit(1);
   }      

   /* Check if archive exists and load if so */
   const char *archivePath = "Archive.mta";
   NS::URL *archiveURL = (access(archivePath, F_OK) == 0) ? NS::URL::fileURLWithPath(NS::String::string("Archive.mta", UTF8StringEncoding)) : NULL;
   MTL4::Archive *defaultArchive = NULL;
   if (archiveURL) {
      defaultArchive = device->newArchive(archiveURL, &error);
      if (!defaultArchive) {
         fprintf(stderr, "%s", error ? error->localizedDescription()->utf8String() : "unknown");
         exit(1);
      }
   }

   MTL4::CompilerDescriptor *compilerDescriptor = MTL4::CompilerDescriptor::alloc()->init();
   MTL4::Compiler *compiler = device->newCompiler(compilerDescriptor, &error);
   if (!compiler) {
      fprintf(stderr, "%s", error ? error->localizedDescription()->utf8String() : "unknown");
      exit(1);
   }

   MTL4::RenderPipelineDescriptor *renderPipelineDescriptor = MTL4::RenderPipelineDescriptor::alloc()->init();
   renderPipelineDescriptor->setLabel(NS::String::string("MTL 4 render pipeline", UTF8StringEncoding));

   MTL4::LibraryFunctionDescriptor *vertexFunction = MTL4::LibraryFunctionDescriptor::alloc()->init();
   vertexFunction->setLibrary(library);
   vertexFunction->setName(NS::String::string("vertexShader", UTF8StringEncoding));
   renderPipelineDescriptor->setVertexFunctionDescriptor(vertexFunction);

   MTL4::LibraryFunctionDescriptor *fragmentFunction = MTL4::LibraryFunctionDescriptor::alloc()->init();
   fragmentFunction->setLibrary(library);
   fragmentFunction->setName(NS::String::string("fragmentShader", UTF8StringEncoding));
   renderPipelineDescriptor->setFragmentFunctionDescriptor(fragmentFunction);

   renderPipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm);

   MTL4::CompilerTaskOptions *compilerTaskOptions = MTL4::CompilerTaskOptions::alloc()->init();
   if (defaultArchive) {
      const NS::Object *archivesObjects = { defaultArchive };
      NS::Array *archives = NS::Array::array(archivesObjects);
      compilerTaskOptions->setLookupArchives(archives);
   }

   MTL::RenderPipelineState *pipeline = compiler->newRenderPipelineState(renderPipelineDescriptor, compilerTaskOptions, &error);
   if (!pipeline) {
      fprintf(stderr, "%s", error ? error->localizedDescription()->utf8String() : "unknown");
      exit(1);
   }

   /* create buffers */
   MTL::Buffer *vertexBuffers[maxFramesInFlights] {};

   for (i32 bufIdx = 0; bufIdx < maxFramesInFlights; ++bufIdx) {
      vertexBuffers[bufIdx] = device->newBuffer(sizeof(TriangleData), MTL::ResourceStorageModeShared);
   }
   MTL::Buffer *viewportSizeBuffer = device->newBuffer(sizeof(viewportSize), MTL::ResourceStorageModeShared);

   /* create argument table */
   MTL4::ArgumentTableDescriptor *argumentTableDescriptor = MTL4::ArgumentTableDescriptor::alloc()->init();
   argumentTableDescriptor->setMaxBufferBindCount(2);

   MTL4::ArgumentTable *argumentTable = device->newArgumentTable(argumentTableDescriptor, &error);
   if (!argumentTable) {
      fprintf(stderr, "%s", error ? error->localizedDescription()->utf8String() : "unknown");
      exit(1);
   }

   /* create residency set */
   MTL::ResidencySetDescriptor *residencySetDescriptor = MTL::ResidencySetDescriptor::alloc()->init();

   MTL::ResidencySet *residencySet = device->newResidencySet(residencySetDescriptor, &error);
   if (!residencySet) {
      fprintf(stderr, "%s", error ? error->localizedDescription()->utf8String() : "unknown");
      exit(1);
   }

   /* create command allocators */
   MTL4::CommandAllocator *commandAllocators[maxFramesInFlights] {};

   for (i32 frameIdx = 0; frameIdx < maxFramesInFlights; ++frameIdx) {
      MTL4::CommandAllocator *allocator = device->newCommandAllocator();
      if (!allocator) {
         fprintf(stderr, "%s", "CommandAllocator cannot be created");
         exit(1);
      }
      commandAllocators[frameIdx] = allocator;
   }


   /* configure residency sets */
   commandQueue->addResidencySet(residencySet);
   commandQueue->addResidencySet(layer->residencySet());

   residencySet->addAllocation(viewportSizeBuffer);
   for (i32 bufIdx = 0; bufIdx < maxFramesInFlights; ++bufIdx) {
      residencySet->addAllocation(vertexBuffers[bufIdx]);
   }

   residencySet->commit();

   /* create shared event */
   MTL::SharedEvent *sharedEvent = device->newSharedEvent();
   sharedEvent->setSignaledValue(frameNumber);

   /* update viewport size */
   viewportSize.x = layer->drawableSize().width;
   viewportSize.y = layer->drawableSize().height;

   memcpy(viewportSizeBuffer->contents(), &viewportSize, sizeof(viewportSize));

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
      if (!drawable) {
         continue;
      }

      /* set up render pass descriptor as we don't have MTK::View, but have a CAMetalLayer */
      MTL4::RenderPassDescriptor *renderPassDescriptor = MTL4::RenderPassDescriptor::alloc()->init();
      renderPassDescriptor->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
      renderPassDescriptor->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
      renderPassDescriptor->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.2, 0.2, 0.2, 1.0));
      renderPassDescriptor->colorAttachments()->object(0)->setTexture(drawable->texture());

      frameNumber += 1;

      char frameStrBuffer[64];
      snprintf(frameStrBuffer, 64, "Frame %d", frameNumber); 
      NS::String *frameString = NS::String::string(frameStrBuffer, UTF8StringEncoding);

      if (frameNumber >= maxFramesInFlights) {
         u64 previousValueToWaitFor = frameNumber - maxFramesInFlights;
         sharedEvent->waitUntilSignaledValue(previousValueToWaitFor, 10);
      }

      u32 frameIdx = frameNumber % maxFramesInFlights;
      MTL4::CommandAllocator *frameAllocator = commandAllocators[frameIdx];
      frameAllocator->reset();

      commandBuffer->beginCommandBuffer(frameAllocator);
      commandBuffer->setLabel(frameString);

      MTL4::RenderCommandEncoder *renderEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);
      renderEncoder->setLabel(frameString);
      
      MTL::Viewport viewPort;
      viewPort.originX = 0.0;
      viewPort.originY = 0.0;
      viewPort.width = (double)viewportSize.x;
      viewPort.height = (double)viewportSize.y;
      viewPort.znear = 0.0;
      viewPort.zfar = 1.0;

      renderEncoder->setViewport(viewPort);

      renderEncoder->setRenderPipelineState(pipeline);

      renderEncoder->setArgumentTable(argumentTable, MTL::RenderStageVertex);

      MTL::Buffer *vertexBuffer = vertexBuffers[frameIdx];
      setVertexData(vertexBuffer, frameNumber);

      argumentTable->setAddress(vertexBuffer->gpuAddress(), InputBufferIndexForVertexData);
      argumentTable->setAddress(viewportSizeBuffer->gpuAddress(), InputBufferIndexForViewportSize);

      renderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, 3);
      renderEncoder->endEncoding();

      commandBuffer->endCommandBuffer();

      commandQueue->wait(drawable);
      MTL4::CommandBuffer *commandBuffers[] = {commandBuffer};
      commandQueue->commit(commandBuffers, 1);
      commandQueue->signalDrawable(drawable);

      drawable->present();

      commandQueue->signalEvent(sharedEvent, frameNumber); 

      insidePool->release();
   }

   pool->release();
   
   SDL_Metal_DestroyView(sdlView);
   SDL_DestroyWindow(mainWindow);
   SDL_Quit();

   return 0;
}
