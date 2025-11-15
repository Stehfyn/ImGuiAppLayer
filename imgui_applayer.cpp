#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer.h"
#include "imgui_internal.h"
#include <ctime>

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
  static void AppWindowLayerSettingsHandler_ClearAll(ImGuiContext*, ImGuiSettingsHandler*)
  {
  }

  static void AppWindowLayerSettingsHandler_ReadInit(ImGuiContext*, ImGuiSettingsHandler*)
  {
  }

  static void* AppWindowLayerSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
  {
    return nullptr;
  }

  static void AppWindowLayerSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
  {
  }

  static void AppWindowLayerSettingsHandler_ApplyAll(ImGuiContext*, ImGuiSettingsHandler*)
  {
  }

  static void AppWindowLayerSettingsHandler_WriteAll(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer*)
  {
  }
}

void ImGuiAppWindowLayer::OnAttach(ImGuiApp* app) const
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiContext& g = *ctx;

    IM_UNUSED(app);

    // Add .ini handle for persistent AppWindow layer data
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "AppWindowLayer";
    ini_handler.TypeHash = ImHashStr("AppWindowLayer");
    ini_handler.ClearAllFn = AppWindowLayerSettingsHandler_ClearAll;
    ini_handler.ReadInitFn = AppWindowLayerSettingsHandler_ReadInit;
    ini_handler.ReadOpenFn = AppWindowLayerSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = AppWindowLayerSettingsHandler_ReadLine;
    ini_handler.ApplyAllFn = AppWindowLayerSettingsHandler_ApplyAll;
    ini_handler.WriteAllFn = AppWindowLayerSettingsHandler_WriteAll;
    g.SettingsHandlers.push_back(ini_handler);
}

void ImGuiAppWindowLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppWindowLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    for (auto& sidebar : app->Sidebars)
      sidebar->OnUpdate(app, dt);

    for (auto& window : app->Windows)
      window->OnUpdate(app, dt);
}

void ImGuiAppWindowLayer::OnRender(const ImGuiApp* app) const
{
    for (auto& sidebar : app->Sidebars)
    {
      sidebar->OnStylePush(app);

      if (sidebar->Window && (sidebar->Flags & ImGuiWindowFlags_AlwaysAutoResize))
      {
        int idx = (ImGuiDir_Up == sidebar->DockDir) || (ImGuiDir_Down == sidebar->DockDir);
        sidebar->Size = sidebar->Window->ContentSizeIdeal[idx] + (2.0f * ImGui::GetStyle().WindowPadding[idx]);
      }

      if (ImGui::BeginViewportSideBar(sidebar->Label, sidebar->Viewport, sidebar->DockDir, sidebar->Size, sidebar->Flags))
      {
        sidebar->Window = ImGui::GetCurrentWindow();
        sidebar->OnRender(app);
      }

      ImGui::End();

      sidebar->OnStylePop(app);
    }

    for (auto& window : app->Windows)
    {
      if (ImGui::Begin(window->Label))
          window->OnRender(app);
      ImGui::End();
    }
}

namespace
{
  // Encourage "pure" design, such that a control is "agnostic to the data passing through it."
  static void RenderTextT(const char* text, ImVec2 text_size, ImVec2 pos, ImVec2 avail, float t_value)
  {
    float offset = avail.x * 0.5f + (t_value * 0.5f * (avail.x - text_size.x));
    ImGui::SetCursorScreenPos(pos + ImVec2(offset, 0.0f) + (text_size * -0.5f));
    ImGui::Text("%s", text);
  }

  struct RandomTimeData
  {
    char label[128];
    char type[128];
    float max_timer_secs;
  };

  struct RandomTimeTempData
  {
    bool generate;
  };

  struct RandomTimeControlDemo : ImGuiControl <RandomTimeData, RandomTimeTempData>
  {
    // Random time between 1 and 30 seconds
    static float GenerateTime() { return static_cast<float>(1 + (rand() % 30)); }

    virtual void OnInitialize(ImGuiApp*, RandomTimeData* data) const override final
    {
      std::string_view sv;

      srand(static_cast<unsigned int>(time(nullptr)));

      sv = ImGuiType<decltype(this)>::Name;

      ImStrncpy(data->type, sv.data(), sv.length());
      ImFormatString(data->label, sizeof(data->label), "%s", data->type);

      data->max_timer_secs = GenerateTime();
    }

    virtual void OnUpdate(float dt, RandomTimeData* data, const RandomTimeTempData* temp_data, const RandomTimeTempData* last_temp_data) const override final
    {
      IM_UNUSED(temp_data);
      IM_UNUSED(last_temp_data);

      if (temp_data->generate)
        data->max_timer_secs = GenerateTime();
    }

    virtual void OnRender(const RandomTimeData* data, RandomTimeTempData* temp_data) const override final
    {
      IM_UNUSED(data);
      IM_UNUSED(temp_data);

      if (ImGui::Begin(data->label))
      {
        ImGui::Text("%s", "Max Timer Seconds");

        temp_data->generate = ImGui::Button("Generate");
        ImGui::SameLine();

        ImGui::Text("%.1f", data->max_timer_secs);
      }
      ImGui::End();
    }
  };

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

  struct BreathingControlDemo : ImGuiControl<BreathingControlData, BreathingControlTempData, RandomTimeData>
  {
    virtual void OnInitialize(ImGuiApp*, BreathingControlData* data, const RandomTimeData* random_time_data) const override final
    {
      std::string_view sv;

      IM_UNUSED(random_time_data); // Explicitly ignore dependency on initialization (as we explicitly use it on OnUpdate())

      sv = ImGuiType<decltype(this)>::Name;

      ImStrncpy(data->type, sv.data(), sv.length());
      ImFormatString(data->label, sizeof(data->label), "%s", data->type);
    }

    virtual void OnUpdate(float dt, BreathingControlData* data, const BreathingControlTempData* temp_data, const BreathingControlTempData* last_temp_data, const RandomTimeData* random_time_data) const override final
    {
      data->timer_secs = ImMax(0.0f, data->timer_secs - dt);

      if (temp_data->hovered ^ last_temp_data->hovered)
      {
        // If hovered, use RandomTimeData::max_timer_secs to set the timer (otherwise, data->timer_secs will be set to 0.0f)
        data->timer_secs = temp_data->hovered * random_time_data->max_timer_secs;
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

    virtual void OnRender(const BreathingControlData* data, BreathingControlTempData* temp_data, const RandomTimeData* random_time_data) const override final
    {
      IM_UNUSED(random_time_data); // Explicitly ignore dependency on rendering (as we explicitly use it on OnUpdate())

      if (ImGui::Begin(data->label))
      {
        ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 2.0f * ImGui::GetFrameHeight());

        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, data->col);
        ImGui::Button("Hover Me!", size);
        temp_data->hovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor();

        ImVec2 pos = ImGui::GetCursorScreenPos();
        size = ImGui::GetContentRegionAvail();
        ImRect r = { pos, pos + size };

        ImGui::PushClipRect(r.Min, r.Max, true);
        ImGui::NewLine();
        RenderTextT(data->timer_text, ImGui::CalcTextSize(data->timer_text), ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail(), data->t_value);
        ImGui::PopClipRect();
      }
      ImGui::End();
    }
  };

  struct BaseWindow : ImGuiAppWindow<BaseWindow>
  {
    virtual void OnRender(const ImGuiApp*) const override final
    {
      ImGui::TextWrapped("%s", "This is a base window managed by ImGuiAppWindowLayer.");
    }
  };

  struct StatusBar : ImGuiAppSidebar<StatusBar>
  {
    virtual void OnRender(const ImGuiApp*) const override final
    {
      ImGui::TextWrapped("%s", "This is a status bar managed by ImGuiAppSidebar.");
    }
  };

  typedef void(*JumpAndDrawShit)(void);

  struct DebugUruguaaaayyyBarData
  {
    int idx;
    ImVector<JumpAndDrawShit> callbacks;
  };

  struct DebugBarTempDataaaaaaaaa
  {
    int idx;
  };

  struct AppDebuggerControl : ImGuiControl<DebugUruguaaaayyyBarData, DebugBarTempDataaaaaaaaa>
  {
    virtual void OnInitialize(ImGuiApp*, DebugUruguaaaayyyBarData* data) const override final
    {
      data->callbacks.push_back([] {ImGui::ShowMetricsWindow(); });
      data->callbacks.push_back([] { ImGui::Begin("ImGuiStyleEditor"); ImGui::ShowStyleEditor(); ImGui::End(); });
      data->callbacks.push_back([] {ImGui::ShowAboutWindow(); });
    }

    virtual void OnUpdate(float, DebugUruguaaaayyyBarData* data, const DebugBarTempDataaaaaaaaa* temp_data, const DebugBarTempDataaaaaaaaa* last_temp_data) const override final
    {
      IM_UNUSED(last_temp_data);
      data->idx = temp_data->idx;
    }


    virtual void OnRender(const DebugUruguaaaayyyBarData* data, DebugBarTempDataaaaaaaaa* temp_data) const override final
    {
      IM_UNUSED(data);
      IM_UNUSED(temp_data);

      static const auto get_width = [](const char* str) -> float { return ImGui::CalcTextSize(str).x + 2.0f * ImGui::GetStyle().FramePadding.x; };

      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
      temp_data->idx = 0 | data->idx;
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(((1 << 1) & data->idx) ? ImGuiCol_ButtonActive : ImGuiCol_Button));
      temp_data->idx ^= (1 << 1) * ImGui::Button("Debug", ImVec2(get_width("Debug"), ImGui::GetFrameHeightWithSpacing())); ImGui::PopStyleColor(); ImGui::SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(((1 << 2) & data->idx) ? ImGuiCol_ButtonActive : ImGuiCol_Button));
      temp_data->idx ^= (1 << 2) * ImGui::Button("Style", ImVec2(get_width("Style"), ImGui::GetFrameHeightWithSpacing())); ImGui::PopStyleColor(); ImGui::SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(((1 << 3) & data->idx) ? ImGuiCol_ButtonActive : ImGuiCol_Button));
      temp_data->idx ^= (1 << 3) * ImGui::Button("About", ImVec2(get_width("About"), ImGui::GetFrameHeightWithSpacing())); ImGui::PopStyleColor(); ImGui::SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
      ImGuiIO& io = ImGui::GetIO();
      char buf[128];
      ImFormatString(buf, sizeof(buf), "%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
      float w = ImGui::CalcTextSize(buf).x;
      ImVec2 pos = ImGui::GetCursorScreenPos();
      ImVec2 offset = ImVec2(ImGui::GetContentRegionAvail().x - w, 0.0f);
      pos.x = ImMax(pos.x, pos.x + offset.x);
      ImGui::SetCursorScreenPos(pos);
      ImFormatString(buf, sizeof(buf), "%.3f ms/frame ", 1000.0f / io.Framerate);
      ImGui::Text("%s", buf); ImGui::SameLine();

      pos = ImGui::GetCursorScreenPos();
      pos.x = ImMin(pos.x, pos.x + offset.x);
      ImGui::SetCursorScreenPos(pos);
      ImFormatString(buf, sizeof(buf), "(%.1f FPS)", io.Framerate); 
      ImGui::GetWindowDrawList()->AddRectFilled(pos, pos + ImGui::CalcTextSize(buf) + ImGui::GetStyle().FramePadding *2.0f, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
      ImGui::Text("%s", buf);
      ImGui::PopStyleColor();

      int i = 0;
      for (auto& cb : data->callbacks)
        if ((1 << (++i)) & data->idx)
          cb();
    }
  };

  struct DebugBar : ImGuiAppSidebar<DebugBar>
  {
    virtual void OnInitialize(ImGuiApp* app) override final
    {
      ImGui::PushSidebarControl<AppDebuggerControl>(app, this);
    }

    virtual void OnUpdate(const ImGuiApp* app, float dt) const override final
    {
      for (auto& control : Controls)
        control->OnUpdate(app, dt);
    }

    virtual void OnRender(const ImGuiApp* app) const override final
    {
      for (auto& control : Controls)
        control->OnRender(app);
    }

    virtual void OnStylePush(const ImGuiApp*) const override final
    {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());
    }

    virtual void OnStylePop(const ImGuiApp*) const override final
    {
      ImGui::PopStyleVar();
      ImGui::PopStyleVar();
    }
  };
}

namespace ImGui
{
  IMGUI_API void InitializeApp(ImGuiApp* app)
  {
      IM_ASSERT(app);
      PushAppLayer<ImGuiAppTaskLayer>(app);
      PushAppLayer<ImGuiAppCommandLayer>(app);
      PushAppLayer<ImGuiAppStatusLayer>(app);
      PushAppLayer<ImGuiAppWindowLayer>(app);
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
        {
          PushAppWindow<BaseWindow>(&app);
          PushAppWindow<BaseWindow>(&app);
          PushAppWindow<BaseWindow>(&app);
          PushAppWindow<BaseWindow>(&app);
          PushAppSidebar<DebugBar>(&app, ImGui::GetMainViewport(), ImGuiDir_Down, 0.0f, ImGuiWindowFlags_AlwaysAutoResize);
          PushAppSidebar<StatusBar>(&app, ImGui::GetMainViewport(), ImGuiDir_Up, 0.0f, ImGuiWindowFlags_AlwaysAutoResize);
          PushAppSidebar<StatusBar>(&app, ImGui::GetMainViewport(), ImGuiDir_Down, 0.0f, ImGuiWindowFlags_AlwaysAutoResize);
        }
        // This demo showcases a button whose color appears to "breathe" while hovered,
        // (i.e. BreathingControlDemo) with a timer that is randomly set each time the
        // button is hovered, which is managed by another control (RandomTimeControlDemo).
        {
          PushAppControl<RandomTimeControlDemo>(&app);
          PushAppControl<BreathingControlDemo>(&app);
        }
      }

      UpdateApp(&app);
      RenderApp(&app);

      if (app.ShutdownPending)
        ShutdownApp(&app);
  }
}
