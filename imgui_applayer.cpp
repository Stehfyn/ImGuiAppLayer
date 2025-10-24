#include "imgui_applayer.h"

void ImGuiApp::OnExecuteCommand(ImGuiAppCommand cmd)
{
  switch (cmd) {
  case ImGuiAppCommand_Shutdown:
    ShutdownCommanded = true;
    break;
  }
}

void ImGuiAppTaskLayer::OnAttach(ImGuiApp* app ) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppTaskLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppTaskLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    IM_UNREFERENCED_PARAMETER(app);
    IM_UNREFERENCED_PARAMETER(dt);
}

void ImGuiAppTaskLayer::OnRender(const ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppCommandLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppCommandLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppCommandLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    ImGuiAppCommand cmd = ImGuiAppCommand_None;

    for (auto& control : app->Controls)
        control->OnGetCommand(app, &cmd);

    app->OnExecuteCommand(cmd);
}

void ImGuiAppCommandLayer::OnRender(const ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppStatusLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppStatusLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNREFERENCED_PARAMETER(app);
}

void ImGuiAppStatusLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    for (auto& control : app->Controls)
      control->OnUpdate(app, dt);
}

void ImGuiAppStatusLayer::OnRender(const ImGuiApp* app) const
{
    for (auto& control : app->Controls)
      control->OnRender(app);
}


