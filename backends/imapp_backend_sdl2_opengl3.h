#pragma once
#include "imguix.h"

struct ImGuiApp;
struct ImGuiAppPlatformState;

IMGUIX_API bool ImGuiApp_Sdl2OpenGL3_InitPlatform(ImGuiApp* app, ImGuiAppPlatformState* state, ImGuiAppConfig& config);
IMGUIX_API void ImGuiApp_Sdl2OpenGL3_ShutdownPlatform(ImGuiApp* app, ImGuiAppPlatformState* state);
