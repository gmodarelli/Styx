/*
Copyright(c) 2023 Giuseppe Modarelli

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <Core/Window.h>
#include <RHI/D3D12Lite.h>

#include <memory>
#include <vector>
#include <array>

#include <stdio.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

struct Mesh
{
	uint32_t vertexCount;
	uint32_t vertexOffset;
	uint32_t indexCount;
	uint32_t indexOffset;
};

struct Transform
{
	DirectX::XMFLOAT4X4 worldMatrix;
};

std::vector<float> g_positions;
std::vector<float> g_normals;
std::vector<uint32_t> g_indices;
std::vector<Mesh> g_meshes;
std::vector<Transform> g_transforms;

using namespace Styx;

void loadScene(const char* path);
void processNode(aiNode* node, const aiScene* scene);
Mesh processMesh(aiMesh* mesh, const aiScene* scene);

// GPU resources
DXGI_FORMAT g_depthFormat = DXGI_FORMAT_D32_FLOAT;
std::unique_ptr<D3D12Lite::TextureResource> g_depthBuffer;
std::unique_ptr<D3D12Lite::BufferResource> g_positionBuffer;
std::unique_ptr<D3D12Lite::BufferResource> g_normalBuffer;
std::unique_ptr<D3D12Lite::BufferResource> g_indexBuffer;
std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> g_passConstantBuffers;
D3D12Lite::PipelineResourceSpace g_perPassResourceSpace;
std::unique_ptr<D3D12Lite::Shader> g_vertexShader;
std::unique_ptr<D3D12Lite::Shader> g_pixelShader;
std::unique_ptr<D3D12Lite::PipelineStateObject> g_meshPreviewPSO;

struct PassConstants
{
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
	uint32_t positionBufferIndex;
	uint32_t normalBufferIndex;
};

// NOTE(gmodarelli): This is all temporary test code to test the current WIP implementation
// of the RHI
int main()
{
	Window::Initialize();

	D3D12Lite::Uint2 screenSize(Window::GetWidth(), Window::GetHeight());
	std::unique_ptr<D3D12Lite::Device> device = std::make_unique<D3D12Lite::Device>(Window::GetWindowHandle(), screenSize);
	std::unique_ptr<D3D12Lite::GraphicsContext> graphicsContext = device->CreateGraphicsContext();

	// Create the depth buffer
	{
		D3D12Lite::TextureCreationDesc desc{};
		desc.mResourceDesc.Format = g_depthFormat;
		desc.mResourceDesc.Width = screenSize.x;
		desc.mResourceDesc.Height = screenSize.y;
		desc.mViewFlags = D3D12Lite::TextureViewFlags::srv | D3D12Lite::TextureViewFlags::dsv;

		g_depthBuffer = device->CreateTexture(desc);
	}

	// Testing the assimp installation
	loadScene("Assets/Models/NewSponza_Main_glTF_002.gltf");

	assert(g_positions.size() > 0);
	assert(g_normals.size() > 0);
	assert(g_positions.size() == g_normals.size());
	assert(g_indices.size() > 0);

	{
		uint32_t sizeInBytes = static_cast<uint32_t>(g_positions.size() * sizeof(float));
		D3D12Lite::BufferCreationDesc desc{};
		desc.mSize = sizeInBytes;
		desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
		desc.mViewFlags = D3D12Lite::BufferViewFlags::srv;
		desc.mStride = sizeof(float) * 3;
		desc.mIsRawAccess = true;
		desc.mDebugName = L"Position Buffer";

		g_positionBuffer = device->CreateBuffer(desc);

		std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
		uploadBuffer->mBuffer = g_positionBuffer.get();
		uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
		uploadBuffer->mBufferDataSize = sizeInBytes;

		memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, g_positions.data(), sizeInBytes);
		device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
	}

	{
		uint32_t sizeInBytes = static_cast<uint32_t>(g_normals.size() * sizeof(float));
		D3D12Lite::BufferCreationDesc desc{};
		desc.mSize = sizeInBytes;
		desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
		desc.mViewFlags = D3D12Lite::BufferViewFlags::srv;
		desc.mStride = sizeof(float) * 3;
		desc.mIsRawAccess = true;
		desc.mDebugName = L"Normal Buffer";

		g_normalBuffer = device->CreateBuffer(desc);

		std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
		uploadBuffer->mBuffer = g_normalBuffer.get();
		uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
		uploadBuffer->mBufferDataSize = sizeInBytes;

		memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, g_normals.data(), sizeInBytes);
		device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
	}

	{
		uint32_t sizeInBytes = static_cast<uint32_t>(g_indices.size() * sizeof(uint32_t));
		D3D12Lite::BufferCreationDesc desc{};
		desc.mSize = sizeInBytes;
		desc.mAccessFlags = D3D12Lite::BufferAccessFlags::gpuOnly;
		desc.mViewFlags = D3D12Lite::BufferViewFlags::none;
		desc.mStride = sizeof(uint32_t);
		desc.mIsRawAccess = false;
		desc.mFormat = DXGI_FORMAT_R32_UINT;
		desc.mDebugName = L"Index Buffer";

		g_indexBuffer = device->CreateBuffer(desc);

		std::unique_ptr<D3D12Lite::BufferUpload> uploadBuffer = std::make_unique<D3D12Lite::BufferUpload>();
		uploadBuffer->mBuffer = g_indexBuffer.get();
		uploadBuffer->mBufferData = std::make_unique<uint8_t[]>(sizeInBytes);
		uploadBuffer->mBufferDataSize = sizeInBytes;

		memcpy_s(uploadBuffer->mBufferData.get(), sizeInBytes, g_indices.data(), sizeInBytes);
		device->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(uploadBuffer));
	}

	// NOTE(gmodarelli): Temporarily faking a camera
	DirectX::XMVECTOR cameraPosition = DirectX::XMVectorSet(0.0, 2.0, 0.0, 0.0);
	DirectX::XMVECTOR cameraTarget = DirectX::XMVectorSet(-2.0, 2.0, 0.0, 0.0);
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(cameraPosition, cameraTarget, DirectX::XMVectorSet(0.0, 1.0, 0.0, 0.0));
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), screenSize.x / (float)screenSize.y, 0.01f, 100.0f);

	PassConstants passConstants;
	DirectX::XMStoreFloat4x4(&passConstants.viewMatrix, viewMatrix);
	DirectX::XMStoreFloat4x4(&passConstants.projectionMatrix, projectionMatrix);
	passConstants.positionBufferIndex = g_positionBuffer->mDescriptorHeapIndex;
	passConstants.normalBufferIndex = g_normalBuffer->mDescriptorHeapIndex;

	D3D12Lite::BufferCreationDesc passConstantBufferDesc{};
	passConstantBufferDesc.mSize = sizeof(PassConstants);
	passConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	passConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	passConstantBufferDesc.mDebugName = L"Pass Constant Buffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		g_passConstantBuffers[i] = device->CreateBuffer(passConstantBufferDesc);
		g_passConstantBuffers[i]->SetMappedData(&passConstants, sizeof(PassConstants));
	}

	// Mesh Preview PSO
	{
		D3D12Lite::ShaderCreationDesc vsDesc{};
		vsDesc.mShaderName = L"MeshPreview.hlsl";
		vsDesc.mEntryPoint = L"VertexShader";
		vsDesc.mType = D3D12Lite::ShaderType::vertex;

		D3D12Lite::ShaderCreationDesc psDesc{};
		psDesc.mShaderName = L"MeshPreview.hlsl";
		psDesc.mEntryPoint = L"PixelShader";
		psDesc.mType = D3D12Lite::ShaderType::pixel;

		g_vertexShader = device->CreateShader(vsDesc);
		g_pixelShader = device->CreateShader(psDesc);

		D3D12Lite::GraphicsPipelineDesc psoDesc = D3D12Lite::GetDefaultGraphicsPipelineDesc();
		psoDesc.mVertexShader = g_vertexShader.get();
		psoDesc.mPixelShader = g_pixelShader.get();
		psoDesc.mRenderTargetDesc.mNumRenderTargets = 1;
		psoDesc.mRenderTargetDesc.mRenderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		psoDesc.mDepthStencilDesc.DepthEnable = true;
		psoDesc.mRenderTargetDesc.mDepthStencilFormat = g_depthFormat;
		psoDesc.mDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		g_perPassResourceSpace.SetCBV(g_passConstantBuffers[0].get());
		g_perPassResourceSpace.Lock();

		D3D12Lite::PipelineResourceLayout resourceLayout;
		resourceLayout.mSpaces[D3D12Lite::PER_PASS_SPACE] = &g_perPassResourceSpace;
		resourceLayout.mNum32BitConstants = 16 + 1;

		g_meshPreviewPSO = device->CreateGraphicsPipeline(psoDesc, resourceLayout);
	}

	while (!Window::ShouldClose())
	{
		Window::Tick();

		D3D12Lite::Uint2 swapchainSize = device->GetScreenSize();
		if (swapchainSize.x != Window::GetWidth() || swapchainSize.y != Window::GetHeight())
		{
			if (device->ResizeSwapchain(Window::GetWindowHandle(), D3D12Lite::Uint2{ Window::GetWidth(), Window::GetHeight() }))
			{
				printf("[Main] The SwapChain has been resized to (%d x %d)\n", Window::GetWidth(), Window::GetHeight());
			}
		}

		// Render
		{
			device->BeginFrame();

			D3D12Lite::TextureResource& backBuffer = device->GetCurrentBackBuffer();

			graphicsContext->Reset();

			graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			graphicsContext->AddBarrier(*g_depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			graphicsContext->FlushBarriers();

			float color[4] = {0.3f, 0.3f, 0.8f, 1.0f};
			graphicsContext->ClearRenderTarget(backBuffer, color);
			graphicsContext->ClearDepthStencilTarget(*g_depthBuffer, 1.0f, 0);

			if (g_positionBuffer->mIsReady && g_normalBuffer->mIsReady && g_indexBuffer->mIsReady)
			{
				g_passConstantBuffers[device->GetFrameId()]->SetMappedData(&passConstants, sizeof(PassConstants));

				D3D12Lite::PipelineInfo pso;
				pso.mPipeline = g_meshPreviewPSO.get();
				pso.mRenderTargets.push_back(&backBuffer);
				pso.mDepthStencilTarget = g_depthBuffer.get();

				graphicsContext->SetPipeline(pso);
				graphicsContext->SetPipelineResources(D3D12Lite::PER_PASS_SPACE, g_perPassResourceSpace);
				graphicsContext->SetDefaultViewPortAndScissor(device->GetScreenSize());
				graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				graphicsContext->SetIndexBuffer(*g_indexBuffer);

				for (uint32_t i = 0; i < g_meshes.size(); i++)
				{
					graphicsContext->SetPipeline32BitConstants(1, 16, g_transforms[i].worldMatrix.m, 0);
					graphicsContext->SetPipeline32BitConstant(1, g_meshes[i].vertexOffset, 16);

					graphicsContext->DrawIndexed(g_meshes[i].indexCount, g_meshes[i].indexOffset, 0);
				}
			}

			graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
			graphicsContext->FlushBarriers();

			device->SubmitContextWork(*graphicsContext);

			device->EndFrame();
			device->Present();
		}
	}

	device->WaitForIdle();

	device->DestroyPipelineStateObject(std::move(g_meshPreviewPSO));
	device->DestroyShader(std::move(g_vertexShader));
	device->DestroyShader(std::move(g_pixelShader));
	device->DestroyBuffer(std::move(g_positionBuffer));
	device->DestroyBuffer(std::move(g_normalBuffer));
	device->DestroyBuffer(std::move(g_indexBuffer));

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		device->DestroyBuffer(std::move(g_passConstantBuffers[i]));
	}

	device->DestroyTexture(std::move(g_depthBuffer));

	device->DestroyContext(std::move(graphicsContext));
	device = nullptr;

	Window::Shutdown();

	return 0;
}

void loadScene(const char* path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		printf("[Main] Failed to load model at '%s'. Error: %s\n", path, importer.GetErrorString());
		return;
	}

	processNode(scene->mRootNode, scene);
}

void processNode(aiNode* node, const aiScene* scene)
{
	for (uint32_t i = 0; i < node->mNumMeshes; i++)
	{
		DirectX::XMMATRIX m = DirectX::XMMatrixSet(
			node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
			node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
			node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
			node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4);

		Transform transform;
		DirectX::XMStoreFloat4x4(&transform.worldMatrix, m);
		g_transforms.push_back(transform);

		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		g_meshes.push_back(processMesh(mesh, scene));
	}

	for (uint32_t i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

Mesh processMesh(aiMesh* mesh, const aiScene* scene)
{
	Mesh outMesh;
	outMesh.indexOffset = (uint32_t)g_indices.size();
	outMesh.indexCount = 0;
	outMesh.vertexOffset = (uint32_t)g_positions.size() / 3;
	outMesh.vertexCount = mesh->mNumVertices;

	for (uint32_t i = 0; i < mesh->mNumVertices; i++)
	{
		g_positions.push_back(mesh->mVertices[i].x);
		g_positions.push_back(mesh->mVertices[i].y);
		g_positions.push_back(mesh->mVertices[i].z);

		if (mesh->HasNormals())
		{
			g_normals.push_back(mesh->mNormals[i].x);
			g_normals.push_back(mesh->mNormals[i].y);
			g_normals.push_back(mesh->mNormals[i].z);
		}
		else
		{
			g_normals.push_back(0.0f);
			g_normals.push_back(1.0f);
			g_normals.push_back(0.0f);
		}
	}

	for (uint32_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		outMesh.indexCount += face.mNumIndices;

		for (uint32_t j = 0; j < face.mNumIndices; j++)
		{
			g_indices.push_back(face.mIndices[j]);
		}
	}

	return outMesh;
}
