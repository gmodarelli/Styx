#pragma once

#include "RendererTypes.h"
#include "RHI/D3D12Lite.h"

#include <array>
#include <memory>

namespace Styx
{
	struct HeightfieldNoiseMaterialConstants
	{
		int32_t seed = 42;
		float frequency = 0.01f;
		int32_t octaves = 3;
		float lacunarity = 2.0f;
		float gain = 0.5f;
	};

	// NOTE: This should be a pass
	class TerrainRenderer
	{
	public:
		TerrainRenderer(D3D12Lite::Device* device) : m_Device(device) {}
		~TerrainRenderer() = default;

		void Initialize();
		void Shutdown();

		void Render(D3D12Lite::GraphicsContext* gfx, D3D12Lite::ComputeContext* compute, Camera& camera, D3D12Lite::TextureResource* rt0, D3D12Lite::TextureResource* depthBuffer);
		void RenderUI();

	private:
		void LoadResources();
		void InitializePSOs();

	public:
		HeightfieldNoiseMaterialConstants m_MaterialConstants;

	private:
		D3D12Lite::Device* m_Device;

		Mesh m_Mesh;
		Transform m_Transform;

		std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> m_PassConstantBuffers;
		std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> m_ObjectConstantBuffers;
		std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> m_MaterialConstantBuffers;
		D3D12Lite::PipelineResourceSpace m_PerPassResourceSpace;
		D3D12Lite::PipelineResourceSpace m_PerObjectResourceSpace;
		D3D12Lite::PipelineResourceSpace m_PerMaterialResourceSpace;
		std::unique_ptr<D3D12Lite::Shader> m_VertexShader;
		std::unique_ptr<D3D12Lite::Shader> m_PixelShader;
		std::unique_ptr<D3D12Lite::PipelineStateObject> m_TerrainPSO;

		std::unique_ptr<D3D12Lite::Shader> m_HeightfieldNoiseShader;
		std::unique_ptr<D3D12Lite::PipelineStateObject> m_HeightfieldNoisePSO;
		std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> m_HeightfieldNoiseObjectConstantBuffers;
		std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> m_HeightfieldNoiseMaterialConstantBuffers;
		D3D12Lite::PipelineResourceSpace m_HeightfieldNoisePerObjectResourceSpace;
		D3D12Lite::PipelineResourceSpace m_HeightfieldNoisePerMaterialResourceSpace;
		std::unique_ptr<D3D12Lite::TextureResource> m_HeightfieldTexture;
	};
}
