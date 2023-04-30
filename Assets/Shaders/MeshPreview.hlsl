#include "Assets/Shaders/Common.hlsl"

ConstantBuffer<PassConstants> PassConstantBuffer : register(b0, perPassSpace);
ConstantBuffer<PerDrawConstants> DrawConstantBuffer : register(b1);

struct Interpolators
{
	float4 position : SV_POSITION;
	float3 positionWS : WORLD_POSITION;
	float3 normal : NORMAL;
};

Interpolators VertexShader(uint vertexId : SV_VertexID)
{
	ByteAddressBuffer positionBuffer = ResourceDescriptorHeap[PassConstantBuffer.positionBufferIndex];
	ByteAddressBuffer normalBuffer = ResourceDescriptorHeap[PassConstantBuffer.normalBufferIndex];

	uint vertexIndex = vertexId + DrawConstantBuffer.vertexOffset;
	float3 position = positionBuffer.Load<float3>(vertexIndex * sizeof(float3));
	float3 normal = normalBuffer.Load<float3>(vertexIndex * sizeof(float3));

	Interpolators output;
	output.positionWS = mul(DrawConstantBuffer.worldMatrix, float4(position, 1.0)).xyz;
	output.position = mul(PassConstantBuffer.viewMatrix, float4(output.positionWS, 1.0));
	output.position = mul(PassConstantBuffer.projectionMatrix, output.position);
	output.normal = normal;

	return output;
}

float4 PixelShader(Interpolators input) : SV_TARGET
{
	return float4(input.normal * 0.5 + 0.5, 1.0);
}