#include "Assets/Shaders/Common.hlsl"

ConstantBuffer<PassConstants> PassConstantBuffer : register(b0, perPassSpace);
ConstantBuffer<PerObjectConstants> ObjectConstantBuffer : register(b1);

struct Interpolators
{
	float4 position : SV_POSITION;
	float3 positionWS : WORLD_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
};

Interpolators VertexShader(uint vertexId : SV_VertexID)
{
	ByteAddressBuffer positionBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.positionBufferIndex];
	ByteAddressBuffer normalBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.normalBufferIndex];
	ByteAddressBuffer uvBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.uvBufferIndex];

	uint vertexIndex = vertexId + ObjectConstantBuffer.vertexOffset;
	float3 position = positionBuffer.Load<float3>(vertexIndex * sizeof(float3));
	float3 normal = normalBuffer.Load<float3>(vertexIndex * sizeof(float3));
	float2 uv = uvBuffer.Load<float2>(vertexIndex * sizeof(float2));

	Interpolators output;
	output.positionWS = mul(ObjectConstantBuffer.worldMatrix, float4(position, 1.0)).xyz;
	output.position = mul(PassConstantBuffer.viewMatrix, float4(output.positionWS, 1.0));
	output.position = mul(PassConstantBuffer.projectionMatrix, output.position);
	output.normal = normal;
	output.uv = uv;

	return output;
}

float4 PixelShader(Interpolators input) : SV_TARGET
{
	return float4(input.normal * 0.5 + 0.5, 1.0);
}