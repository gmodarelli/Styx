#include "TerrainRenderer.h"
#include "RHI/D3D12Lite.h"
#include "Model.h"

#include <DirectXMath.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cassert>
#include <imgui/imgui.h>

namespace
{
	struct TerrainPassConstants
	{
		DirectX::XMFLOAT4X4 viewMatrix;
		DirectX::XMFLOAT4X4 projectionMatrix;
	};

	struct TerrainObjectConstants
	{
		float worldMatrix[4][4];
		uint32_t vertexOffset;
		uint32_t positionBufferIndex;
		uint32_t uvBufferIndex;
	};

	struct TerrainMaterialConstants
	{
		uint32_t heightmapIndex;
		float terrainTileSize;
		float terrainHeight;
	};

	struct HeightfieldNoiseObjectConstants
	{
		uint32_t heightfieldNoiseTextureWidth;
		uint32_t heightfieldNoiseTextureHeight;
		uint32_t heightfieldNoiseTextureIndex;
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
		m_Device->DestroyBuffer(std::move(m_MaterialConstantBuffers[i]));
		m_Device->DestroyBuffer(std::move(m_HeightfieldNoiseObjectConstantBuffers[i]));
		m_Device->DestroyBuffer(std::move(m_HeightfieldNoiseMaterialConstantBuffers[i]));
	}

	m_Device->DestroyBuffer(std::move(m_Mesh.positionBuffer));
	m_Device->DestroyBuffer(std::move(m_Mesh.uvBuffer));
	m_Device->DestroyBuffer(std::move(m_Mesh.indexBuffer));

	m_Device->DestroyPipelineStateObject(std::move(m_HeightfieldNoisePSO));
	m_Device->DestroyShader(std::move(m_HeightfieldNoiseShader));
	m_Device->DestroyTexture(std::move(m_HeightfieldTexture));
}

void Styx::TerrainRenderer::Render(D3D12Lite::GraphicsContext* gfx, D3D12Lite::ComputeContext* compute, Camera& camera, D3D12Lite::TextureResource* rt0, D3D12Lite::TextureResource* depthBuffer)
{
	// Render the heightfield noise
	{
		D3D12Lite::PipelineInfo pso;
		pso.mPipeline = m_HeightfieldNoisePSO.get();

		HeightfieldNoiseObjectConstants objectConstants;
		objectConstants.heightfieldNoiseTextureIndex = m_HeightfieldTexture->mDescriptorHeapIndex;
		objectConstants.heightfieldNoiseTextureWidth = 513;
		objectConstants.heightfieldNoiseTextureHeight = 513;
		m_HeightfieldNoiseObjectConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&objectConstants, sizeof(HeightfieldNoiseObjectConstants));

		m_HeightfieldNoiseMaterialConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&m_MaterialConstants, sizeof(HeightfieldNoiseMaterialConstants));

		compute->Reset();
		compute->AddBarrier(*m_HeightfieldTexture.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		compute->FlushBarriers();

		compute->SetPipeline(pso);
		compute->SetPipelineResources(D3D12Lite::PER_OBJECT_SPACE, m_HeightfieldNoisePerObjectResourceSpace);
		compute->SetPipelineResources(D3D12Lite::PER_MATERIAL_SPACE, m_HeightfieldNoisePerMaterialResourceSpace);
		compute->Dispatch((513 / 8) + 1, (513 / 8) + 1, 1);

		compute->AddBarrier(*m_HeightfieldTexture.get(), D3D12_RESOURCE_STATE_COMMON);
		compute->FlushBarriers();

		m_Device->SubmitContextWork(*compute);
	}

	// Render the terrain
	{
		D3D12Lite::PipelineInfo pso;
		pso.mPipeline = m_TerrainPSO.get();
		pso.mRenderTargets.push_back(rt0);
		pso.mDepthStencilTarget = depthBuffer;

		TerrainPassConstants passConstants;
		DirectX::XMStoreFloat4x4(&passConstants.viewMatrix, camera.view);
		DirectX::XMStoreFloat4x4(&passConstants.projectionMatrix, camera.projection);
		m_PassConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&passConstants, sizeof(TerrainPassConstants));

		TerrainObjectConstants objectConstants;
		memcpy_s(&objectConstants.worldMatrix, sizeof(float[4][4]), m_Transform.worldMatrix.m, sizeof(float[4][4]));
		objectConstants.vertexOffset = m_Mesh.vertexOffset;
		objectConstants.positionBufferIndex = m_Mesh.positionBuffer->mDescriptorHeapIndex;
		objectConstants.uvBufferIndex = m_Mesh.uvBuffer->mDescriptorHeapIndex;
		m_ObjectConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&objectConstants, sizeof(TerrainObjectConstants));

		TerrainMaterialConstants materialConstants;
		materialConstants.heightmapIndex = m_HeightfieldTexture->mDescriptorHeapIndex;
		materialConstants.terrainTileSize = 100.0f;
		materialConstants.terrainHeight = 5.0f;
		m_MaterialConstantBuffers[m_Device->GetFrameId()]->SetMappedData(&materialConstants, sizeof(TerrainMaterialConstants));

		gfx->Reset();

		gfx->AddBarrier(*m_HeightfieldTexture.get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		gfx->AddBarrier(*rt0, D3D12_RESOURCE_STATE_RENDER_TARGET);
		gfx->AddBarrier(*depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		gfx->FlushBarriers();

		float color[4] = {0.3f, 0.3f, 0.3f, 1.0f};
		gfx->ClearRenderTarget(*rt0, color);
		gfx->ClearDepthStencilTarget(*depthBuffer, 1.0f, 0);

		gfx->SetPipeline(pso);
		gfx->SetPipelineResources(D3D12Lite::PER_PASS_SPACE, m_PerPassResourceSpace);
		gfx->SetPipelineResources(D3D12Lite::PER_OBJECT_SPACE, m_PerObjectResourceSpace);
		gfx->SetPipelineResources(D3D12Lite::PER_MATERIAL_SPACE, m_PerMaterialResourceSpace);
		gfx->SetDefaultViewPortAndScissor(m_Device->GetScreenSize());
		gfx->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gfx->SetIndexBuffer(*m_Mesh.indexBuffer);

		gfx->DrawIndexed(m_Mesh.indexCount, m_Mesh.indexOffset, 0);

		gfx->AddBarrier(*m_HeightfieldTexture.get(), D3D12_RESOURCE_STATE_COMMON);
		gfx->FlushBarriers();
	}
}

void Styx::TerrainRenderer::RenderUI()
{
	ImGui::Begin("Heightfield Noise");
	{
		{
			ImGui::InputInt("Seed", &m_MaterialConstants.seed);
			ImGui::InputFloat("Frequency", &m_MaterialConstants.frequency);
			ImGui::InputInt("Octaves", &m_MaterialConstants.octaves);
			ImGui::InputFloat("Lacunarity", &m_MaterialConstants.lacunarity);
			ImGui::InputFloat("Gain", &m_MaterialConstants.gain);
		}
	}
	ImGui::End();
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
	passConstantBufferDesc.mSize = sizeof(TerrainPassConstants);
	passConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	passConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	passConstantBufferDesc.mDebugName = L"TerrainRenderer::PassConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_PassConstantBuffers[i] = m_Device->CreateBuffer(passConstantBufferDesc);
	}

	D3D12Lite::BufferCreationDesc objectConstantBufferDesc{};
	objectConstantBufferDesc.mSize = sizeof(TerrainObjectConstants);
	objectConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	objectConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	objectConstantBufferDesc.mDebugName = L"TerrainRenderer::ObjectConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_ObjectConstantBuffers[i] = m_Device->CreateBuffer(objectConstantBufferDesc);
	}

	D3D12Lite::BufferCreationDesc materialConstantBufferDesc{};
	materialConstantBufferDesc.mSize = sizeof(TerrainMaterialConstants);
	materialConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	materialConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	materialConstantBufferDesc.mDebugName = L"TerrainRenderer::MaterialConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_MaterialConstantBuffers[i] = m_Device->CreateBuffer(materialConstantBufferDesc);
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

	m_PerMaterialResourceSpace.SetCBV(m_MaterialConstantBuffers[0].get());
	m_PerMaterialResourceSpace.Lock();

	D3D12Lite::PipelineResourceLayout resourceLayout;
	resourceLayout.mSpaces[D3D12Lite::PER_PASS_SPACE] = &m_PerPassResourceSpace;
	resourceLayout.mSpaces[D3D12Lite::PER_OBJECT_SPACE] = &m_PerObjectResourceSpace;
	resourceLayout.mSpaces[D3D12Lite::PER_MATERIAL_SPACE] = &m_PerMaterialResourceSpace;

	m_TerrainPSO = m_Device->CreateGraphicsPipeline(psoDesc, resourceLayout);

	D3D12Lite::TextureCreationDesc heightfieldCreationDesc{};
	heightfieldCreationDesc.mResourceDesc.Format = DXGI_FORMAT_R16_UNORM;
	heightfieldCreationDesc.mResourceDesc.Width = 513;
	heightfieldCreationDesc.mResourceDesc.Height = 513;
	heightfieldCreationDesc.mResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	heightfieldCreationDesc.mViewFlags = D3D12Lite::TextureViewFlags::uav | D3D12Lite::TextureViewFlags::srv;
	m_HeightfieldTexture = m_Device->CreateTexture(heightfieldCreationDesc);

	D3D12Lite::ShaderCreationDesc csDesc{};
	csDesc.mShaderName = L"HeightfieldNoise.hlsl";
	csDesc.mEntryPoint = L"HeightfieldNoise";
	csDesc.mType = D3D12Lite::ShaderType::compute;

	m_HeightfieldNoiseShader = m_Device->CreateShader(csDesc);

	D3D12Lite::BufferCreationDesc heightfieldNoiseObjectConstantBufferDesc{};
	heightfieldNoiseObjectConstantBufferDesc.mSize = sizeof(HeightfieldNoiseObjectConstants);
	heightfieldNoiseObjectConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	heightfieldNoiseObjectConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	heightfieldNoiseObjectConstantBufferDesc.mDebugName = L"TerrainRenderer::HeightfieldNoiseObjectConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_HeightfieldNoiseObjectConstantBuffers[i] = m_Device->CreateBuffer(heightfieldNoiseObjectConstantBufferDesc);
	}

	D3D12Lite::BufferCreationDesc heightfieldNoiseMaterialConstantBufferDesc{};
	heightfieldNoiseMaterialConstantBufferDesc.mSize = sizeof(HeightfieldNoiseMaterialConstants);
	heightfieldNoiseMaterialConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	heightfieldNoiseMaterialConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	heightfieldNoiseMaterialConstantBufferDesc.mDebugName = L"TerrainRenderer::HeightfieldNoiseMaterialConstantBuffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_HeightfieldNoiseMaterialConstantBuffers[i] = m_Device->CreateBuffer(heightfieldNoiseMaterialConstantBufferDesc);
	}

	D3D12Lite::ComputePipelineDesc cPsoDesc = { m_HeightfieldNoiseShader.get()};

	D3D12Lite::PipelineResourceLayout computeResourceLayout;
	computeResourceLayout.mSpaces[D3D12Lite::PER_OBJECT_SPACE] = &m_HeightfieldNoisePerObjectResourceSpace;
	computeResourceLayout.mSpaces[D3D12Lite::PER_MATERIAL_SPACE] = &m_HeightfieldNoisePerMaterialResourceSpace;

	m_HeightfieldNoisePerObjectResourceSpace.SetCBV(m_HeightfieldNoiseObjectConstantBuffers[0].get());
	m_HeightfieldNoisePerObjectResourceSpace.Lock();

	m_HeightfieldNoisePerMaterialResourceSpace.SetCBV(m_HeightfieldNoiseMaterialConstantBuffers[0].get());
	m_HeightfieldNoisePerMaterialResourceSpace.Lock();

	m_HeightfieldNoisePSO = m_Device->CreateComputePipeline(cPsoDesc, computeResourceLayout);
}
