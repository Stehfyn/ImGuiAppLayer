// ImGuiAppLayer demo. Mirrors Dear ImGui's split of imgui.cpp / imgui_demo.cpp and its demo
// window layout: a single "ImGuiAppLayer Demo" window with collapsing-header sections that drive
// a live ImGuiApp, pushing/popping the real layers, windows, sidebars and controls so each
// framework feature (incl. the Breathing control and its Random Time dependency) is showcased.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer.h"
#include "imgui_internal.h"
#include "imnodes.h"

#include <ctime>
#include <cstdlib>
#include <cstdio>

namespace
{
  // Encourage "pure" design, such that a control is "agnostic to the data passing through it."
  static void RenderTextT(const char* text, ImVec2 text_size, ImVec2 pos, ImVec2 avail, float t_value)
  {
    // Draw directly into the (clipped) window draw list: this is a purely visual centered effect,
    // so it must not move the layout cursor (SetCursorScreenPos past the content max trips
    // imgui's "using SetCursorPos to extend boundaries" assert).
    float offset = avail.x * 0.5f + (t_value * 0.5f * (avail.x - text_size.x));
    ImVec2 text_pos = pos + ImVec2(offset, 0.0f) + (text_size * -0.5f);
    ImGui::GetWindowDrawList()->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), text);
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

  struct RandomTimeControlDemo : ImGuiAppControl <RandomTimeData, RandomTimeTempData>
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

      const ImGuiViewport* vp = ImGui::GetMainViewport();
      const float em = ImGui::GetFontSize();
      ImGui::SetNextWindowSize(ImVec2(em * 14.0f, 0.0f), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + em * 2.0f, vp->WorkPos.y + vp->WorkSize.y * 0.55f), ImGuiCond_FirstUseEver);

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
    const ImGuiApp* app;       // used to read the optional Random Time "source" control
    float timer_secs;
    float t_value;
    float t_direction;
    ImVec4 col;
  };

  struct BreathingControlTempData
  {
    bool hovered;
  };

  struct BreathingControlDemo : ImGuiAppControl<BreathingControlData, BreathingControlTempData>
  {
    // Used when the Random Time "source" control is not active.
    static constexpr float DefaultMaxTimerSecs = 5.0f;

    // Read the Random Time source control's value if that control is active, else the default.
    float SourceMaxTimerSecs(const BreathingControlData* data) const
    {
      if (data->app != nullptr)
        if (const RandomTimeData* src = static_cast<const RandomTimeData*>(data->app->Data.GetVoidPtr(ImGuiType<RandomTimeData>::ID)))
          return src->max_timer_secs;
      return DefaultMaxTimerSecs;
    }

    virtual void OnInitialize(ImGuiApp* app, BreathingControlData* data) const override final
    {
      std::string_view sv;

      data->app = app;

      sv = ImGuiType<decltype(this)>::Name;

      ImStrncpy(data->type, sv.data(), sv.length());
      ImFormatString(data->label, sizeof(data->label), "%s", data->type);
    }

    virtual void OnUpdate(float dt, BreathingControlData* data, const BreathingControlTempData* temp_data, const BreathingControlTempData* last_temp_data) const override final
    {
      data->timer_secs = ImMax(0.0f, data->timer_secs - dt);

      if (temp_data->hovered ^ last_temp_data->hovered)
      {
        // On hover, sample the Random Time source (or the default when that control is inactive).
        data->timer_secs = temp_data->hovered * SourceMaxTimerSecs(data);
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
      const ImGuiViewport* vp = ImGui::GetMainViewport();
      const float em = ImGui::GetFontSize();
      ImGui::SetNextWindowSize(ImVec2(em * 18.0f, em * 9.0f), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + em * 18.0f, vp->WorkPos.y + vp->WorkSize.y * 0.55f), ImGuiCond_FirstUseEver);

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

  // imnodes data-flow graph: shows the Breathing control's duration input being fed by the
  // Random Time control (its source) when active, or a Default node otherwise.
  void ShowDataFlowGraph(const ImGuiApp& app, bool show_random_time, bool show_breathing)
  {
    enum { NODE_RT = 1, NODE_DEFAULT, NODE_BREATHING, ATTR_RT_OUT = 10, ATTR_DEFAULT_OUT, ATTR_BR_IN, LINK_SOURCE = 100 };

    const RandomTimeData*      rt = static_cast<const RandomTimeData*>(app.Data.GetVoidPtr(ImGuiType<RandomTimeData>::ID));
    const BreathingControlData* br = static_cast<const BreathingControlData*>(app.Data.GetVoidPtr(ImGuiType<BreathingControlData>::ID));

    static bool placed_rt = false, placed_default = false, placed_breathing = false;

    ImNodes::BeginNodeEditor();

    // Positions must be set BEFORE BeginNode: imnodes lays out node content at the node's current
    // origin and later (in EndNodeEditor) calls SetCursorPos(origin) to draw it. Setting the origin
    // after EndNode leaves content at the old origin while the draw cursor jumps to the new one,
    // past the canvas content max -> trips imgui's "extend boundaries" assert.
    if (show_random_time)
    {
      if (!placed_rt) { ImNodes::SetNodeGridSpacePos(NODE_RT, ImVec2(24.0f, 24.0f)); placed_rt = true; }
      ImNodes::BeginNode(NODE_RT);
      ImNodes::BeginNodeTitleBar(); ImGui::TextUnformatted("Random Time"); ImNodes::EndNodeTitleBar();
      ImGui::Text("max = %.1f s", rt ? rt->max_timer_secs : 0.0f);
      ImNodes::BeginOutputAttribute(ATTR_RT_OUT); ImGui::TextUnformatted("source"); ImNodes::EndOutputAttribute();
      ImNodes::EndNode();
    }
    else if (show_breathing)
    {
      if (!placed_default) { ImNodes::SetNodeGridSpacePos(NODE_DEFAULT, ImVec2(24.0f, 24.0f)); placed_default = true; }
      ImNodes::BeginNode(NODE_DEFAULT);
      ImNodes::BeginNodeTitleBar(); ImGui::TextUnformatted("Default"); ImNodes::EndNodeTitleBar();
      ImGui::Text("%.1f s", 5.0f);
      ImNodes::BeginOutputAttribute(ATTR_DEFAULT_OUT); ImGui::TextUnformatted("source"); ImNodes::EndOutputAttribute();
      ImNodes::EndNode();
    }

    if (show_breathing)
    {
      if (!placed_breathing) { ImNodes::SetNodeGridSpacePos(NODE_BREATHING, ImVec2(224.0f, 48.0f)); placed_breathing = true; }
      ImNodes::BeginNode(NODE_BREATHING);
      ImNodes::BeginNodeTitleBar(); ImGui::TextUnformatted("Breathing"); ImNodes::EndNodeTitleBar();
      ImNodes::BeginInputAttribute(ATTR_BR_IN); ImGui::TextUnformatted("duration"); ImNodes::EndInputAttribute();
      ImGui::Text("timer = %.1f s", br ? br->timer_secs : 0.0f);
      ImNodes::EndNode();
    }

    if (show_breathing)
      ImNodes::Link(LINK_SOURCE, show_random_time ? ATTR_RT_OUT : ATTR_DEFAULT_OUT, ATTR_BR_IN);

    ImNodes::EndNodeEditor();
  }
}

namespace ImGui
{
  IMGUI_API void ShowAppLayerDemo(bool* p_open)
  {
      static ImGuiApp app;

      // imnodes shares the current ImGui context; create its context once (lives for the session).
      static bool imnodes_ready = false;
      if (!imnodes_ready) { ImNodes::CreateContext(); imnodes_ready = true; }

      // Each example is an independent on/off toggle, like Dear ImGui's Menu > Examples.
      // Everything starts off so the demo is opt-in (nothing spawns until you toggle it).
      static bool show_base_window  = false;
      static bool show_status_bar   = false;
      static bool show_random_time  = false;
      static bool show_breathing    = false;
      static bool applied_base_window = false;
      static bool applied_status_bar  = false;
      static bool applied_random_time = false;
      static bool applied_breathing   = false;
      static bool show_metrics        = false;

      bool dirty = false;

      if (ImGui::Begin("ImGuiAppLayer Demo", p_open, ImGuiWindowFlags_MenuBar))
      {
        if (ImGui::BeginMenuBar())
        {
          if (ImGui::BeginMenu("Examples"))
          {
            dirty |= ImGui::MenuItem("Base window",          nullptr, &show_base_window);
            dirty |= ImGui::MenuItem("Status bar",           nullptr, &show_status_bar);
            dirty |= ImGui::MenuItem("Random Time control",  nullptr, &show_random_time);
            dirty |= ImGui::MenuItem("Breathing control",    nullptr, &show_breathing);
            ImGui::EndMenu();
          }
          if (ImGui::BeginMenu("Tools"))
          {
            ImGui::MenuItem("Metrics/Debugger", nullptr, &show_metrics);
            ImGui::EndMenu();
          }
          ImGui::EndMenuBar();
        }

        ImGui::Text("ImGuiAppLayer says hello! (%s) (%d)", IMGUI_APPLAYER_VERSION, IMGUI_APPLAYER_VERSION_NUM);
        ImGui::Spacing();
        ImGui::TextWrapped("%s", "Enable examples from the Examples menu to push/pop them on a live "
            "ImGuiApp. The Breathing control breathes while hovered for a duration taken from the "
            "Random Time control when it is active (its source), or a default otherwise.");

        if (ImGui::CollapsingHeader("ImGuiApp Status", ImGuiTreeNodeFlags_DefaultOpen))
        {
          const ImGuiIO& io = ImGui::GetIO();
          ImGui::Text("Initialized: %s", app.Initialized ? "yes" : "no");
          ImGui::Text("Layers: %d   Windows: %d   Sidebars: %d   Controls: %d",
              app.Layers.Size, app.Windows.Size, app.Sidebars.Size, app.Controls.Size);
          ImGui::Text("Storage entries: %d   Shutdown pending: %s",
              app.StorageEntries.Size, app.ShutdownPending ? "yes" : "no");
          ImGui::Separator();
          ImGui::Text("Platform: %s", io.BackendPlatformName ? io.BackendPlatformName : "unknown");
          ImGui::Text("Renderer: %s", io.BackendRendererName ? io.BackendRendererName : "unknown");
          ImGui::Text("%.1f FPS (%.3f ms/frame)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
        }
      }
      ImGui::End();

      if (show_metrics)
      {
        const float em = ImGui::GetFontSize();
        ImGui::SetNextWindowSize(ImVec2(em * 32.0f, em * 20.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("ImGuiAppLayer Metrics/Debugger", &show_metrics))
        {
          const ImGuiIO& io = ImGui::GetIO();
          ImGui::Text("%.1f FPS   Controls: %d   Windows: %d   Sidebars: %d",
              io.Framerate, app.Controls.Size, app.Windows.Size, app.Sidebars.Size);
          ImGui::TextUnformatted("Data flow (Random Time / Default -> Breathing):");
          ShowDataFlowGraph(app, show_random_time, show_breathing);
        }
        ImGui::End();
      }

      // Reconcile desired -> live app. Full rebuild keeps app storage consistent across toggles.
      if (dirty ||
          applied_base_window != show_base_window  ||
          applied_status_bar  != show_status_bar   ||
          applied_random_time != show_random_time  ||
          applied_breathing   != show_breathing)
      {
        ShutdownApp(&app);
        InitializeApp(&app);

        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float em = ImGui::GetFontSize();       // text size: drives all sizing, scales with DPI/font

        if (show_base_window)
        {
          PushAppWindow<BaseWindow>(&app);
          ImGuiAppWindowBase* w = app.Windows.back();
          w->HasInitialPlacement = true;
          w->InitialSize = ImVec2(em * 16.0f, em * 8.0f);
          w->InitialPos  = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + em * 2.0f);
        }

        if (show_status_bar)
          PushAppSidebar<StatusBar>(&app, vp, ImGuiDir_Down, 0.0f, ImGuiWindowFlags_AlwaysAutoResize);

        // Controls are app-level: they render their own windows, no host sidebar needed.
        if (show_random_time)
          PushAppControl<RandomTimeControlDemo>(&app);
        if (show_breathing)
          PushAppControl<BreathingControlDemo>(&app);

        applied_base_window = show_base_window;
        applied_status_bar  = show_status_bar;
        applied_random_time = show_random_time;
        applied_breathing   = show_breathing;
      }

      UpdateApp(&app);
      RenderApp(&app);

      if (app.ShutdownPending)
        ShutdownApp(&app);
  }
}
