#pragma once

#include "RendererTypes.h"
#include "RHI/D3D12Lite.h"

#include <array>
#include <memory>

namespace Styx
{
	// NOTE: This should be a pass
	class TerrainRenderer
	{
	public:
		TerrainRenderer(D3D12Lite::Device* device) : m_Device(device) {}
		~TerrainRenderer() = default;

		void Initialize(const char* terrainPlanePath, D3D12Lite::PipelineResourceSpace* perPassResources);
		void Shutdown();

		void Render(D3D12Lite::GraphicsContext* gfx, D3D12Lite::PipelineResourceSpace* passResources, D3D12Lite::TextureResource* rt0, D3D12Lite::TextureResource* depthBuffer);

	private:
		D3D12Lite::Device* m_Device;

		Mesh m_Mesh;
		Transform m_Transform;

		std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> m_ObjectConstantBuffers;
		D3D12Lite::PipelineResourceSpace m_PerObjectResourceSpace;
		std::unique_ptr<D3D12Lite::Shader> m_VertexShader;
		std::unique_ptr<D3D12Lite::Shader> m_PixelShader;
		std::unique_ptr<D3D12Lite::PipelineStateObject> m_TerrainPSO;
	};
}
