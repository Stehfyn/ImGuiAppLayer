#pragma once

struct ImGuiApp;

struct ImGuiAppPlatform
{
    const char* Name = nullptr;
    void* UserData = nullptr;
    bool (*Initialize)(ImGuiApp* app, void* user_data) = nullptr;
    void (*Shutdown)(ImGuiApp* app, void* user_data) = nullptr;
};

struct ImGuiAppConfig
{
    ImGuiAppPlatform Platform;
};
