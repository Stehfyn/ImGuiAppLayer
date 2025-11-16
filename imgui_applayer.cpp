#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer.h"
#include "imgui_internal.h"

#include "implot/implot.h"

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

  inline float TweenLinear(float t)
  {
    return t;
  }

  inline float TweenExponential(float t)
  {
    // ease-out
    return 1.0f - ImPow(2.0f, -10.0f * t);
  }

  inline float TweenHarmonic(float t)
  {
    // cosine-based smooth oscillation
    return 0.5f - 0.5f * ImCos(t * 3.14159265f);
  }

  void RenderArrowPolyline(ImDrawList* draw_list, ImVec2 pos, ImU32 col, ImGuiDir dir, float scale, float thickness)
  {
    const float h = draw_list->_Data->FontSize * 1.00f;
    float r = h * 0.25f * scale;
    ImVec2 center = pos + ImVec2(h * 0.50f, h * 0.50f * scale);

    ImVec2 a, b, c;
    switch (dir)
    {
    case ImGuiDir_Up:
    case ImGuiDir_Down:
      if (dir == ImGuiDir_Up) r = -r;
      a = ImVec2(+0.000f, +0.750f) * r;
      b = ImVec2(-0.866f, -0.750f) * r;
      c = ImVec2(+0.866f, -0.750f) * r;
      break;
    case ImGuiDir_Left:
    case ImGuiDir_Right:
      if (dir == ImGuiDir_Left) r = -r;
      a = ImVec2(+0.750f, +0.000f) * r;
      b = ImVec2(-0.750f, +0.866f) * r;
      c = ImVec2(-0.750f, -0.866f) * r;
      break;
    case ImGuiDir_None:
    case ImGuiDir_COUNT:
      IM_ASSERT(0);
      break;
    }

    draw_list->AddLine(center + a, center + b, ImGui::GetColorU32(ImGuiCol_Text), thickness);
    draw_list->AddLine(center + a, center + c, ImGui::GetColorU32(ImGuiCol_Text), thickness);
  }

  inline void ScaledSparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size, ImPlotScale plot_scale)
  {
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
    if (ImPlot::BeginPlot(id,size,ImPlotFlags_CanvasOnly)) {
        ImPlot::SetupAxisScale(ImAxis_Y1, plot_scale);
        ImPlot::SetupAxes(nullptr,nullptr,ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, count - 1, min_v, max_v, ImGuiCond_Always);
        ImPlot::SetNextLineStyle(col);
        ImPlot::SetNextFillStyle(col, 0.25);
        ImPlot::PlotLine(id, values, count, 1, 0, ImPlotLineFlags_Shaded, offset);
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
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

  struct DebugBarData
  {
    int idx;
    bool enabled;
    float t_value;
    bool animating;
    float duration;
    float anim_time;
    ImVector<JumpAndDrawShit> callbacks;
    float scale_max;
    ImVector<float> frame_times;
    float accum_time;

    char toggle_button_label[64];
    float toggle_button_width_scale;
    float toggle_button_circle_radius_font_relative_scale;
    int toggle_button_circle_num_segments;

    float debug_console_arrow_scale;
    float debug_console_arrow_line_width;
    bool use_implot;
    bool use_implot_colormap;
    float plot_col_coeff;
    ImPlotScale plot_scale;
    ImPlotColormap plot_colormap;
    char plot_label[64];
  };

  struct DebugBarTempData
  {
    bool enabled;
    int idx;
  };

  struct AppDebuggerControl : ImGuiControl<DebugBarData, DebugBarTempData>
  {
    virtual void OnInitialize(ImGuiApp*, DebugBarData* data) const override final
    {
      data->callbacks.push_back([] { });
      data->callbacks.push_back([] { ImGui::ShowMetricsWindow(); });
      data->callbacks.push_back([] { ImGui::Begin("ImGuiStyleEditor"); ImGui::ShowStyleEditor(); ImGui::End(); });
      data->callbacks.push_back([] { ImGui::ShowDemoWindow(); ImPlot::ShowDemoWindow(); });
      data->callbacks.push_back([] { ImGui::ShowAboutWindow(); });
      data->toggle_button_circle_radius_font_relative_scale = (8.0f / 20.0f);
      data->toggle_button_width_scale = 1.625f;
      data->toggle_button_circle_num_segments = 40;
      ImStrncpy(data->toggle_button_label, "##toggle", sizeof(data->toggle_button_label));

      data->debug_console_arrow_scale = 1.0f;
      data->debug_console_arrow_line_width = 1.5f;
      data->plot_scale = ImPlotScale_SymLog;
      data->use_implot = true;
      data->use_implot_colormap = false;
      data->plot_colormap = ImPlotColormap_Cool;
      data->plot_col_coeff = 1.5f;
      ImStrncpy(data->plot_label, "##frametimes", sizeof(data->plot_label));
    }

    virtual void OnUpdate(float dt, DebugBarData* data, const DebugBarTempData* temp_data, const DebugBarTempData* last_temp_data) const override final
    {
      if (temp_data->enabled ^ last_temp_data->enabled)
      {
        data->animating = true;
        data->anim_time = 0.0f;
        data->duration = 0.10f;
        data->t_value = 0.0f;
        data->enabled = temp_data->enabled;
      }

      if (data->animating)
      {
        data->anim_time += dt;
        float t = data->anim_time / data->duration;

        if (t >= 1.0f)
        {
          t = 1.0f;
          data->animating = false;
        }

        // choose tween
        float tween;
        // tween = TweenLinear(t);
        // tween = TweenExponential(t);
        tween = TweenHarmonic(t);
        data->t_value = data->enabled ? tween : (1.0f - tween);
      }

      data->idx = temp_data->idx;
      if (100 > data->frame_times.size())
        data->frame_times.push_back(dt);
      else
      {
        std::rotate(data->frame_times.begin(), data->frame_times.begin() + 1, data->frame_times.end());

        if (0.0f >= data->scale_max)
        {
          data->scale_max = *std::max_element(data->frame_times.begin(), data->frame_times.end());
        }
        else
          data->scale_max = ImMax(dt, data->scale_max);

        data->frame_times.back() = dt;
        data->accum_time += dt;
      }
    }

    virtual void OnRender(const DebugBarData* data, DebugBarTempData* temp_data) const override final
    {
      const ImGuiIO& io = ImGui::GetIO();
      char buf[128];

      bool enabled = data->enabled && (1.0f == data->t_value);
      bool show = (0.0f < data->t_value);

      static const auto get_width = [](const char* str) -> float { return ImGui::CalcTextSize(str, NULL, true).x + 2.0f * ImGui::GetStyle().FramePadding.x; };
      static const auto toggle_text_button = [=](const char* str, int idx) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(((1 << idx) & data->idx) ? ImGuiCol_ButtonActive : ImGuiCol_Button));
        temp_data->idx ^= (1 << idx) * ImGui::Button(str, ImVec2(get_width(str), ImGui::GetFrameHeightWithSpacing())); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
        };

      ImVec4 cir_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);

      ImGui::PushStyleColor(ImGuiCol_Text, ImLerp(ImVec4(), ImGui::GetStyleColorVec4(ImGuiCol_Text), data->t_value));
      ImGui::PushStyleColor(ImGuiCol_Separator, ImLerp(ImVec4(), ImGui::GetStyleColorVec4(ImGuiCol_Separator), data->t_value));
      ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetColorU32(ImGuiCol_Text));

      ImVec2 size(ImGui::GetFrameHeight() * data->toggle_button_width_scale, ImGui::GetFrameHeight());
      temp_data->enabled = data->enabled;
      temp_data->enabled ^= ImGui::InvisibleButton(data->toggle_button_label, size);
      ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
      ImDrawList* dl = ImGui::GetWindowDrawList();
      ImRect mr = r;
      ImVec2 sz = r.GetSize();
      float rr = data->toggle_button_circle_radius_font_relative_scale * dl->_Data->FontSize;
      mr.Min += sz * (0.5f * (1.0f - 0.75f));
      mr.Max -= sz * (0.5f * (1.0f - 0.75f));

      ImVec4 col = ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Button), ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), data->t_value);
      dl->AddRectFilled(mr.Min, mr.Max, ImColor(col), 2 * rr, ImDrawFlags_RoundCornersAll);

      ImVec2 c = mr.GetCenter();
      c += ImVec2(ImLerp(-0.25f * mr.GetWidth(), 0.25f * mr.GetWidth(), data->t_value), 0.0f);
      dl->AddCircleFilled(c, rr, ImGui::GetColorU32(cir_col), data->toggle_button_circle_num_segments);

      ImGui::SameLine();
      ImGui::BeginDisabled(!enabled);
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();

      ImGui::AlignTextToFramePadding();

      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
      temp_data->idx = data->idx;

      if (show)
      {
        char buf2[64];
        ImVec2 pos2 = ImGui::GetCursorScreenPos();
        ImFormatString(buf2, sizeof(buf2), " %c   ##Console", ((ImS64)ImRound64(data->accum_time) % 2) ? '_' : ' ');
        toggle_text_button(buf2,    1);
        ImVec2 offset = ImGui::GetStyle().FramePadding * ImVec2(0.0f, 1.0f);
        RenderArrowPolyline(dl, pos2 + offset, ImGui::GetColorU32(ImGuiCol_Text), ImGuiDir_Right, data->debug_console_arrow_scale, data->debug_console_arrow_line_width);

        //toggle_text_button("Debug", 2);
        //toggle_text_button("Style", 3);
        //toggle_text_button("Demos", 4);
        //toggle_text_button("About", 5);
      }

      ImFormatString(buf, sizeof(buf), "%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
      float w = ImGui::CalcTextSize(buf).x;
      ImVec2 pos = ImGui::GetCursorScreenPos();
      ImVec2 offset = ImVec2(ImGui::GetContentRegionAvail().x - w, 0.0f);

      pos.x += ImMax(0.0f, offset.x);

      if (show)
      {
        ImVec2 plot_size = ImVec2(ImMax(4.0f, ImGui::GetContentRegionAvail().x), ImGui::GetFrameHeight());
        ImVec4 plot_col = ImVec4(data->plot_col_coeff, data->plot_col_coeff, data->plot_col_coeff, 1.0f) * col;

        if (data->use_implot)
        {
          if (data->use_implot_colormap)
          {
            ImPlot::PushColormap(data->plot_colormap);
            plot_col = ImPlot::GetColormapColor(0);
            ImPlot::PopColormap();
          }

          ScaledSparkline(data->plot_label, data->frame_times.Data, data->frame_times.size(), 0.0f, data->scale_max, 0, plot_col, plot_size, data->plot_scale);
        }
        else
        {
          ImGui::PlotLines(data->plot_label, data->frame_times.Data, data->frame_times.Size, 0, nullptr, 0.0f, data->scale_max, plot_size);
        }

        ImGui::SameLine();
      }

      ImGui::SetCursorScreenPos(pos);
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); ImGui::SameLine();
      pos = ImGui::GetCursorScreenPos();
      ImFormatString(buf, sizeof(buf), "%.3f ms/frame ", 1000.0f / io.Framerate);
      dl->AddRectFilled(pos, pos + ImGui::CalcTextSize(buf) + ImGui::GetStyle().FramePadding * 2, ImColor(ImGui::GetColorU32(ImGuiCol_WindowBg, 0.6f)));
      ImGui::Text("%s", buf); ImGui::SameLine();

      pos = ImGui::GetCursorScreenPos();
      pos.x = ImMin(pos.x, pos.x + offset.x);
      ImGui::SetCursorScreenPos(pos);
      ImFormatString(buf, sizeof(buf), "(%.1f FPS)", io.Framerate);
      dl->AddRectFilled(pos, pos + ImGui::CalcTextSize(buf) + ImGui::GetStyle().FramePadding * 2, ImColor(ImGui::GetColorU32(ImGuiCol_WindowBg, 0.6f)));
      ImGui::Text("%s", buf);
      ImGui::PopStyleColor();

      if (show)
      {
        int i = 1;
        for (auto& cb : data->callbacks)
          if ((1 << (i++)) & data->idx)
            cb();
      }

      ImGui::EndDisabled();
      ImGui::PopStyleColor();
      ImGui::PopStyleColor();
      ImGui::PopStyleColor();
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

      if (!ImPlot::GetCurrentContext())
        ImPlot::CreateContext();

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
