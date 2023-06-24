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
#include <Renderer/Model.h>

#include <memory>
#include <vector>
#include <array>

#include <stdio.h>

using namespace Styx;

// GPU resources
DXGI_FORMAT g_depthFormat = DXGI_FORMAT_D32_FLOAT;
std::unique_ptr<D3D12Lite::TextureResource> g_depthBuffer;
std::array<std::unique_ptr<D3D12Lite::BufferResource>, D3D12Lite::NUM_FRAMES_IN_FLIGHT> g_passConstantBuffers;
D3D12Lite::PipelineResourceSpace g_perPassResourceSpace;
std::unique_ptr<D3D12Lite::Shader> g_vertexShader;
std::unique_ptr<D3D12Lite::Shader> g_pixelShader;
std::unique_ptr<D3D12Lite::PipelineStateObject> g_meshPreviewPSO;

struct PassConstants
{
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
};

DirectX::XMVECTOR g_worldForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
DirectX::XMVECTOR g_worldRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

// Cameras
struct Camera
{
	DirectX::XMVECTOR position;
	DirectX::XMVECTOR target;
	DirectX::XMVECTOR up;
	DirectX::XMVECTOR forward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	DirectX::XMVECTOR right = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMMATRIX transform;
	DirectX::XMMATRIX view;
	float yaw = 0.0f;
	float pitch = 0.0f;
};

Camera g_freeFlyCamera;
void updateFreeFlyCamera();

// Input Axis Mapping
float g_inputForwardAxis = 0.0f;
float g_inputRightAxis = 0.0f;
float g_inputUpAxis = 0.0f;

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

	std::vector<Mesh> meshes;
	std::vector<Transform> transforms;
	Scene::LoadScene(device.get(), "Assets/Models/NewSponza_Main_glTF_002.gltf", meshes, transforms);

	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), screenSize.x / (float)screenSize.y, 0.01f, 100.0f);

	D3D12Lite::BufferCreationDesc passConstantBufferDesc{};
	passConstantBufferDesc.mSize = sizeof(PassConstants);
	passConstantBufferDesc.mAccessFlags = D3D12Lite::BufferAccessFlags::hostWritable;
	passConstantBufferDesc.mViewFlags = D3D12Lite::BufferViewFlags::cbv;
	passConstantBufferDesc.mDebugName = L"Pass Constant Buffer";

	for (uint32_t i = 0; i < D3D12Lite::NUM_FRAMES_IN_FLIGHT; i++)
	{
		g_passConstantBuffers[i] = device->CreateBuffer(passConstantBufferDesc);
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
		// TODO: Make this a struct
		resourceLayout.mNum32BitConstants = 16 + 4;

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

		// Update
		{
			// Camera input movememnt
			// W
			if (Window::GetKey(1))
			{
				g_inputForwardAxis += 5.0f * 0.001f;
			}
			// S
			if (Window::GetKey(4))
			{
				g_inputForwardAxis -= 5.0f * 0.001f;
			}
			// A
			if (Window::GetKey(3))
			{
				g_inputRightAxis -= 5.0f * 0.001f;
			}
			// D
			if (Window::GetKey(5))
			{
				g_inputRightAxis += 5.0f * 0.001f;
			}
			// Q
			if (Window::GetKey(0))
			{
				g_inputUpAxis += 5.0f * 0.001f;
			}
			// E
			if (Window::GetKey(2))
			{
				g_inputUpAxis -= 5.0f * 0.001f;
			}

			updateFreeFlyCamera();
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

			D3D12Lite::PipelineInfo pso;
			pso.mPipeline = g_meshPreviewPSO.get();
			pso.mRenderTargets.push_back(&backBuffer);
			pso.mDepthStencilTarget = g_depthBuffer.get();

			PassConstants passConstants;
			DirectX::XMStoreFloat4x4(&passConstants.viewMatrix, g_freeFlyCamera.view);
			DirectX::XMStoreFloat4x4(&passConstants.projectionMatrix, projectionMatrix);
			g_passConstantBuffers[device->GetFrameId()]->SetMappedData(&passConstants, sizeof(PassConstants));

			graphicsContext->SetPipeline(pso);
			graphicsContext->SetPipelineResources(D3D12Lite::PER_PASS_SPACE, g_perPassResourceSpace);
			graphicsContext->SetDefaultViewPortAndScissor(device->GetScreenSize());

			for (uint32_t i = 0; i < meshes.size(); i++)
			{
				if (meshes[i].positionBuffer->mIsReady && meshes[i].normalBuffer->mIsReady && meshes[i].uvBuffer->mIsReady && meshes[i].indexBuffer->mIsReady)
				{
					graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					graphicsContext->SetIndexBuffer(*meshes[i].indexBuffer);

					graphicsContext->SetPipeline32BitConstants(1, 16, transforms[i].worldMatrix.m, 0);
					graphicsContext->SetPipeline32BitConstant(1, meshes[i].vertexOffset, 16);
					graphicsContext->SetPipeline32BitConstant(1, meshes[i].positionBuffer->mDescriptorHeapIndex, 17);
					graphicsContext->SetPipeline32BitConstant(1, meshes[i].normalBuffer->mDescriptorHeapIndex, 18);
					graphicsContext->SetPipeline32BitConstant(1, meshes[i].uvBuffer->mDescriptorHeapIndex, 19);

					graphicsContext->DrawIndexed(meshes[i].indexCount, meshes[i].indexOffset, 0);
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

	for (uint32_t i = 0; i < meshes.size(); i++)
	{
		if (meshes[i].positionBuffer->mIsReady && meshes[i].normalBuffer->mIsReady && meshes[i].uvBuffer->mIsReady && meshes[i].indexBuffer->mIsReady)
		{
			device->DestroyBuffer(std::move(meshes[i].positionBuffer));
			device->DestroyBuffer(std::move(meshes[i].normalBuffer));
			device->DestroyBuffer(std::move(meshes[i].uvBuffer));
			device->DestroyBuffer(std::move(meshes[i].indexBuffer));
		}
	}

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

void updateFreeFlyCamera()
{
	g_freeFlyCamera.transform = DirectX::XMMatrixRotationRollPitchYaw(g_freeFlyCamera.pitch, g_freeFlyCamera.yaw, 0);
	g_freeFlyCamera.target = DirectX::XMVector3TransformCoord(g_worldForward, g_freeFlyCamera.transform);
	g_freeFlyCamera.target = DirectX::XMVector3Normalize(g_freeFlyCamera.target);

	g_freeFlyCamera.right = DirectX::XMVector3TransformCoord(g_worldRight, g_freeFlyCamera.transform);
	g_freeFlyCamera.forward = DirectX::XMVector3TransformCoord(g_worldForward, g_freeFlyCamera.transform);
	g_freeFlyCamera.up = DirectX::XMVector3Cross(g_freeFlyCamera.forward, g_freeFlyCamera.right);

	g_freeFlyCamera.position = DirectX::XMVectorAdd(g_freeFlyCamera.position, DirectX::XMVectorMultiply(DirectX::XMVectorSet(g_inputRightAxis, g_inputRightAxis, g_inputRightAxis, 0.0f), g_freeFlyCamera.right));
	g_freeFlyCamera.position = DirectX::XMVectorAdd(g_freeFlyCamera.position, DirectX::XMVectorMultiply(DirectX::XMVectorSet(g_inputForwardAxis, g_inputForwardAxis, g_inputForwardAxis, 0.0f), g_freeFlyCamera.forward));
	g_freeFlyCamera.position = DirectX::XMVectorAdd(g_freeFlyCamera.position, DirectX::XMVectorMultiply(DirectX::XMVectorSet(g_inputUpAxis, g_inputUpAxis, g_inputUpAxis, 0.0f), g_freeFlyCamera.up));

	g_inputForwardAxis = 0.0f;
	g_inputRightAxis = 0.0f;
	g_inputUpAxis = 0.0f;

	g_freeFlyCamera.target = DirectX::XMVectorAdd(g_freeFlyCamera.position, g_freeFlyCamera.target);
	g_freeFlyCamera.view = DirectX::XMMatrixLookAtLH(g_freeFlyCamera.position, g_freeFlyCamera.target, g_freeFlyCamera.up);
}
