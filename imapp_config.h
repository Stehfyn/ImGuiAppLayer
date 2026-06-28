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

    ImGuiAppFrameConfig()
        : ClearColor(0.0f, 0.0f, 0.0f, 1.0f)
        , Flags(ImGuiAppFrameFlags_None)
    {
    }
};

enum ImGuiAppStyle_
{
    ImGuiAppStyle_Dark    = 0,
    ImGuiAppStyle_Light   = 1,
    ImGuiAppStyle_Classic = 2,
};
typedef int ImGuiAppStyle;

struct ImGuiAppPlatform
{
    const char* Name;
    void*       NativeWindowHandle;
};

struct ImGuiAppConfig
{
    ImGuiAppPlatform    Platform;
    ImGuiConfigFlags    ConfigFlags;
    ImGuiAppStyle       Style;
    ImVec4              ClearColor;
    float               FontScale;
    float               DpiScale;
    bool                PersistSettings;
    const char*         WindowTitle;
    int                 WindowWidth;
    int                 WindowHeight;
};
