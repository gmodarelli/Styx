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
#include <Renderer/TerrainRenderer.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_dx12.h>

#include <memory>
#include <array>

#include <stdio.h>

using namespace Styx;

// GPU resources
DXGI_FORMAT g_depthFormat = DXGI_FORMAT_D32_FLOAT;
std::unique_ptr<D3D12Lite::TextureResource> g_depthBuffer;

DirectX::XMVECTOR g_worldForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
DirectX::XMVECTOR g_worldRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

// Cameras
Camera g_freeFlyCamera;
float g_mouseSensitivity = 0.01f;
void updateFreeFlyCamera();

// Input Axis Mapping
float g_inputForwardAxis = 0.0f;
float g_inputRightAxis = 0.0f;
float g_inputUpAxis = 0.0f;

void ImGuiHierarchyForModel(Model* model, bool first)
{
	if (first)
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

	if (ImGui::TreeNode(model->name))
	{
		for (uint32_t i = 0; i < model->meshes.size(); i++)
		{
			ImGui::Text(model->meshes[i].name);
		}

		for (uint32_t i = 0; i < model->m_Children.size(); i++)
		{
			ImGuiHierarchyForModel(model->m_Children[i], false);
		}

		ImGui::TreePop();
	}
}

// NOTE(gmodarelli): This is all temporary test code to test the current WIP implementation
// of the RHI
int main()
{
	Window::Initialize();

	D3D12Lite::Uint2 screenSize(Window::GetWidth(), Window::GetHeight());
	std::unique_ptr<D3D12Lite::Device> device = std::make_unique<D3D12Lite::Device>(Window::GetWindowHandle(), screenSize);
	std::unique_ptr<D3D12Lite::GraphicsContext> graphicsContext = device->CreateGraphicsContext();
	std::unique_ptr<D3D12Lite::ComputeContext> computeContext = device->CreateComputeContext();

	// Create the depth buffer
	{
		D3D12Lite::TextureCreationDesc desc{};
		desc.mResourceDesc.Format = g_depthFormat;
		desc.mResourceDesc.Width = screenSize.x;
		desc.mResourceDesc.Height = screenSize.y;
		desc.mViewFlags = D3D12Lite::TextureViewFlags::srv | D3D12Lite::TextureViewFlags::dsv;

		g_depthBuffer = device->CreateTexture(desc);
	}

	g_freeFlyCamera.projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), screenSize.x / (float)screenSize.y, 0.01f, 1000.0f);

	TerrainRenderer terrainRenderer(device.get());
	terrainRenderer.Initialize();

	// ImGUI
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		ImGui::StyleColorsDark();

		D3D12Lite::Descriptor descriptor = device->GetImguiDescriptor(0);
		D3D12Lite::Descriptor descriptor2 = device->GetImguiDescriptor(1);

		ImGui_ImplSDL2_InitForD3D(static_cast<SDL_Window*>(Window::GetSDLWindow()));
		ImGui_ImplDX12_Init(device->GetDevice(), D3D12Lite::NUM_FRAMES_IN_FLIGHT, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, nullptr,
			descriptor.mCPUHandle, descriptor.mGPUHandle, descriptor2.mCPUHandle, descriptor2.mGPUHandle);
		Window::HackHackHack();
	}

	while (!Window::ShouldClose())
	{
		Window::Tick();
		float deltaTime = Window::GetDeltaTime();

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
			if (Window::GetKey(9)) // Right mouse button
			{
				// Mouse input movement
				uint32_t edge_padding = 5;
				float mouseX, mouseY;
				Window::GetMousePosition(&mouseX, &mouseY);
				if (mouseX >= Window::GetDisplayWidth() - edge_padding)
				{
					mouseX = static_cast<float>(edge_padding + 1);
					Window::SetMousePosition(mouseX, mouseY);
				}
				else if (mouseX <= edge_padding)
				{
					mouseX = static_cast<float>(Window::GetDisplayWidth() - edge_padding - 1);
					Window::SetMousePosition(mouseX, mouseY);
				}

				float mouseDeltaX, mouseDeltaY;
				Window::GetMouseDelta(&mouseDeltaX, &mouseDeltaY);
				g_freeFlyCamera.yaw += mouseDeltaX * g_mouseSensitivity;
				g_freeFlyCamera.pitch += mouseDeltaY * g_mouseSensitivity;

				float minYawRad = DirectX::XMConvertToRadians(-80.0f);
				float maxYawRad = DirectX::XMConvertToRadians(80.0f);
				g_freeFlyCamera.pitch = g_freeFlyCamera.pitch < minYawRad ? minYawRad : g_freeFlyCamera.pitch;
				g_freeFlyCamera.pitch = g_freeFlyCamera.pitch > maxYawRad ? maxYawRad : g_freeFlyCamera.pitch;

				// Camera input movement
				float movememntSpeed = g_freeFlyCamera.movementSpeed;
				if (Window::GetKey(6)) // Left Shift
				{
					movememntSpeed *= 2.0f;
				}

				// W
				if (Window::GetKey(1))
				{
					g_inputForwardAxis += movememntSpeed * deltaTime;
				}
				// S
				if (Window::GetKey(4))
				{
					g_inputForwardAxis -= movememntSpeed * deltaTime;
				}
				// A
				if (Window::GetKey(3))
				{
					g_inputRightAxis -= movememntSpeed * deltaTime;
				}
				// D
				if (Window::GetKey(5))
				{
					g_inputRightAxis += movememntSpeed * deltaTime;
				}
				// Q
				if (Window::GetKey(0))
				{
					g_inputUpAxis -= movememntSpeed * deltaTime;
				}
				// E
				if (Window::GetKey(2))
				{
					g_inputUpAxis += movememntSpeed * deltaTime;
				}
			}

			updateFreeFlyCamera();
		}

		// Render
		{
			device->BeginFrame();

			// ImGUI
			{
				ImGui_ImplSDL2_NewFrame();
				ImGui_ImplDX12_NewFrame();
				ImGui::NewFrame();

				// ImGui::Begin("Hierarchy");
				// {
				// 	ImGuiHierarchyForModel(scene.m_Root, true);
				// }
				// ImGui::End();

				ImGui::Begin("Camera");
				{
					// Position
					{
						DirectX::XMFLOAT3 p;
						DirectX::XMStoreFloat3(&p, g_freeFlyCamera.position);
						float position[3] = { p.x, p.y, p.z };

						if (ImGui::InputFloat3("Position", position))
						{
							g_freeFlyCamera.position = DirectX::XMVectorSet(position[0], position[1], position[2], 0.0f);
						}
					}

					// Yaw & Pitch
					{
						float yawDeg = DirectX::XMConvertToDegrees(g_freeFlyCamera.yaw);
						float pitchDeg = DirectX::XMConvertToDegrees(g_freeFlyCamera.pitch);
						if (ImGui::InputFloat("Yaw", &yawDeg))
						{
							g_freeFlyCamera.yaw = DirectX::XMConvertToRadians(yawDeg);
						}

						if (ImGui::InputFloat("Pitch", &pitchDeg))
						{
							pitchDeg = pitchDeg < -80.0f ? -80.0f : pitchDeg;
							pitchDeg = pitchDeg > 80.0f ? 80.0f : pitchDeg;

							g_freeFlyCamera.pitch = DirectX::XMConvertToRadians(pitchDeg);
						}
					}
				}
				ImGui::End();

				ImGui::Render();
			}

			D3D12Lite::TextureResource& backBuffer = device->GetCurrentBackBuffer();

			terrainRenderer.Render(graphicsContext.get(), computeContext.get(), g_freeFlyCamera, &backBuffer, g_depthBuffer.get());

			// ImGUI
			{
				D3D12Lite::PipelineInfo pipeline;
				pipeline.mPipeline = nullptr;
				pipeline.mRenderTargets.push_back(&backBuffer);
				graphicsContext->SetPipeline(pipeline);
				ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), graphicsContext->GetCommandList());
				ImGui::EndFrame();
			}

			graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
			graphicsContext->FlushBarriers();

			device->SubmitContextWork(*graphicsContext);

			device->EndFrame();
			device->Present();
		}
	}

	device->WaitForIdle();

	ImGui_ImplSDL2_Shutdown();
	ImGui_ImplDX12_Shutdown();
	ImGui::DestroyContext();

	// scene.Shutdown();
	terrainRenderer.Shutdown();

	device->DestroyTexture(std::move(g_depthBuffer));

	device->DestroyContext(std::move(graphicsContext));
	device->DestroyContext(std::move(computeContext));
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
