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
	class GraphicsContext;
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

	struct Model
	{
		char name[256];
		std::vector<std::unique_ptr<Mesh>> meshes;
		std::vector<Transform> transforms;

		void Destroy(D3D12Lite::Device* device);

		std::vector<Model*> m_Children;
	};

	class Scene
	{
	public:
		Scene(D3D12Lite::Device* device) : m_Device(device), m_Root(nullptr) {};
		~Scene() = default;

		void Initialize(const char* path);
		void Shutdown();

		void Render(D3D12Lite::GraphicsContext* gfx);

	private:
		void DrawModel(D3D12Lite::GraphicsContext* gfx, Model* model);
		void ProcessNode(aiNode* node, const aiScene* scene, Model* parent);

	public:
		Model* m_Root;

	private:
		D3D12Lite::Device* m_Device;
	};
}
