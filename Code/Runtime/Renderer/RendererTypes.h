#pragma once

#include <DirectXMath.h>
#include <memory>

#define RAD_TO_DEG(rad) (rad * 180.0f / 3.14159265f)
#define DEG_TO_RAD(deg) (deg * 3.14159265f / 180.0f)

namespace D3D12Lite
{
	struct BufferResource;
}

namespace Styx
{
	struct Camera
	{
		DirectX::XMVECTOR position = DirectX::XMVectorSet(75.0f, 55.0f, -85.0f, 0.0f);
		DirectX::XMVECTOR target;
		DirectX::XMVECTOR up;
		DirectX::XMVECTOR forward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		DirectX::XMVECTOR right = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		DirectX::XMMATRIX transform;
		DirectX::XMMATRIX view;
		float yaw = DEG_TO_RAD(-40.0f);
		float pitch = DEG_TO_RAD(35.0f);
		float movementSpeed = 5.0f;
	};

	struct Transform
	{
		DirectX::XMFLOAT4X4 worldMatrix;
	};

	struct Mesh
	{
		char name[256];
		uint32_t vertexCount;
		uint32_t vertexOffset;
		uint32_t indexCount;
		uint32_t indexOffset;

		std::unique_ptr<D3D12Lite::BufferResource> positionBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> normalBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> tangentBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> uvBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> indexBuffer;
	};
}
