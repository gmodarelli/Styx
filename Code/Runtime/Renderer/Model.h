#pragma once

#include "RendererTypes.h"

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
	struct Model
	{
		char name[256];
		std::vector<Mesh> meshes;
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

	public:
		static Mesh ProcessMesh(D3D12Lite::Device* device, aiMesh* mesh, const aiScene* scene);

	private:
		void DrawModel(D3D12Lite::GraphicsContext* gfx, Model* model);
		void ProcessNode(aiNode* node, const aiScene* scene, Model* parent);

	public:
		Model* m_Root;

	private:
		D3D12Lite::Device* m_Device;
	};
}
