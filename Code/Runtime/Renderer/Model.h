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

struct aiMesh;
struct aiNode;
struct aiScene;

namespace Styx
{
	struct Transform
	{
		DirectX::XMFLOAT4X4 worldMatrix;
	};

	class Mesh
	{
	public:
		Mesh(D3D12Lite::Device* device, aiMesh* mesh, const aiScene* scene);

		uint32_t vertexCount;
		uint32_t vertexOffset;
		uint32_t indexCount;
		uint32_t indexOffset;

		std::unique_ptr<D3D12Lite::BufferResource> positionBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> normalBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> uvBuffer;
		std::unique_ptr<D3D12Lite::BufferResource> indexBuffer;
	};

	struct Model
	{
		std::vector<std::unique_ptr<Mesh>> meshes;
		std::vector<Transform> transforms;

		void Destroy(D3D12Lite::Device* device);
	};

	class Scene
	{
	public:
		Scene(D3D12Lite::Device* device) : m_Device(device) {};
		~Scene() = default;

		void Initialize(const char* path);
		void Shutdown();

	private:
		void ProcessNode(aiNode* node, const aiScene* scene);

	public:
		std::vector<std::unique_ptr<Model>> m_Models;

	private:
		D3D12Lite::Device* m_Device;
	};
}
