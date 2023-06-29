#include "TerrainRenderer.h"
#include "RHI/D3D12Lite.h"
#include "Model.h"

#include <DirectXMath.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>

namespace
{
	struct PassConstants
	{
		DirectX::XMFLOAT4X4 viewMatrix;
		DirectX::XMFLOAT4X4 projectionMatrix;
	};

	struct ObjectConstants
	{
		float worldMatrix[4][4];
		uint32_t vertexOffset;
		uint32_t positionBufferIndex;
		uint32_t normalBufferIndex;
		uint32_t tangentBufferIndex;
		uint32_t uvBufferIndex;
	};
}

void Styx::TerrainRenderer::Initialize()
{
	LoadResources();
	InitializePSOs();
}

void Styx::TerrainRenderer::Shutdown()
{
	m_Device->DestroyPipelineStateObject(std::move(m_TerrainPSO));
	m_Device->DestroyShader(std::move(m_VertexShader));
	m_Device->DestroyShader(std::move(m_PixelShader));

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_Device->DestroyBuffer(std::move(m_PassConstantBuffers[i]));
		m_Device->DestroyBuffer(std::move(m_ObjectConstantBuffers[i]));
	}

	m_Device->DestroyBuffer(std::move(m_Mesh.positionBuffer));
	m_Device->DestroyBuffer(std::move(m_Mesh.normalBuffer));
	m_Device->DestroyBuffer(std::move(m_Mesh.tangentBuffer));
	m_Device->DestroyBuffer(std::move(m_Mesh.uvBuffer));
	m_Device->DestroyBuffer(std::move(m_Mesh.indexBuffer));
}

void Styx::TerrainRenderer::Render(D3D12Lite::GraphicsContext* gfx, Camera& camera, D3D12Lite::TextureResource* rt0, D3D12Lite::TextureResource* depthBuffer)
{
	D3D12Lite::PipelineInfo pso;
	pso.mPipeline = m_TerrainPSO.get();
	pso.mRenderTargets.push_back(rt0);
	pso.mDepthStencilTarget = depthBuffer;

	PassConstants passConstants;
	DirectX::XMStoreFloat4x4(&passConstants.viewMatrix, camera.view);
	DirectX::XMStoreFloat4x4(&passConstants.projectionMatrix, camera.projection);
	m_PassConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&passConstants, sizeof(PassConstants));

	ObjectConstants objectConstants;
	memcpy_s(&objectConstants.worldMatrix, sizeof(float[4][4]), m_Transform.worldMatrix.m, sizeof(float[4][4]));
	objectConstants.vertexOffset = m_Mesh.vertexOffset;
	objectConstants.positionBufferIndex = m_Mesh.positionBuffer->mDescriptorHeapIndex;
	objectConstants.normalBufferIndex = m_Mesh.normalBuffer->mDescriptorHeapIndex;
	objectConstants.tangentBufferIndex = m_Mesh.tangentBuffer->mDescriptorHeapIndex;
	objectConstants.uvBufferIndex = m_Mesh.uvBuffer->mDescriptorHeapIndex;
	m_ObjectConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&objectConstants, sizeof(ObjectConstants));

	gfx->SetPipeline(pso);
	gfx->SetPipelineResources(D3D12Lite::PER_PASS_SPACE, m_PerPassResourceSpace);
	gfx->SetPipelineResources(D3D12Lite::PER_OBJECT_SPACE, m_PerObjectResourceSpace);
	gfx->SetDefaultViewPortAndScissor(m_Device->GetScreenSize());
	gfx->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfx->SetIndexBuffer(*m_Mesh.indexBuffer);

	gfx->DrawIndexed(m_Mesh.indexCount, m_Mesh.indexOffset, 0);
}

void Styx::TerrainRenderer::LoadResources()
{
	Assimp::Importer importer;
	const char* terrainPlanePath = "Assets/Models/TerrainPlane.gltf";
	const aiScene* scene = importer.ReadFile(terrainPlanePath, aiProcess_Triangulate | aiProcess_CalcTangentSpace);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		printf("[TerrainRenderer::LoadResources] Failed to load model at '%s'. Error: %s\n", terrainPlanePath, importer.GetErrorString());
		return;
	}

	aiNode* node = scene->mRootNode;
	assert(node->mNumMeshes == 1);
	DirectX::XMMATRIX m = DirectX::XMMatrixSet(
		node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
		node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
		node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
		node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4);

	DirectX::XMStoreFloat4x4(&m_Transform.worldMatrix, m);

	aiMesh* mesh = scene->mMeshes[node->mMeshes[0]];
	m_Mesh = Scene::ProcessMesh(m_Device, mesh, scene);

}

void Styx::TerrainRenderer::InitializePSOs()
{
	D3D12Lite::BufferCreationDesc passConstantBufferDesc{};
	passConstantBufferDesc.mSize = sizeof(PassConstants);
	passConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	passConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	passConstantBufferDesc.mDebugName = L"TerrainRenderer::PassConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_PassConstantBuffers[i] = m_Device->CreateBuffer(passConstantBufferDesc);
	}

	D3D12Lite::BufferCreationDesc objectConstantBufferDesc{};
	objectConstantBufferDesc.mSize = sizeof(ObjectConstants);
	objectConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	objectConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	objectConstantBufferDesc.mDebugName = L"TerrainRenderer::ObjectConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_ObjectConstantBuffers[i] = m_Device->CreateBuffer(objectConstantBufferDesc);
	}

	D3D12Lite::ShaderCreationDesc vsDesc{};
	vsDesc.mShaderName = L"Terrain.hlsl";
	vsDesc.mEntryPoint = L"VertexShader";
	vsDesc.mType = D3D12Lite::ShaderType::vertex;

	D3D12Lite::ShaderCreationDesc psDesc{};
	psDesc.mShaderName = L"Terrain.hlsl";
	psDesc.mEntryPoint = L"PixelShader";
	psDesc.mType = D3D12Lite::ShaderType::pixel;

	m_VertexShader = m_Device->CreateShader(vsDesc);
	m_PixelShader = m_Device->CreateShader(psDesc);

	D3D12Lite::GraphicsPipelineDesc psoDesc = D3D12Lite::GetDefaultGraphicsPipelineDesc();
	psoDesc.mVertexShader = m_VertexShader.get();
	psoDesc.mPixelShader = m_PixelShader.get();
	psoDesc.mRenderTargetDesc.mNumRenderTargets = 1;
	psoDesc.mRenderTargetDesc.mRenderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.mDepthStencilDesc.DepthEnable = true;
	psoDesc.mRenderTargetDesc.mDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.mDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	m_PerPassResourceSpace.SetCBV(m_PassConstantBuffers[0].get());
	m_PerPassResourceSpace.Lock();

	m_PerObjectResourceSpace.SetCBV(m_ObjectConstantBuffers[0].get());
	m_PerObjectResourceSpace.Lock();

	D3D12Lite::PipelineResourceLayout resourceLayout;
	resourceLayout.mSpaces[D3D12Lite::PER_PASS_SPACE] = &m_PerPassResourceSpace;
	resourceLayout.mSpaces[D3D12Lite::PER_OBJECT_SPACE] = &m_PerObjectResourceSpace;

	m_TerrainPSO = m_Device->CreateGraphicsPipeline(psoDesc, resourceLayout);
}
