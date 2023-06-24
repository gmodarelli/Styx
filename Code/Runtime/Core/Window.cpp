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

#include "Window.h"

#include <SDL.h>
#include <SDL_syswm.h>

#include <cassert>
#include <stdio.h>
#include <array>

namespace Styx
{
	SDL_Window* mWindow = nullptr;
	uint32_t mWidth = 1920;
	uint32_t mHeight = 1080;
	bool mClose = false;
	bool mShown = false;
	bool mMinimized = false;
	bool mMaximized = false;

	// NOTE(gmodarelli): Temp
	// Timer
	uint64_t mTimerLast = 0;
	uint64_t mTimerNow = 0;
	float mDeltaTime = 0.0f;

	// NOTE(gmodarelli): Temp
	std::array<bool, 10> mKeys;
	std::array<bool, 10> mPreviousFrameKeys;
	float mMousePositionX = 0.0f;
	float mMousePositionY = 0.0f;
	float mMouseDeltaX = 0.0f;
	float mMouseDeltaY = 0.0f;

	void Window::Initialize()
	{
		// Initialize SDL 2
		if (!SDL_WasInit(SDL_INIT_VIDEO))
		{
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
			{
				const char* error = SDL_GetError();
				printf("[Core::Window] Failed to initialize SDL video subsystem: '%s'\n", error);
				assert(false && "Failed to initialize SDL video subsystem");
				return;
			}
		}

		if (!SDL_WasInit(SDL_INIT_EVENTS))
		{
			if (SDL_InitSubSystem(SDL_INIT_EVENTS) == -1)
			{
				const char* error = SDL_GetError();
				printf("[Core::Window] Failed to initialize SDL events subsystem: '%s'\n", error);
				assert(false && "Failed to initialize SDL events subsystem");
				return;
			}
		}

		// Fetch display count
		int32_t num_displays = SDL_GetNumVideoDisplays();
		if (num_displays < 0)
		{
			const char* error = SDL_GetError();
			printf("[Core::Window] Failed to get the number of monitors: '%s'\n", error);
			assert(false && "Failed to get the number of monitors");
			SDL_Quit();
			return;
		}

		printf("[Core::Window] Found %d displays\n", num_displays);
		SDL_Rect display_rect(0, 0, mWidth, mHeight);

		for (int32_t i = 0; i < num_displays; i++)
		{
			int32_t result = SDL_GetDisplayUsableBounds(i, &display_rect);
			if (result < 0)
			{
				const char* error = SDL_GetError();
				printf("[Core::Window] Failed to get the bounds of display at index %d: '%s'\n", i, error);
				assert(false && "Failed to get the bounds of a display");
			}
			else
			{
				printf("[Core::Window] Display %d (%d, %d) -> (%d x %d)\n", i, display_rect.x, display_rect.y, display_rect.w, display_rect.h);
			}
		}

		uint32_t window_flags = 0;
		window_flags |= SDL_WINDOW_SHOWN;
		window_flags |= SDL_WINDOW_RESIZABLE;
		window_flags |= SDL_WINDOW_MAXIMIZED;
		window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
		mWindow = SDL_CreateWindow("Styx", display_rect.x, display_rect.y, display_rect.w, display_rect.h, window_flags);

		if (mWindow == nullptr)
		{
			const char* error = SDL_GetError();
			printf("[Core::Window] Failed to create a window: '%s'\n", error);
			assert(false && "Failed to create a window");
			SDL_Quit();
			return;
		}

		int32_t newWidth = 0;
		int32_t newHeight = 0;
		SDL_GetWindowSize(mWindow, &newWidth, &newHeight);
		mWidth = (uint32_t)newWidth;
		mHeight = (uint32_t)newHeight;
	}

	void Window::Shutdown()
	{
		SDL_DestroyWindow(mWindow);
		mWindow = nullptr;

		SDL_Quit();
	}

	void Window::Tick()
	{
		mTimerLast = mTimerNow;
		mTimerNow = SDL_GetPerformanceCounter();
		mDeltaTime = (float)((mTimerNow - mTimerLast) / (double)SDL_GetPerformanceFrequency());

		SDL_Event sdlEvent;
		while (SDL_PollEvent(&sdlEvent))
		{
			if (sdlEvent.type == SDL_WINDOWEVENT)
			{
				if (sdlEvent.window.windowID == SDL_GetWindowID(mWindow))
				{
					switch (sdlEvent.window.event)
					{
					case SDL_WINDOWEVENT_CLOSE:
						mClose = true;
						break;

                    case SDL_WINDOWEVENT_RESIZED:
                        mWidth = static_cast<uint32_t>(sdlEvent.window.data1);
                        mHeight = static_cast<uint32_t>(sdlEvent.window.data2);
                        break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        mWidth = static_cast<uint32_t>(sdlEvent.window.data1);
                        mHeight = static_cast<uint32_t>(sdlEvent.window.data2);
                        break;
                    case SDL_WINDOWEVENT_MINIMIZED:
                        mMinimized = true;
                        mMaximized = false;
                        break;
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        mMaximized = true;
                        mMinimized = false;
                        break;
					}
				}
			}
		}

		// NOTE(gmodarelli): This is here temporarily just to get camera movement going
		mPreviousFrameKeys = mKeys;
		const uint8_t* keyStates = SDL_GetKeyboardState(nullptr);
		mKeys[0] = keyStates[SDL_SCANCODE_Q];
		mKeys[1] = keyStates[SDL_SCANCODE_W];
		mKeys[2] = keyStates[SDL_SCANCODE_E];
		mKeys[3] = keyStates[SDL_SCANCODE_A];
		mKeys[4] = keyStates[SDL_SCANCODE_S];
		mKeys[5] = keyStates[SDL_SCANCODE_D];
		mKeys[6] = keyStates[SDL_SCANCODE_LSHIFT];

		int x, y;
		uint32_t mouse_states = SDL_GetGlobalMouseState(&x, &y);
		mMouseDeltaX = (float)x - mMousePositionX;
		mMouseDeltaY = (float)y - mMousePositionY;
		mMousePositionX = (float)x;
		mMousePositionY = (float)y;

		mKeys[7] = (mouse_states & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
		mKeys[8] = (mouse_states & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
		mKeys[9] = (mouse_states & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
	}

	void* Window::GetWindowHandle()
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(mWindow, &info);
		return (void*)info.info.win.window;
	}

	uint32_t Window::GetWidth()
	{
		return mWidth;
	}

	uint32_t Window::GetHeight()
	{
		return mHeight;
	}

	bool Window::ShouldClose()
	{
		return mClose;
	}

	bool Window::IsMinimized()
	{
		return mMinimized;
	}

	float Window::GetDeltaTime()
	{
		return mDeltaTime;
	}

	bool Window::GetKey(const int key)
	{
		assert(key >= 0 && key < 10);
		return mKeys[(uint32_t)key];
	}

	void Window::GetMousePosition(float* x, float* y)
	{
		*x = mMousePositionX;
		*y = mMousePositionY;
	}

	void Window::SetMousePosition(float x, float y)
	{
		if (SDL_WarpMouseGlobal((int)x, (int)y) != 0)
		{
			return;
		}

		mMousePositionX = x;
		mMousePositionY = y;
	}

	void Window::GetMouseDelta(float* x, float* y)
	{
		*x = mMouseDeltaX;
		*y = mMouseDeltaY;
	}

    uint32_t Window::GetDisplayIndex()
    {
        int index = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(mWindow));

        // during engine startup, the window doesn't exist yet, therefore it's not displayed by any monitor.
        // in this case the index can be -1, so we'll instead set the index to 0 (whatever the primary display is)
        return index != -1 ? index : 0;
    }

    uint32_t Window::GetDisplayWidth()
    {
        SDL_DisplayMode display_mode;
        assert(SDL_GetCurrentDisplayMode(GetDisplayIndex(), &display_mode) == 0);

        return display_mode.w;
    }

    uint32_t Window::GetDisplayHeight()
    {
        int display_index = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(mWindow));

        SDL_DisplayMode display_mode;
        assert(SDL_GetCurrentDisplayMode(GetDisplayIndex(), &display_mode) == 0);

        return display_mode.h;
    }
}
