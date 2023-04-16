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
#include <stdio.h>

using namespace Styx;

// NOTE(gmodarelli): This is all temporary test code to test the current WIP implementation
// of the RHI
int main()
{
	Window::Initialize();

	D3D12Lite::Uint2 screenSize(Window::GetWidth(), Window::GetHeight());
	std::unique_ptr<D3D12Lite::Device> device = std::make_unique<D3D12Lite::Device>(Window::GetWindowHandle(), screenSize);
	std::unique_ptr<D3D12Lite::GraphicsContext> graphicsContext = device->CreateGraphicsContext();

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
			graphicsContext->FlushBarriers();

			float color[4] = {0.3f, 0.3f, 0.8f, 1.0f};
			graphicsContext->ClearRenderTarget(backBuffer, color);

			graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
			graphicsContext->FlushBarriers();

			device->SubmitContextWork(*graphicsContext);

			device->EndFrame();
			device->Present();
		}
	}

	device->WaitForIdle();
	device->DestroyContext(std::move(graphicsContext));
	device = nullptr;

	Window::Shutdown();

	return 0;
}