#pragma once

#include <stdint.h>
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

namespace D3D12Lite
{
	class Device;
	struct BufferResource;
}

namespace Styx
{
	struct Transform
	{
		DirectX::XMFLOAT4X4 worldMatrix;
	};

	struct Mesh
	{
		uint32_t vertexCount;
		uint32_t vertexOffset;
		uint32_t indexCount;
		uint32_t indexOffset;

		std::unique_ptr<D3D12Lite::BufferResource> positionBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> normalBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> uvBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> indexBuffer;
	};

	class Scene
	{
	public:
		static void LoadScene(D3D12Lite::Device* device, const char* path, std::vector<Mesh>& outMeshes, std::vector<Transform>& outTransforms);
	};
}
