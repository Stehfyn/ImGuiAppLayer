#pragma once
#include <SDL.h>

struct ImGuiAppPlatformState
{
    SDL_Window*   Window;
    SDL_GLContext GLContext;
    bool          Running;
};