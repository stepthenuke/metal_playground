#ifndef ShaderTypes_h
#define ShaderTypes_h

#include <simd/simd.h>
typedef enum InputBufferIndex
{
    InputBufferIndexForVertexData = 0,
    InputBufferIndexForViewportSize = 1,
} InputBufferIndex;

typedef struct
{
    simd_float2 position;
    simd_float4 color;
} VertexData;

#endif /* ShaderTypes_h */
