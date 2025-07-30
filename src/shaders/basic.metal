#include <metal_stdlib>
using namespace metal;

#include "../common/shader_types.h"

struct RasterizerData
{
   float4 position [[position]];
   float4 color;
};

vertex RasterizerData
vertexShader(uint vertexID [[vertex_id]],
             constant VertexData *vertexData [[buffer(InputBufferIndexForVertexData)]],
             constant simd_uint2 *viewportSizePointer [[buffer(InputBufferIndexForViewportSize)]])
{
   RasterizerData out;
   
   simd_float2 pixelSpacePosition = vertexData[vertexID].position.xy;
   simd_float2 viewportSize = simd_float2(*viewportSizePointer);

   out.position.xy = pixelSpacePosition / (viewportSize / 2.0);
   out.position.z = 0.0;
   out.position.w = 1.0;
   
   out.color = vertexData[vertexID].color;

   return out;
}

fragment float4 fragmentShader(RasterizerData in [[stage_in]])
{
   return in.color;
}
