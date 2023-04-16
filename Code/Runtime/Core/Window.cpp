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

#include "pch.h"
#include "Window.h"

#include <SDL.h>

namespace Styx
{
	SDL_Window* g_window = nullptr;
	bool g_close = false;

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
		SDL_Rect display_rect(0, 0, 1920, 1080);

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
		g_window = SDL_CreateWindow("Styx", display_rect.x, display_rect.y, display_rect.w, display_rect.h, window_flags);

		if (g_window == nullptr)
		{
			const char* error = SDL_GetError();
			printf("[Core::Window] Failed to create a window: '%s'\n", error);
			assert(false && "Failed to create a window");
			SDL_Quit();
			return;
		}
	}

	void Window::Shutdown()
	{
		SDL_DestroyWindow(g_window);
		g_window = nullptr;

		SDL_Quit();
	}

	void Window::Tick()
	{
		SDL_Event sdl_event;
		while (SDL_PollEvent(&sdl_event))
		{
			if (sdl_event.type == SDL_WINDOWEVENT)
			{
				if (sdl_event.window.windowID == SDL_GetWindowID(g_window))
				{
					switch (sdl_event.window.event)
					{
					case SDL_WINDOWEVENT_CLOSE:
						g_close = true;
						break;
					}
				}
			}
		}
	}

	bool Window::ShouldClose()
	{
		return g_close;
	}

	void* Window::GetSDLWindow()
	{
		return g_window;
	}
}
