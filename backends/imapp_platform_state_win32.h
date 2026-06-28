#pragma once
#include <windows.h>

struct ImGuiApp;

struct ImGuiAppPlatformState
{
    struct WGLWindowData { HDC hDC; };
    HWND          Hwnd;
    WNDCLASSEXA   WindowClass;
    HGLRC         MainGLRC;
    WGLWindowData MainWindow;
};

LRESULT WINAPI ImGuiApp_Win32WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);