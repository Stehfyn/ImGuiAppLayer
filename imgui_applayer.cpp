#include "imgui_applayer.h"
#include "imgui_internal.h"

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

namespace
{
  struct MyControlData
  {
    char label[64];
    char type[64];
    char text[64];
    char timer_text[64];
    float timer_secs;
  };

  struct MyControlTempData
  {
    bool hovered;
  };

  struct MyControl : ImGuiControl<MyControlData, MyControlTempData>
  {
    virtual void OnInitialize(ImGuiApp*, MyControlData* data) const override final
    {
      ImStrncpy(data->type, ImGuiType<decltype(this)>::Name.data(), sizeof(data->type));

      ImFormatString(data->label, sizeof(data->label), "## %s", data->type);
    }

    virtual void OnUpdate(const ImGuiApp*, float dt, MyControlData* data, const MyControlTempData* temp_data, const MyControlTempData* last_temp_data) const override final
    {
      data->timer_secs = ImMax(0.0f, data->timer_secs - dt);

      if (temp_data->hovered ^ last_temp_data->hovered)
      {
        data->timer_secs = 3.0f * temp_data->hovered;
      }

      if (0.0f < data->timer_secs)
      {
        ImFormatString(data->timer_text, sizeof(data->timer_text), "%.1f Seconds Left! ## Timer Text", data->timer_secs);
      }
      else
      {
        ImStrncpy(data->timer_text, "Timer Expired! ## Timer Text", sizeof(data->timer_text));
      }
    }

    virtual void OnRender(const ImGuiApp*, const MyControlData* data, MyControlTempData* temp_data) const override final
    {
      if (ImGui::Begin(data->label))
      {
        ImGui::Text(data->label);

        temp_data->hovered = ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

        ImGui::Text(data->timer_text);
      }
      ImGui::End();
    }
  };
}

namespace ImGui
{
  void ShowAppLayerDemo()
  {
      static ImGuiApp app;

      if (1 == ImGui::GetFrameCount())
      {
        InitializeApp(&app);

        PushAppControl<MyControl>(&app);
      }

      UpdateApp(&app);
      RenderApp(&app);

      if (app.ShutdownCommanded)
        ShutdownApp(&app);
  }
}
