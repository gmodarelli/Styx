#include "Model.h"
#include <../RHI/D3D12Lite.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>

namespace Styx
{
	void Scene::Initialize(const char* path)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			printf("[Main] Failed to load model at '%s'. Error: %s\n", path, importer.GetErrorString());
			return;
		}

		ProcessNode(scene->mRootNode, scene);
	}

	void Scene::Shutdown()
	{
		for (uint32_t modelIndex = 0; modelIndex < m_Models.size(); modelIndex++)
		{
			m_Models[modelIndex]->Destroy(m_Device);
		}
	}

	void Scene::ProcessNode(aiNode* node, const aiScene* scene)
	{
		std::unique_ptr<Model> model = std::make_unique<Model>();

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			DirectX::XMMATRIX m = DirectX::XMMatrixSet(
				node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
				node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
				node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
				node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4);

			Transform transform;
			DirectX::XMStoreFloat4x4(&transform.worldMatrix, m);
			model->transforms.push_back(transform);

			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			model->meshes.push_back(std::make_unique<Mesh>(m_Device, mesh, scene));
		}

		m_Models.push_back(std::move(model));

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene);
		}
	}

	Mesh::Mesh(D3D12Lite::Device* device, aiMesh* mesh, const aiScene* scene)
	{
		assert(mesh->HasPositions());
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<float> uvs;
		std::vector<uint32_t> indices;

		indexOffset = (uint32_t)indices.size();
		indexCount = 0;
		vertexOffset = (uint32_t)positions.size() / 3;
		vertexCount = mesh->mNumVertices;

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			positions.push_back(mesh->mVertices[i].x);
			positions.push_back(mesh->mVertices[i].y);
			positions.push_back(mesh->mVertices[i].z);

			normals.push_back(mesh->mNormals[i].x);
			normals.push_back(mesh->mNormals[i].y);
			normals.push_back(mesh->mNormals[i].z);

			uvs.push_back(mesh->mTextureCoords[0][i].x);
			uvs.push_back(mesh->mTextureCoords[0][i].y);
		}

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			indexCount += face.mNumIndices;

			for (uint32_t j = 0; j < face.mNumIndices; j++)
			{
				indices.push_back(face.mIndices[j]);
			}
		}

		assert(positions.size() > 0);
		assert(normals.size() > 0);
		assert(uvs.size() > 0);
		assert(positions.size() == normals.size());
		assert(indices.size() > 0);

		{
			uint32_t sizeInBytes = static_cast<uint32_t>(positions.size() * sizeof(float));
			D3D12Lite::BufferCreationDesc desc{};
			desc.mSize = sizeInBytes;
			desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
			desc.mViewFlags = D3D12Lite::BufferViewFlags::srv;
			desc.mStride = sizeof(float) * 3;
			desc.mIsRawAccess = true;
			desc.mDebugName = L"Position Buffer";

			positionBuffer = device->CreateBuffer(desc);

			std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
			uploadBuffer->mBuffer = positionBuffer.get();
			uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
			uploadBuffer->mBufferDataSize = sizeInBytes;

			memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, positions.data(), sizeInBytes);
			device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
		}

		{
			uint32_t sizeInBytes = static_cast<uint32_t>(normals.size() * sizeof(float));
			D3D12Lite::BufferCreationDesc desc{};
			desc.mSize = sizeInBytes;
			desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
			desc.mViewFlags = D3D12Lite::BufferViewFlags::srv;
			desc.mStride = sizeof(float) * 3;
			desc.mIsRawAccess = true;
			desc.mDebugName = L"Normal Buffer";

			normalBuffer = device->CreateBuffer(desc);

			std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
			uploadBuffer->mBuffer = normalBuffer.get();
			uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
			uploadBuffer->mBufferDataSize = sizeInBytes;

			memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, normals.data(), sizeInBytes);
			device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
		}

		{
			uint32_t sizeInBytes = static_cast<uint32_t>(uvs.size() * sizeof(float));
			D3D12Lite::BufferCreationDesc desc{};
			desc.mSize = sizeInBytes;
			desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
			desc.mViewFlags = D3D12Lite::BufferViewFlags::srv;
			desc.mStride = sizeof(float) * 2;
			desc.mIsRawAccess = true;
			desc.mDebugName = L"UV Buffer";

			uvBuffer = device->CreateBuffer(desc);

			std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
			uploadBuffer->mBuffer = uvBuffer.get();
			uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
			uploadBuffer->mBufferDataSize = sizeInBytes;

			memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, uvs.data(), sizeInBytes);
			device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
		}

		{
			uint32_t sizeInBytes = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
			D3D12Lite::BufferCreationDesc desc{};
			desc.mSize = sizeInBytes;
			desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
			desc.mViewFlags = D3D12Lite::BufferViewFlags::none;
			desc.mStride = sizeof(uint32_t);
			desc.mIsRawAccess = false;
			desc.mFormat = DXGI_FORMAT_R32_UINT;
			desc.mDebugName = L"Index Buffer";

			indexBuffer = device->CreateBuffer(desc);

			std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
			uploadBuffer->mBuffer = indexBuffer.get();
			uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
			uploadBuffer->mBufferDataSize = sizeInBytes;

			memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, indices.data(), sizeInBytes);
			device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
		}
	}
	void Model::Destroy(D3D12Lite::Device* device)
	{
		for (uint32_t i = 0; i < meshes.size(); i++)
		{
			device->DestroyBuffer(std::move(meshes[i]->positionBuffer));
			device->DestroyBuffer(std::move(meshes[i]->normalBuffer));
			device->DestroyBuffer(std::move(meshes[i]->uvBuffer));
			device->DestroyBuffer(std::move(meshes[i]->indexBuffer));
		}
	}
}
