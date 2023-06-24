#ifndef __COMMON_HLSL_
#define __COMMON_HLSL_

// #include "ShaderInterop.h"

#define perObjectSpace		space0
#define perMaterialSpace	space1
#define perPassSpace		space2
#define perFrameSpace		space3

#define anisoClampSampler	0
#define anisoWrapSampler	1
#define linearClampSampler	2
#define linearWrapSampler	3
#define pointClampSampler	4
#define pointWrapSampler	5

struct PassConstants
{
	float4x4 viewMatrix;
	float4x4 projectionMatrix;
};

struct PerDrawConstants
{
	float4x4 worldMatrix;
	uint vertexOffset;
	uint positionBufferIndex;
	uint normalBufferIndex;
	uint uvBufferIndex;
};


#endif // __COMMON_HLSL_