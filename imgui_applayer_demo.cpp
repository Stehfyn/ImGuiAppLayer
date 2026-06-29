// ImGuiAppLayer demo. Mirrors Dear ImGui's split of imgui.cpp / imgui_demo.cpp and its demo
// window layout: a single "ImGuiAppLayer Demo" window with collapsing-header sections that drive
// a live ImGuiApp, pushing/popping the real layers, windows, sidebars and controls so each
// framework feature (incl. the Breathing control and its Random Time dependency) is showcased.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer.h"
#include "imgui_applayer_nodes.h"
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

      ImStrncpy(data->type, sv.data(), sv.length() + 1); // +1: ImStrncpy copies count-1 chars
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

      ImStrncpy(data->type, sv.data(), sv.length() + 1); // +1: ImStrncpy copies count-1 chars
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

  // Seed a fresh graph. The graph IS the app: the collective of layer/window/control nodes composes it,
  // so there is no "App" container node. Seed one window + one control to show the two pillars at once.
  void SeedAppGraph(ImGuiAppGraph* graph)
  {
    ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Window,  "MainWindow");
    ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Control, "NewControl");
  }

  // "+ Add node" palette: layers, windows, sidebars, controls -- the pieces that compose the app. Builtin
  // controls are backed by the demo's compiled types (RandomTime / Breathing) so their real data types
  // become wireable graph nodes.
  void ShowNodePalette(ImGuiAppGraph* graph)
  {
    if (ImGui::BeginMenu("Control"))
    {
      if (ImGui::MenuItem("New control (draft)"))
        ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Control, "NewControl");
      ImGui::Separator();
      if (ImGui::MenuItem("Random Time (builtin)"))
        ImGui::AppGraphAddBuiltin(graph, ImGuiAppNodeKind_Control, "RandomTime", "RandomTimeData");
      if (ImGui::MenuItem("Breathing (builtin)"))
        ImGui::AppGraphAddBuiltin(graph, ImGuiAppNodeKind_Control, "Breathing", "BreathingData");
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Layer"))
    {
      if (ImGui::MenuItem("Task"))    { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "TaskLayer");    n->LayerType = ImGuiAppLayerType_Task;    }
      if (ImGui::MenuItem("Command")) { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "CommandLayer"); n->LayerType = ImGuiAppLayerType_Command; }
      if (ImGui::MenuItem("Status"))  { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "StatusLayer");  n->LayerType = ImGuiAppLayerType_Status;  }
      if (ImGui::MenuItem("Window"))  { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "WindowLayer");  n->LayerType = ImGuiAppLayerType_Window;  }
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Window"))  ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Window,  "Window");
    if (ImGui::MenuItem("Sidebar")) ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Sidebar, "Sidebar");
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
        ImGui::SetNextWindowSize(ImVec2(em * 64.0f, em * 34.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(em * 46.0f, em * 24.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowBgAlpha(1.0f);    // opaque: no other-window text bleeding through the editor
        if (ImGui::Begin("ImGuiAppLayer Metrics/Debugger", &show_metrics))
        {
          // The whole authored object-model graph: App/Layer/Window/Sidebar/Control nodes, typed
          // containment + data-dependency links, and field bindings -- one owned model with a single
          // monotonic id allocator. Seeded on first use so the canvas is not empty.
          static ImGuiAppGraph graph;
          static const char* graph_path = "imguix_node_graph.txt";
          static const char* header_path = "imguix_generated_control.h";
          static ImGuiTextBuffer code;
          static float code_w = 0.0f;                    // code-panel width; 0 == collapsed (default)
          static char write_msg[64] = "";                // transient "wrote header" confirmation
          static bool mirror_live = true;                // overlay read-only nodes mirrored from the live app (on by default)
          if (graph.Nodes.empty())
            SeedAppGraph(&graph);

          const ImGuiIO& io = ImGui::GetIO();
          const ImGuiStyle& style = ImGui::GetStyle();

          auto ToolSep = [&]()                           // vertical rule between toolbar groups
          {
            ImGui::SameLine(0.0f, em * 0.6f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);   // imgui_internal.h (already included)
            ImGui::SameLine(0.0f, em * 0.6f);
          };
          auto HelpMarker = [](const char* desc)         // always-visible discoverability
          {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("(?)");
            ImGui::SetItemTooltip("%s", desc);
          };

          // -------------------------------------------------------------------------------
          // 1) Framed segmented toolbar: one subtle-framed, auto-height row, grouped by rules.
          // -------------------------------------------------------------------------------
          ImGui::BeginChild("##Toolbar", ImVec2(0.0f, 0.0f), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY);
          {
            // [Node]
            if (ImGui::Button("+ Add node"))
              ImGui::OpenPopup("##addnode");
            ImGui::SetItemTooltip("Add a node (App / Layer / Window / Sidebar / Control) to the canvas");
            if (ImGui::BeginPopup("##addnode"))
            {
              ShowNodePalette(&graph);
              ImGui::EndPopup();
            }

            ToolSep();

            // [Graph I/O] -- the popup hit-tests the Save button, so capture hover BEFORE the popup
            // and emit the tooltip AFTER, so neither clobbers the other's last-item id.
            if (ImGui::Button("Save"))
              ImGui::SaveAppGraph(graph_path, &graph);
            const bool save_hovered = ImGui::IsItemHovered();
            if (ImGui::BeginPopupContextItem("##savepath"))
            {
              ImGui::TextDisabled("Graph file:");
              ImGui::TextUnformatted(graph_path);
              ImGui::EndPopup();
            }
            if (save_hovered)
              ImGui::SetTooltip("Save graph -> %s  (right-click for path)", graph_path);
            ImGui::SameLine();
            if (ImGui::Button("Load"))
              ImGui::LoadAppGraph(graph_path, &graph);
            ImGui::SetItemTooltip("Load graph <- %s", graph_path);

            ToolSep();

            // [Codegen]
            if (ImGui::Button("Generate"))
            {
              code.clear();
              ImGui::GenerateAppGraphCode(&graph, &code);   // whole graph: structs + deps + bring-up, topo-ordered
              if (code_w <= 0.0f) code_w = em * 22.0f;   // auto-reveal so output is never hidden
              write_msg[0] = 0;
            }
            ImGui::SetItemTooltip("Generate C++ from %d node(s)", graph.Nodes.Size);

            ImGui::SameLine();
            ImGui::BeginDisabled(code.size() == 0);      // nothing to write until generated
            if (ImGui::Button("Write .h"))
            {
              if (ImFileHandle fh = ImFileOpen(header_path, "wt"))
              {
                ImFileWrite(code.c_str(), sizeof(char), (ImU64)code.size(), fh);
                ImFileClose(fh);
                ImFormatString(write_msg, IM_ARRAYSIZE(write_msg), "wrote %s", header_path);
              }
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))   // fires even while greyed out
              ImGui::SetTooltip(code.size() > 0 ? "Write generated C++ -> %s" : "Generate code first", header_path);

            ImGui::SameLine();
            const bool code_open = code_w > 0.0f;        // latch before the button (stack-safe tint)
            if (code_open) ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button(code_open ? "Code v" : "Code >"))
              code_w = code_open ? 0.0f : em * 22.0f;    // mutates code_w, NOT code_open
            if (code_open) ImGui::PopStyleColor();        // gated on the same latch -> always balanced
            ImGui::SetItemTooltip("Show / hide the generated-code panel");

            ToolSep();

            // [Live] mirror the running app's controls as read-only nodes.
            ImGui::Checkbox("Mirror live", &mirror_live);
            ImGui::SetItemTooltip("Overlay read-only nodes mirrored from the running app's controls (Examples menu).");

            ToolSep();
            HelpMarker("Drag from a node's output port to another node's input port to connect. "
                       "Click a node's title to rename it; per-node edit and Delete live inside each node. "
                       "Right-click Save for its file path.");

            // Right-aligned live status: FPS (color-coded) | nodes | links | topo | C/W/S.
            ImVector<int> topo_order;
            char topo_err[128];
            const bool topo_ok = ImGui::AppGraphTopoOrder(&graph, &topo_order, topo_err, IM_ARRAYSIZE(topo_err));
            char fps_buf[24], rest_buf[112];
            ImFormatString(fps_buf, IM_ARRAYSIZE(fps_buf), "%.0f FPS", io.Framerate);
            ImFormatString(rest_buf, IM_ARRAYSIZE(rest_buf), "  nodes %d  links %d  %s   C%d W%d S%d",
                graph.Nodes.Size, graph.Links.Size, topo_ok ? "ok" : "CYCLE", app.Controls.Size, app.Windows.Size, app.Sidebars.Size);
            const float cluster_w = ImGui::CalcTextSize(fps_buf).x + ImGui::CalcTextSize(rest_buf).x;
            ImGui::SameLine();
            const float rx = ImGui::GetContentRegionMax().x - cluster_w;   // exact right edge (window-local)
            if (rx > ImGui::GetCursorPosX())             // skip if it would overlap the buttons
              ImGui::SetCursorPosX(rx);
            ImGui::AlignTextToFramePadding();
            const ImVec4 fps_col = io.Framerate >= 60.0f ? ImVec4(0.45f, 0.85f, 0.45f, 1.0f)
                                 : io.Framerate >= 30.0f ? ImVec4(0.90f, 0.80f, 0.35f, 1.0f)
                                                         : ImVec4(0.90f, 0.45f, 0.45f, 1.0f);
            ImGui::TextColored(fps_col, "%s", fps_buf);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextDisabled("%s", rest_buf);
          }
          ImGui::EndChild();

          // -------------------------------------------------------------------------------
          // 2) Body: canvas (hero) + draggable vertical splitter + collapsible code panel.
          // -------------------------------------------------------------------------------
          const ImVec2 body = ImGui::GetContentRegionAvail();
          const float tree_origin_x = ImGui::GetCursorScreenPos().x;   // screen-space left edge of the tree child
          static float tree_w = 0.0f;                    // left outliner width (resizable)
          if (tree_w <= 0.0f) tree_w = em * 16.0f;       // sensible default on first show (fits layer type names)
          const float tree_grip = em * 0.5f;
          const float min_canvas = em * 16.0f;           // canvas never starves
          const float grip_w = (code_w > 0.0f) ? em * 0.5f : 0.0f;

          // Resize by pinning the tree's right edge to the mouse, computed BEFORE rendering the child so the
          // edge tracks the cursor with no per-frame lag or drift. tree_grab_dx is the offset within the grip
          // captured at grab time, so the edge doesn't jump when you click.
          static bool  tree_drag = false;
          static float tree_grab_dx = 0.0f;
          if (tree_drag)
          {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) tree_drag = false;
            else tree_w = io.MousePos.x - tree_grab_dx - tree_origin_x;
          }

          const float max_code = ImMax(0.0f, body.x - tree_w - tree_grip - grip_w - min_canvas);
          code_w = ImClamp(code_w, 0.0f, max_code);
          tree_w = ImClamp(tree_w, em * 9.0f, ImMax(em * 9.0f, body.x - tree_grip - code_w - grip_w - min_canvas));
          const float canvas_w = ImMax(0.0f, body.x - tree_w - tree_grip - code_w - grip_w);

          // Left: scene-hierarchy tree (resizable) communicating the whole app + the authored graph.
          static int tree_sel = -1;
          ImGui::BeginChild("##Tree", ImVec2(tree_w, 0.0f), ImGuiChildFlags_Borders);
          {
            ImGui::ShowAppGraphTree(&app, &graph, &tree_sel);
          }
          ImGui::EndChild();

          // Tree/canvas splitter.
          ImGui::SameLine(0.0f, 0.0f);
          ImGui::InvisibleButton("##tsplit", ImVec2(tree_grip, body.y));
          if (ImGui::IsItemActivated()) { tree_drag = true; tree_grab_dx = io.MousePos.x - ImGui::GetItemRectMin().x; }
          if (tree_drag || ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
          {
            const ImVec2 tr0 = ImGui::GetItemRectMin(), tr1 = ImGui::GetItemRectMax();
            const float tcx = (tr0.x + tr1.x) * 0.5f;
            const ImU32 tcol = ImGui::GetColorU32(tree_drag ? ImGuiCol_SeparatorActive
                                                 : ImGui::IsItemHovered() ? ImGuiCol_SeparatorHovered
                                                                          : ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddLine(ImVec2(tcx, tr0.y), ImVec2(tcx, tr1.y), tcol, 1.0f);
          }
          ImGui::SameLine(0.0f, 0.0f);

          ImGui::BeginChild("##NodeGraph", ImVec2(canvas_w, 0.0f), ImGuiChildFlags_Borders);
          {
            if (graph.Links.Size == 0)                   // transient hint, gone after the first link
              ImGui::TextDisabled("Tip: drag a node's output port to another node's input port to connect");
            if (mirror_live)
              ImGui::BuildAppLiveGraph(&app, &graph);
            else
            {
              // Mirror off: strip the mirrored (live) nodes so the canvas shows only authored design nodes.
              ImVector<int> live_ids;
              for (int i = 0; i < graph.Nodes.Size; i++)
                if (graph.Nodes.Data[i].IsLive) live_ids.push_back(graph.Nodes.Data[i].Id);
              for (int i = 0; i < live_ids.Size; i++)
                ImGui::AppGraphRemoveNode(&graph, live_ids.Data[i]);
            }
            ShowAppGraphEditor(&app, &graph);
          }
          ImGui::EndChild();

          if (code_w > 0.0f)
          {
            // Vertical splitter grip: emitted in the PARENT window AFTER EndChild(##NodeGraph), i.e.
            // fully outside BeginNodeEditor/EndNodeEditor, so it can never pan the canvas.
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::InvisibleButton("##vsplit", ImVec2(grip_w, body.y));
            const bool sp_active = ImGui::IsItemActive();
            const bool sp_hover = ImGui::IsItemHovered();
            if (sp_active) code_w -= io.MouseDelta.x;     // drag left widens the panel
            if (sp_active || sp_hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            const ImU32 gc = ImGui::GetColorU32(sp_active ? ImGuiCol_SeparatorActive
                                              : sp_hover ? ImGuiCol_SeparatorHovered
                                                         : ImGuiCol_Separator);
            const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
            const float cx = (r0.x + r1.x) * 0.5f;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(cx, r0.y), ImVec2(cx, r1.y), gc, 1.0f);

            code_w = ImClamp(code_w, 0.0f, max_code);     // re-clamp after the drag
            if (!sp_active && code_w < em * 6.0f) code_w = 0.0f;   // release below threshold -> snap shut

            if (code_w > 0.0f)                            // re-test AFTER snap: do not submit the panel on
            {                                             // the close frame (a width-0 child fills remaining)
              ImGui::SameLine(0.0f, 0.0f);
              ImGui::BeginChild("##CodePanel", ImVec2(code_w, 0.0f), ImGuiChildFlags_Borders);
              {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("Generated C++");
                if (code.size() > 0)
                {
                  ImGui::SameLine();
                  if (ImGui::SmallButton("Copy"))
                    ImGui::SetClipboardText(code.c_str());
                }
                if (write_msg[0])
                {
                  ImGui::SameLine();
                  ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s", write_msg);
                }
                if (code.size() > 0)
                  ImGui::InputTextMultiline("##code", code.Buf.Data, (size_t)code.Buf.Capacity,
                      ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
                else
                  ImGui::TextDisabled("Press Generate to emit control code.");
              }
              ImGui::EndChild();
            }
          }
        }
        ImGui::End();
      }

      // Reconcile desired -> live app. Full rebuild keeps app storage consistent across toggles. Also fire on
      // first frame (!Initialized) so the framework layers (Task/Command/Status/Window) exist immediately and
      // are visible in the tree/canvas without needing to toggle an example.
      if (!app.IsInitialized() ||
          dirty ||
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
