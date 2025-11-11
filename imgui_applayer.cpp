#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer.h"
#include "imgui_internal.h"

void ImGuiApp::OnExecuteCommand(ImGuiAppCommand cmd)
{
    IM_UNUSED(cmd);
}

void ImGuiAppTaskLayer::OnAttach(ImGuiApp* app ) const
{
    IM_UNUSED(app);
}

void ImGuiAppTaskLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppTaskLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    IM_UNUSED(app);
    IM_UNUSED(dt);
}

void ImGuiAppTaskLayer::OnRender(const ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppCommandLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppCommandLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppCommandLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    IM_UNUSED(dt);

    ImGuiAppCommand cmd;
    ImBitArray<ImGuiAppCommand_COUNT> arr;

    for (auto& control : app->Controls)
    {
        cmd = ImGuiAppCommand_None;
        control->OnGetCommand(app, &cmd);
        arr.SetBit(static_cast<int>(cmd));
    }

    for (cmd = ImGuiAppCommand_None; cmd < ImGuiAppCommand_COUNT; cmd = static_cast<ImGuiAppCommand>(1 + cmd))
    {
        if (arr.TestBit(cmd))
            app->OnExecuteCommand(cmd);
    }
}

void ImGuiAppCommandLayer::OnRender(const ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppStatusLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppStatusLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
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
  static void RenderTextT(const char* text, ImVec2 text_size, ImVec2 pos, ImVec2 avail, float t_value)
  {
    float offset = avail.x * 0.5f + (t_value * 0.5f * (avail.x - text_size.x));
    ImGui::SetCursorScreenPos(pos + ImVec2(offset, 0.0f) + (text_size * -0.5f));
    ImGui::Text("%s", text);
  }

  struct BreathingControlData
  {
    char label[128];
    char type[128];
    char text[128];
    char timer_text[128];
    float timer_secs;
    float t_value;
    float t_direction;
    ImVec4 col;
  };

  struct BreathingControlTempData
  {
    bool hovered;
  };

  struct BreathingControlDemo : ImGuiControl<BreathingControlData, BreathingControlTempData>
  {
    virtual void OnInitialize(ImGuiApp*, BreathingControlData* data) const override final
    {
      std::string_view sv = ImGuiType<decltype(this)>::Name;

      ImStrncpy(data->type, sv.data(), sv.length());
      ImFormatString(data->label, sizeof(data->label), "%s", data->type);
    }

    virtual void OnUpdate(float dt, BreathingControlData* data, const BreathingControlTempData* temp_data, const BreathingControlTempData* last_temp_data) const override final
    {
      data->timer_secs = ImMax(0.0f, data->timer_secs - dt);

      if (temp_data->hovered ^ last_temp_data->hovered)
      {
        data->timer_secs = 30.0f * temp_data->hovered;
        data->t_value = 0.0f;
        data->t_direction = 1.0f;
      }

      if (0.0f < data->timer_secs)
      {
        ImFormatString(data->timer_text, sizeof(data->timer_text), "%.1f Seconds Left!", data->timer_secs);
      }
      else
      {
        ImStrncpy(data->timer_text, "Timer Expired!", sizeof(data->timer_text));
      }

      if (temp_data->hovered)
      {
        data->t_value = ImLinearSweep(data->t_value, data->t_direction, dt);
        data->t_direction = (data->t_value == data->t_direction) ? -data->t_direction : data->t_direction;
        data->col = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Button), ImGui::GetStyleColorVec4(ImGuiCol_WindowBg), 0.0f <= data->t_value ? data->t_value : -data->t_value);
      }
    }

    virtual void OnRender(const BreathingControlData* data, BreathingControlTempData* temp_data) const override final
    {
      if (ImGui::Begin(data->label))
      {
        ImRect r;
        ImVec2 pos;
        ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 2.0f * ImGui::GetFrameHeight());
        
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, data->col);
        ImGui::Button("Hover Me!", size);
        temp_data->hovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor();

        pos = ImGui::GetCursorScreenPos();
        size = ImGui::GetContentRegionAvail();
        r = { pos, pos + size };

        ImGui::PushClipRect(r.Min, r.Max, true);
        ImGui::NewLine();
        RenderTextT(data->timer_text, ImGui::CalcTextSize(data->timer_text), ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail(), data->t_value);
        ImGui::PopClipRect();
      }
      ImGui::End();
    }
  };
}

namespace ImGui
{
  IMGUI_API void InitializeApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      //PushAppLayer<ImGuiTask>
      PushAppLayer<ImGuiAppCommandLayer>(app);
      PushAppLayer<ImGuiAppStatusLayer>(app);
  }

  IMGUI_API void ShutdownApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      while (!app->Controls.empty())
        PopAppControl(app);

      while (!app->Layers.empty())
        PopAppLayer(app);
  }

  IMGUI_API void UpdateApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      for (auto& layer : app->Layers)
        layer->OnUpdate(app, GetIO().DeltaTime);
  }

  IMGUI_API void RenderApp(const ImGuiApp* app)
  {
      IM_ASSERT(app);

      for (auto& layer : app->Layers)
        layer->OnRender(app);
  }

  IMGUI_API void ShowAppLayerDemo()
  {
      static ImGuiApp app;

      if (1 == ImGui::GetFrameCount())
      {
        InitializeApp(&app);

        PushAppControl<BreathingControlDemo>(&app);
      }

      UpdateApp(&app);
      RenderApp(&app);

      if (app.ShutdownCommanded)
        ShutdownApp(&app);
  }
}
