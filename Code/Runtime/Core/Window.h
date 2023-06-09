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

#pragma once

#include <cstdint>

namespace Styx
{
	class Window
	{
	public:
		static void Initialize();
		static void Shutdown();
		static void Tick();

		static void* GetWindowHandle();
		static uint32_t GetWidth();
		static uint32_t GetHeight();

		static bool ShouldClose();
		static bool IsMinimized();

		static float GetDeltaTime();

		static bool GetKey(const int key);
		static void GetMousePosition(float* x, float* y);
		static void SetMousePosition(float x, float y);
		static void GetMouseDelta(float* x, float* y);
		static uint32_t GetDisplayIndex();
		static uint32_t GetDisplayWidth();
		static uint32_t GetDisplayHeight();

		static void* GetSDLWindow();
		static void HackHackHack();
	};
}
