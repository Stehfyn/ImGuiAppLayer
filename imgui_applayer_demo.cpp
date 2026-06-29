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

  // Reflection-capable dataflow payloads: the scalar data that actually flows between nodes.
  // The live control structs (RandomTimeData/BreathingControlData) carry char[] label/type
  // buffers, which are outside the reflection subset (reflect miscounts raw arrays), so node
  // rows reflect these focused aggregates instead. This is the shape codegen will emit.
  struct RandomTimeNodeData  { float max_timer_secs; };
  struct BreathingNodeData   { float timer_secs; };

  // imnodes data-flow graph: shows the Breathing control's duration input being fed by the
  // Random Time control (its source) when active, or a Default node otherwise.
  void ShowDataFlowGraph(ImGuiApp* app, bool show_random_time, bool show_breathing, ImVector<ImGuiAppNodeDraft>* drafts,
                         ImVector<ImGuiAppNodeLink>* links, int* next_link_id)
  {
    IM_ASSERT(app != nullptr);

    // Fixed nodes/attrs use the low ids; drafts get their own id ranges so an arbitrary number of
    // them never collides with the fixed nodes (or each other).
    enum { NODE_RT = 1, NODE_DEFAULT, NODE_BREATHING, ATTR_RT_OUT = 10, ATTR_DEFAULT_OUT, ATTR_BR_IN,
           NODE_DRAFT_BASE = 100, ATTR_DRAFT_BASE = 200 };

    RandomTimeData*             rt = static_cast<RandomTimeData*>(app->Data.GetVoidPtr(ImGuiType<RandomTimeData>::ID));
    const BreathingControlData* br = static_cast<const BreathingControlData*>(app->Data.GetVoidPtr(ImGuiType<BreathingControlData>::ID));

    static bool placed_rt = false, placed_default = false, placed_breathing = false;

    ImNodes::BeginNodeEditor();

    // Positions must be set BEFORE BeginNode: imnodes lays out node content at the node's current
    // origin and later (in EndNodeEditor) calls SetCursorPos(origin) to draw it. Setting the origin
    // after EndNode leaves content at the old origin while the draw cursor jumps to the new one,
    // past the canvas content max -> trips imgui's "extend boundaries" assert.
    // Node titles come from reflect::type_name; field rows come from reflection over the control's
    // data struct (DrawAppNodeFields) instead of a hand-listed set of members. Ports still carry the
    // dataflow semantics (an output "source" on the duration provider, an input on Breathing).
    if (show_random_time)
    {
      if (!placed_rt) { ImNodes::SetNodeGridSpacePos(NODE_RT, ImVec2(24.0f, 24.0f)); placed_rt = true; }
      ImGui::BeginAppNode(NODE_RT, "Random Time");
      // Inline edit: reflect the scalar payload, edit it, write the change back to the live control.
      RandomTimeNodeData rt_node = { rt ? rt->max_timer_secs : 0.0f };
      if (ImGui::EditAppNodeFields(&rt_node) && rt)
        rt->max_timer_secs = rt_node.max_timer_secs;
      ImNodes::BeginOutputAttribute(ATTR_RT_OUT); ImGui::TextUnformatted("source"); ImNodes::EndOutputAttribute();
      ImGui::EndAppNode();
    }
    else if (show_breathing)
    {
      if (!placed_default) { ImNodes::SetNodeGridSpacePos(NODE_DEFAULT, ImVec2(24.0f, 24.0f)); placed_default = true; }
      ImGui::BeginAppNode(NODE_DEFAULT, "Default");
      ImGui::Text("%.1f s", 5.0f);
      ImNodes::BeginOutputAttribute(ATTR_DEFAULT_OUT); ImGui::TextUnformatted("source"); ImNodes::EndOutputAttribute();
      ImGui::EndAppNode();
    }

    if (show_breathing)
    {
      if (!placed_breathing) { ImNodes::SetNodeGridSpacePos(NODE_BREATHING, ImVec2(224.0f, 48.0f)); placed_breathing = true; }
      ImGui::BeginAppNode(NODE_BREATHING, "Breathing");
      ImNodes::BeginInputAttribute(ATTR_BR_IN); ImGui::TextUnformatted("duration"); ImNodes::EndInputAttribute();
      BreathingNodeData br_node = { br ? br->timer_secs : 0.0f };
      ImGui::DrawAppNodeFields(&br_node);
      ImGui::EndAppNode();
    }

    // Design-phase nodes: each draft is edited in place, inside its own node. Interactive widgets
    // inside an imnodes node must live in an attribute (here a static one) so the canvas does not
    // pan/drag while the user types or opens the type combos. The user adds drafts from the side
    // panel; each new one is positioned once (staggered) and dragged freely thereafter.
    int draft_to_remove = -1;
    if (drafts)
    {
      static int placed_draft_count = 0;
      static int editing_node_id = -1;               // which draft node's title is being renamed (-1 = none)
      for (int i = 0; i < drafts->Size; i++)
      {
        ImGuiAppNodeDraft* draft = &drafts->Data[i];
        const int node_id = NODE_DRAFT_BASE + i;
        const int attr_id = ATTR_DRAFT_BASE + i;

        if (i >= placed_draft_count)
          ImNodes::SetNodeGridSpacePos(node_id, ImVec2(224.0f + (i % 3) * 48.0f, 200.0f + (i % 3) * 48.0f));

        // Title is the name: click it to rename inline. The body holds just the fields now.
        ImGui::BeginAppNodeRenamable(node_id, draft->Name, IM_ARRAYSIZE(draft->Name), &editing_node_id);
        ImNodes::BeginStaticAttribute(attr_id);
        ImGui::PushID(i);                              // disambiguate the per-row widget ids across drafts
        ImGui::EditAppNodeDraftFields(draft);
        if (ImGui::SmallButton("Delete node"))
          draft_to_remove = i;
        ImGui::PopID();
        ImNodes::EndStaticAttribute();
        ImGui::EndAppNode();
      }
      if (drafts->Size > placed_draft_count)
        placed_draft_count = drafts->Size;

      // Deferred erase: mutating the vector mid-draw would invalidate the draft pointers above.
      if (draft_to_remove >= 0)
      {
        drafts->erase(drafts->Data + draft_to_remove);
        placed_draft_count = drafts->Size;            // re-place remaining nodes so ids shift cleanly
      }
    }

    // Links come from the model: the user drags between ports to connect (captured below).
    ImGui::DrawAppNodeLinks(links);

    ImNodes::EndNodeEditor();

    ImGui::CaptureAppNodeLinks(links, next_link_id);
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
        if (ImGui::Begin("ImGuiAppLayer Metrics/Debugger", &show_metrics))
        {
          // Persistent drafts the user designs in the graph; each renders as a live node. Links the
          // user drags between ports live in the model alongside them. Seed one draft on first use.
          static ImVector<ImGuiAppNodeDraft> drafts;
          static ImVector<ImGuiAppNodeLink> links;
          static int next_link_id = 1000;
          static const char* graph_path = "imguix_node_graph.txt";
          static const char* header_path = "imguix_generated_control.h";
          static ImGuiTextBuffer code;
          static float code_w = 0.0f;                    // code-panel width; 0 == collapsed (default)
          static char write_msg[64] = "";                // transient "wrote header" confirmation
          if (drafts.empty())
            drafts.push_back(ImGuiAppNodeDraft());

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
              drafts.push_back(ImGuiAppNodeDraft());
            ImGui::SetItemTooltip("Add a draft control node to the canvas");

            ToolSep();

            // [Graph I/O] -- the popup hit-tests the Save button, so capture hover BEFORE the popup
            // and emit the tooltip AFTER, so neither clobbers the other's last-item id.
            if (ImGui::Button("Save"))
              ImGui::SaveAppNodeGraphMulti(graph_path, &drafts, &links);
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
              ImGui::LoadAppNodeGraphMulti(graph_path, &drafts, &links);
            ImGui::SetItemTooltip("Load graph <- %s", graph_path);

            ToolSep();

            // [Codegen]
            if (ImGui::Button("Generate"))
            {
              code.clear();
              for (int i = 0; i < drafts.Size; i++)      // one control per drafted node
                ImGui::GenerateAppControlCode(&drafts.Data[i], &code);
              if (code_w <= 0.0f) code_w = em * 22.0f;   // auto-reveal so output is never hidden
              write_msg[0] = 0;
            }
            ImGui::SetItemTooltip("Generate C++ from %d node(s)", drafts.Size);

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
            HelpMarker("Drag from a node's output port to another node's input port to connect. "
                       "Click a node's title to rename it; per-node edit and Delete live inside each node. "
                       "Right-click Save for its file path.");

            // Right-aligned live status: FPS (color-coded) | nodes | links | C/W/S.
            char fps_buf[24], rest_buf[96];
            ImFormatString(fps_buf, IM_ARRAYSIZE(fps_buf), "%.0f FPS", io.Framerate);
            ImFormatString(rest_buf, IM_ARRAYSIZE(rest_buf), "  nodes %d  links %d   C%d W%d S%d",
                drafts.Size, links.Size, app.Controls.Size, app.Windows.Size, app.Sidebars.Size);
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
          const float min_canvas = em * 20.0f;           // canvas never starves
          const float grip_w = (code_w > 0.0f) ? em * 0.5f : 0.0f;
          const float max_code = ImMax(0.0f, body.x - grip_w - min_canvas);   // clamp ceiling, never negative
          code_w = ImClamp(code_w, 0.0f, max_code);
          const float canvas_w = ImMax(0.0f, body.x - code_w - grip_w);

          ImGui::BeginChild("##NodeGraph", ImVec2(canvas_w, 0.0f), ImGuiChildFlags_Borders);
          {
            if (links.Size == 0)                         // transient hint, gone after the first link
              ImGui::TextDisabled("Tip: drag between node ports to connect");
            ShowDataFlowGraph(&app, show_random_time, show_breathing, &drafts, &links, &next_link_id);
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
