#pragma once

#include "imgui.h"

struct ImGuiApp;

typedef int ImGuiAppFrameFlags;
enum ImGuiAppFrameFlags_
{
    ImGuiAppFrameFlags_None              = 0,
    ImGuiAppFrameFlags_NoClear           = 1 << 0,
    ImGuiAppFrameFlags_NoPresent         = 1 << 1,
    ImGuiAppFrameFlags_NoPlatformWindows = 1 << 2,
};

struct ImGuiAppFrameConfig
{
    ImVec4             ClearColor;
    ImGuiAppFrameFlags Flags;
};

struct ImGuiAppPlatform
{
    const char* Name;
    void*       NativeWindowHandle;
};

struct ImGuiAppConfig
{
    ImGuiAppPlatform Platform;
    ImGuiConfigFlags ConfigFlags;
    float            DpiScale;
    bool             PersistSettings;
    const char*      WindowTitle;
    int              WindowWidth;
    int              WindowHeight;
};
