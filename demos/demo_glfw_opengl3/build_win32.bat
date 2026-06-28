@REM Build with VS2022 Developer Command Prompt
@set OUT_DIR=Debug
@set OUT_EXE=demo_glfw_opengl3
@set INCLUDES=/I..\..\.. /I..\.. /I..\..\external\imgui /I..\..\external\imgui\backends /I..\..\external\glfw\include
@set DEFINES=/D_GLFW_WIN32 /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE
@set SOURCES=main.cpp ..\..\imgui_applayer.cpp ..\..\external\imgui\imgui*.cpp ..\..\external\imgui\backends\imgui_impl_glfw.cpp ..\..\external\imgui\backends\imgui_impl_opengl3.cpp
@set GLFW_SOURCES=..\..\external\glfw\src\context.c ..\..\external\glfw\src\init.c ..\..\external\glfw\src\input.c ..\..\external\glfw\src\monitor.c ..\..\external\glfw\src\platform.c ..\..\external\glfw\src\vulkan.c ..\..\external\glfw\src\window.c ..\..\external\glfw\src\egl_context.c ..\..\external\glfw\src\osmesa_context.c ..\..\external\glfw\src\null_init.c ..\..\external\glfw\src\null_joystick.c ..\..\external\glfw\src\null_monitor.c ..\..\external\glfw\src\null_window.c ..\..\external\glfw\src\win32_init.c ..\..\external\glfw\src\win32_joystick.c ..\..\external\glfw\src\win32_module.c ..\..\external\glfw\src\win32_monitor.c ..\..\external\glfw\src\win32_thread.c ..\..\external\glfw\src\win32_time.c ..\..\external\glfw\src\win32_window.c ..\..\external\glfw\src\wgl_context.c
@set LIBS=opengl32.lib gdi32.lib user32.lib shell32.lib
mkdir %OUT_DIR%
cl /nologo /Zi /MD /std:c++20 /EHsc %INCLUDES% %DEFINES% %SOURCES% %GLFW_SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%
