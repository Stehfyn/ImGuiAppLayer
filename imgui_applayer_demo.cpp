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
#include <cstring>                         // strncmp (codegen-warning scan)

namespace
{
  // Monospace font for the generated-code inspector (set via ImGui::SetAppCodeFont). Null -> UI font.
  static ImFont* g_AppCodeFont = nullptr;

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
      if (ImGui::MenuItem("Task", nullptr, false, !ImGui::AppGraphHasLayerType(graph, ImGuiAppLayerType_Task)))       { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "TaskLayer");    n->LayerType = ImGuiAppLayerType_Task;    }
      if (ImGui::MenuItem("Command", nullptr, false, !ImGui::AppGraphHasLayerType(graph, ImGuiAppLayerType_Command))) { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "CommandLayer"); n->LayerType = ImGuiAppLayerType_Command; }
      if (ImGui::MenuItem("Status", nullptr, false, !ImGui::AppGraphHasLayerType(graph, ImGuiAppLayerType_Status)))   { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "StatusLayer");  n->LayerType = ImGuiAppLayerType_Status;  }
      if (ImGui::MenuItem("Window", nullptr, false, !ImGui::AppGraphHasLayerType(graph, ImGuiAppLayerType_Window)))   { ImGuiAppNode* n = ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Layer, "WindowLayer");  n->LayerType = ImGuiAppLayerType_Window;  }
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Window"))  ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Window,  "Window");
    if (ImGui::MenuItem("Sidebar")) ImGui::AppGraphAddNode(graph, ImGuiAppNodeKind_Sidebar, "Sidebar");
  }

  //---------------------------------------------------------------------------------------------------
  // Dogfooded Metrics/Debugger node editor: the editor is itself an ImGuiApp object model.
  //
  // The graph IS the document. ONE control (GraphDocControl) owns it as PersistData and is the single
  // writer; the panels are controls that receive `const GraphDocData*` as a typed dependency (read), and
  // the interactive panels mutate the doc through that pointer (a localized, deliberate escape from the
  // framework's read-only dependency rule -- the interim until an edit-intent bus lands). TempData is empty
  // on every panel: actions apply immediately, nothing needs to survive to next frame.
  //
  // No ImGuiApp* is passed to OnUpdate/OnRender, so the doc stashes the app it MIRRORS (the demo's example
  // app) in PersistData -- the Breathing-control convention (imgui_applayer_demo.cpp data->app).
  //---------------------------------------------------------------------------------------------------

  // PersistData is value-initialized by PushAppControl<>, then seeded in GraphDocControl::OnInitialize -- no
  // member initializers here (defaults belong to OnInitialize, not the declaration).
  struct GraphDocData
  {
    ImGuiAppGraph   Graph;
    int             Selection;        // node selection shared by tree + canvas
    bool            ShowLive;         // show vs hide (never delete) live-mirror nodes
    float           TreeW;            // outliner width (0 -> default on first use)
    float           CodeH;            // code-inspector height, bottom split under the canvas (0 == collapsed)
    char            WriteMsg[64];     // transient "wrote header" confirmation
    char            GraphPath[256];
    char            HeaderPath[256];
    const ImGuiApp* Mirror;           // app reflected into the live graph (set after push; see ShowAppLayerDemo)
  };
  struct GraphDocTempData {};

  // Mutable fetch of the shared doc from an app's storage (PersistData aliases the start of InstanceData).
  static GraphDocData* GetGraphDoc(ImGuiApp* app)
  {
    return static_cast<GraphDocData*>(app->Data.GetVoidPtr(ImGuiType<GraphDocData>::ID));
  }

  struct GraphDocControl : ImGuiAppControl<GraphDocData, GraphDocTempData>
  {
    virtual void OnInitialize(ImGuiApp* app, GraphDocData* data) const override final
    {
      IM_UNUSED(app);
      data->Selection = -1;
      data->ShowLive  = true;
      data->TreeW     = 0.0f;            // 0 -> EditorBody picks a default on first layout
      data->CodeH     = 0.0f;            // collapsed
      data->WriteMsg[0] = 0;
      data->Mirror    = nullptr;         // set after push by ShowAppLayerDemo
      ImStrncpy(data->GraphPath,  "imguix_node_graph.txt",      sizeof(data->GraphPath));
      ImStrncpy(data->HeaderPath, "imguix_generated_control.h", sizeof(data->HeaderPath));
      if (data->Graph.Nodes.empty())
      {
        SeedAppGraph(&data->Graph);
      }
    }
    virtual void OnUpdate(float dt, GraphDocData* data, const GraphDocTempData*, const GraphDocTempData*) const override final
    {
      IM_UNUSED(dt);
      // Reconcile-before-report: build the live mirror first so every panel reads the reconciled graph this frame.
      if (data->Mirror != nullptr)
      {
        ImGui::BuildAppLiveGraph(data->Mirror, &data->Graph);
      }
    }
  };

  static void EditorToolSep(float em)
  {
    ImGui::SameLine(0.0f, em * 0.6f);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0.0f, em * 0.6f);
  }

  struct ToolbarData {};
  // The button bar's discrete actions are captured here in OnRender and applied in OnUpdate -- TempData IS the
  // edit-intent bus (so file IO and geometry changes leave the render path entirely).
  struct ToolbarTempData
  {
    bool Save;
    bool Load;
    bool WriteHeader;
    bool ToggleCode;
    bool SetShowLive;   // "Show live mirror" checkbox changed this frame
    bool ShowLive;      // its captured value
  };
  struct ToolbarControl : ImGuiAppControl<ToolbarData, ToolbarTempData, GraphDocData>
  {
    virtual void OnUpdate(float dt, ToolbarData*, const ToolbarTempData* in, const ToolbarTempData*, const GraphDocData* cdoc) const override final
    {
      IM_UNUSED(dt);
      GraphDocData* doc = const_cast<GraphDocData*>(cdoc);   // the doc is a read-only dependency; mutating it is the
                                                             // documented interim escape, done here in the logic phase
      if (in->Save)
      {
        ImGui::SaveAppGraph(doc->GraphPath, &doc->Graph);
      }
      if (in->Load)
      {
        ImGui::LoadAppGraph(doc->GraphPath, &doc->Graph);
      }
      if (in->WriteHeader)
      {
        ImGuiTextBuffer full;
        ImGui::GenerateAppGraphCode(&doc->Graph, &full);
        if (ImFileHandle fh = ImFileOpen(doc->HeaderPath, "wt"))
        {
          ImFileWrite(full.c_str(), sizeof(char), (ImU64)full.size(), fh);
          ImFileClose(fh);
          ImFormatString(doc->WriteMsg, IM_ARRAYSIZE(doc->WriteMsg), "wrote %s", doc->HeaderPath);
        }
      }
      if (in->ToggleCode)
      {
        doc->CodeH = (doc->CodeH > 0.0f) ? 0.0f : ImGui::GetFontSize() * 12.0f;
      }
      if (in->SetShowLive)
      {
        doc->ShowLive = in->ShowLive;
      }
    }

    virtual void OnRender(const ToolbarData*, ToolbarTempData* out, const GraphDocData* doc) const override final
    {
      const float em = ImGui::GetFontSize();
      const ImGuiStyle& style = ImGui::GetStyle();
      if (ImGui::BeginChild("##Toolbar", ImVec2(0.0f, 0.0f), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
      {
        if (ImGui::Button("+ Add node"))
        {
          ImGui::OpenPopup("##addnode");
        }
        ImGui::SetItemTooltip("Add a node (Layer / Window / Sidebar / Control) to the canvas");
        // The Add-node palette is a multi-level ImGui popup menu (builtins, layer-type dedup) -- genuinely bound to
        // submission, so it stays a localized, documented graph write rather than a captured intent.
        if (ImGui::BeginPopup("##addnode"))
        {
          ShowNodePalette(const_cast<ImGuiAppGraph*>(&doc->Graph));
          ImGui::EndPopup();
        }

        EditorToolSep(em);
        out->Save = ImGui::Button("Save");
        ImGui::SetItemTooltip("Save graph -> %s", doc->GraphPath);
        ImGui::SameLine();
        out->Load = ImGui::Button("Load");
        ImGui::SetItemTooltip("Load graph <- %s", doc->GraphPath);

        EditorToolSep(em);
        out->WriteHeader = ImGui::Button("Write .h");
        ImGui::SetItemTooltip("Write whole-graph C++ -> %s", doc->HeaderPath);
        ImGui::SameLine();
        const bool code_open = doc->CodeH > 0.0f;
        if (code_open)
        {
          ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
        }
        out->ToggleCode = ImGui::Button(code_open ? "Code v" : "Code >");
        if (code_open)
        {
          ImGui::PopStyleColor();
        }
        ImGui::SetItemTooltip("Show / hide the generated-code inspector");

        EditorToolSep(em);
        bool show_live = doc->ShowLive;
        out->SetShowLive = ImGui::Checkbox("Show live mirror", &show_live);
        out->ShowLive    = show_live;
        ImGui::SetItemTooltip("Hide/show read-only nodes mirrored from the running app. Hiding never deletes your design.");
      }
      ImGui::EndChild();
    }
  };

  // All strip text is derived from the doc in OnUpdate and parked here; OnRender only lays out pills. `Level`
  // selects the pill color (0 disabled, 1 green, 2 amber, 3 red).
  struct StatusStripData
  {
    char GraphMsg[180];                 // "graph ok" or "cycle: ..."
    int  GraphLevel;                    // 1 ok, 3 cycle
    char CountMsg[96];                  // "design N  live N  promoted N"
    bool HasMirror;                     // doc->Mirror != nullptr
    bool MirrorInit;                    // mirror app initialized
    char MirrorCounts[64];              // "L# W# S# C#"
    char Breadcrumb[IM_LABEL_SIZE * 2]; // selection breadcrumb
  };
  struct StatusStripTempData {};   // empty by design: the strip is read-only display, it captures no input
  struct StatusStripControl : ImGuiAppControl<StatusStripData, StatusStripTempData, GraphDocData>
  {
    virtual void OnUpdate(float dt, StatusStripData* data, const StatusStripTempData*, const StatusStripTempData*, const GraphDocData* doc) const override final
    {
      IM_UNUSED(dt);
      // Topo state. The order vector is throwaway scratch for the call; only success/failure feeds the strip.
      ImVector<int> order;
      char err[160] = "";
      if (ImGui::AppGraphTopoOrder(&doc->Graph, &order, err, IM_ARRAYSIZE(err)))
      {
        ImStrncpy(data->GraphMsg, "graph ok", sizeof(data->GraphMsg));
        data->GraphLevel = 1;
      }
      else
      {
        ImFormatString(data->GraphMsg, IM_ARRAYSIZE(data->GraphMsg), "cycle: %s", err[0] ? err : "dependency cycle");
        data->GraphLevel = 3;
      }

      int nd = 0;
      int nl = 0;
      int np = 0;
      for (int i = 0; i < doc->Graph.Nodes.Size; i++)
      {
        const ImGuiAppNode* n = &doc->Graph.Nodes.Data[i];
        if (n->IsLive)
        {
          nl++;
        }
        else
        {
          nd++;
        }
        if (n->IsPromoted)
        {
          np++;
        }
      }
      if (doc->ShowLive)
      {
        ImFormatString(data->CountMsg, IM_ARRAYSIZE(data->CountMsg), "design %d  live %d  promoted %d", nd, nl, np);
      }
      else
      {
        ImFormatString(data->CountMsg, IM_ARRAYSIZE(data->CountMsg), "design %d", nd);
      }

      data->HasMirror = (doc->Mirror != nullptr);
      if (data->HasMirror)
      {
        const ImGuiApp* a = doc->Mirror;
        ImFormatString(data->MirrorCounts, IM_ARRAYSIZE(data->MirrorCounts), "L%d W%d S%d C%d", a->Layers.Size, a->Windows.Size, a->Sidebars.Size, a->Controls.Size);
        data->MirrorInit = a->Initialized;
      }

      ImGui::AppGraphSelectionBreadcrumb(&doc->Graph, doc->Selection, data->Breadcrumb, IM_ARRAYSIZE(data->Breadcrumb));
    }

    virtual void OnRender(const StatusStripData* data, StatusStripTempData*, const GraphDocData*) const override final
    {
      const float em = ImGui::GetFontSize();
      const ImGuiStyle& style = ImGui::GetStyle();
      auto pill = [&](const char* text, int level)
      {
        const ImVec4 col[4] = { style.Colors[ImGuiCol_TextDisabled], ImVec4(0.45f,0.85f,0.45f,1.0f),
                                ImVec4(0.90f,0.80f,0.35f,1.0f), ImVec4(0.90f,0.45f,0.45f,1.0f) };
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(col[level], "%s", text);
      };
      if (ImGui::BeginChild("##Strip", ImVec2(0.0f, 0.0f), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
      {
        pill(data->GraphMsg, data->GraphLevel);
        EditorToolSep(em);
        pill(data->CountMsg, 0);
        if (data->HasMirror)
        {
          EditorToolSep(em);
          pill(data->MirrorCounts, 0);
          EditorToolSep(em);
          pill(data->MirrorInit ? "init" : "uninit", data->MirrorInit ? 1 : 0);
        }
        EditorToolSep(em);
        pill(data->Breadcrumb, 0);
      }
      ImGui::EndChild();
    }
  };

  // PersistData: durable state, mutated only in OnUpdate. CodeText/CodeName are the rendered output (Breathing's
  // timer_text pattern -- derived in OnUpdate, drawn from const data in OnRender).
  struct EditorBodyData
  {
    bool            TreeDragging;            // tree splitter drag FSM (advanced only in OnUpdate)
    float           TreeDragDX;              // grab offset within the grip, captured at drag start
    ImGuiTextBuffer CodeText;                // generated C++ for the selected node
    bool            HasCode;                 // a node is selected and code was generated
    char            CodeName[IM_LABEL_SIZE]; // selected node's draft name (for the panel title)
  };
  // TempData: raw input recorded by OnRender (the only place ImGui item geometry exists) and consumed by OnUpdate.
  // OnRender performs no logic and mutates no state -- it just records what the user did, exactly like the demo's
  // RandomTimeTempData::generate / BreathingControlTempData::hovered.
  struct EditorBodyTempData
  {
    bool  TreeGripActivated;   // drag started on the tree grip this frame
    bool  MouseLeftDown;       // left button held
    float MouseX;              // mouse x (screen)
    float TreeGripMinX;        // tree grip left edge
    float TreeOriginX;         // body row left edge
    bool  CodeGripActive;      // code grip held this frame
    float CodeResolved;        // this frame's clamped code height (drag base)
    float CodeMax;             // this frame's max code height
    float MouseDY;             // mouse y delta while dragging the code grip
    bool  CodeSnapClosed;      // resolved code height collapsed below threshold while idle
    bool  SelectionChanged;    // tree/canvas changed the selection this frame
    int   Selection;           // the new selection
  };
  struct EditorBodyControl : ImGuiAppControl<EditorBodyData, EditorBodyTempData, GraphDocData>
  {
    virtual void OnUpdate(float dt, EditorBodyData* data, const EditorBodyTempData* in, const EditorBodyTempData*, const GraphDocData* cdoc) const override final
    {
      IM_UNUSED(dt);
      GraphDocData* doc = const_cast<GraphDocData*>(cdoc);   // the doc is a read-only dependency; mutating it is the
                                                             // documented interim escape, done here in the logic phase

      // Tree splitter FSM, driven entirely by input captured last render.
      if (in->TreeGripActivated)
      {
        data->TreeDragging = true;
        data->TreeDragDX = in->MouseX - in->TreeGripMinX;
      }
      if (data->TreeDragging)
      {
        if (!in->MouseLeftDown)
        {
          data->TreeDragging = false;
        }
        else
        {
          doc->TreeW = in->MouseX - data->TreeDragDX - in->TreeOriginX;
        }
      }

      // Code splitter: drag up grows the panel; release tiny snaps it closed.
      if (in->CodeGripActive)
      {
        doc->CodeH = ImClamp(in->CodeResolved - in->MouseDY, 0.0f, in->CodeMax);
      }
      else if (in->CodeSnapClosed)
      {
        doc->CodeH = 0.0f;
      }

      if (in->SelectionChanged)
      {
        doc->Selection = in->Selection;
      }

      // Regenerate the inspector text for the current selection while the panel is open.
      data->CodeText.clear();
      data->HasCode = false;
      data->CodeName[0] = 0;
      if (doc->CodeH > 0.0f && doc->Selection >= 0)
      {
        if (ImGuiAppNode* seln = ImGui::AppGraphFindNode(&doc->Graph, doc->Selection))
        {
          ImGui::GenerateAppNodeCode(&doc->Graph, seln, &data->CodeText);
          ImStrncpy(data->CodeName, seln->Draft.Name, sizeof(data->CodeName));
          data->HasCode = true;
        }
      }
    }

    virtual void OnRender(const EditorBodyData* data, EditorBodyTempData* out, const GraphDocData* doc) const override final
    {
      if (doc->Mirror == nullptr)                            // tree + canvas mirror the example app
      {
        return;
      }
      ImGuiApp*      app   = const_cast<ImGuiApp*>(doc->Mirror);      // viewer APIs take non-const; use is read-only
      ImGuiAppGraph* graph = const_cast<ImGuiAppGraph*>(&doc->Graph);

      const float em = ImGui::GetFontSize();
      const ImGuiIO& io = ImGui::GetIO();
      const ImVec2 body = ImGui::GetContentRegionAvail();
      const float tree_origin_x = ImGui::GetCursorScreenPos().x;
      const float tree_grip = em * 0.5f;
      const float min_canvas_w = em * 16.0f;

      // Display-only fit of the persisted TreeW (0 -> default). Local floats; nothing is written to the doc here.
      float tree_w = (doc->TreeW > 0.0f) ? doc->TreeW : em * 16.0f;
      tree_w = ImClamp(tree_w, em * 9.0f, ImMax(em * 9.0f, body.x - tree_grip - min_canvas_w));
      const float right_w = ImMax(0.0f, body.x - tree_w - tree_grip);

      int selection = doc->Selection;

      // Left: tree sidebar (full height).
      if (ImGui::BeginChild("##Tree", ImVec2(tree_w, 0.0f), ImGuiChildFlags_Borders))
      {
        ImGui::ShowAppGraphTree(app, graph, &selection);
      }
      ImGui::EndChild();

      // Tree splitter: record raw input; the drag is resolved in OnUpdate. Cursor is a pure visual.
      ImGui::SameLine(0.0f, 0.0f);
      ImGui::InvisibleButton("##tsplit", ImVec2(tree_grip, body.y));
      out->TreeGripActivated = ImGui::IsItemActivated();
      out->MouseLeftDown     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
      out->MouseX            = io.MousePos.x;
      out->TreeGripMinX      = ImGui::GetItemRectMin().x;
      out->TreeOriginX       = tree_origin_x;
      if (data->TreeDragging || ImGui::IsItemHovered())
      {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      }
      ImGui::SameLine(0.0f, 0.0f);

      // Right column: node graph canvas on top, code inspector on the bottom split.
      if (ImGui::BeginChild("##Right", ImVec2(right_w, 0.0f)))
      {
        const float col_w = ImGui::GetContentRegionAvail().x;
        const float code_grip = (doc->CodeH > 0.0f) ? em * 0.5f : 0.0f;
        const float min_canvas_h = em * 8.0f;
        const float code_max = ImMax(0.0f, body.y - min_canvas_h - code_grip);
        const float code_h = ImClamp(doc->CodeH, 0.0f, code_max);
        const float canvas_h = ImMax(0.0f, body.y - code_h - code_grip);

        if (ImGui::BeginChild("##NodeGraph", ImVec2(col_w, canvas_h), ImGuiChildFlags_Borders))
        {
          ImGui::ShowAppGraphEditor(app, graph, &selection, doc->ShowLive);
        }
        ImGui::EndChild();

        if (code_h > 0.0f)
        {
          // Code splitter: record raw input only.
          ImGui::InvisibleButton("##hsplit", ImVec2(col_w, code_grip));
          out->CodeGripActive = ImGui::IsItemActive();
          out->CodeResolved   = code_h;
          out->CodeMax        = code_max;
          out->MouseDY        = io.MouseDelta.y;
          out->CodeSnapClosed = !out->CodeGripActive && code_h < em * 4.0f;
          if (out->CodeGripActive || ImGui::IsItemHovered())
          {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
          }

          if (ImGui::BeginChild("##CodePanel", ImVec2(col_w, code_h), ImGuiChildFlags_Borders))
          {
            ImGui::AlignTextToFramePadding();
            if (data->HasCode)
            {
              ImGui::TextDisabled("Generated C++ - %s", data->CodeName);
            }
            else
            {
              ImGui::TextDisabled("Generated C++");
            }
            if (data->CodeText.size() > 0)
            {
              ImGui::SameLine();
              if (ImGui::SmallButton("Copy"))
              {
                ImGui::SetClipboardText(data->CodeText.c_str());
              }
            }
            if (doc->WriteMsg[0])
            {
              ImGui::SameLine();
              ImGui::TextColored(ImVec4(0.45f,0.85f,0.45f,1.0f), "%s", doc->WriteMsg);
            }
            if (!data->HasCode)
            {
              ImGui::TextDisabled("Select a node to see its generated code.");
            }
            else
            {
              // Monospace so space-padded column alignment (e.g. the generated enum '=' column) lines up.
              if (g_AppCodeFont)
              {
                ImGui::PushFont(g_AppCodeFont, 0.0f);
              }
              ImGui::InputTextMultiline("##code", data->CodeText.Buf.Data, (size_t)data->CodeText.Buf.Capacity, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
              if (g_AppCodeFont)
              {
                ImGui::PopFont();
              }
            }
          }
          ImGui::EndChild();
        }
      }
      ImGui::EndChild();

      // Record the selection change for OnUpdate to apply (the tree/canvas widgets edited the local copy).
      out->SelectionChanged = (selection != doc->Selection);
      out->Selection = selection;
    }
  };

  // The host window: empty body; its hosted controls (toolbar, strip, body) fill it in push order. Label is
  // fixed (not the type-derived unique label) so the saved .ini dock binding + central-node dock still match.
  struct MetricsDebuggerWindow : ImGuiAppWindow<MetricsDebuggerWindow>
  {
    MetricsDebuggerWindow() { ImStrncpy(this->Label, "ImGuiAppLayer Metrics/Debugger", sizeof(this->Label)); }
    virtual void OnRender(const ImGuiApp*) const override final {}
  };

  // The demo's own control panel, dogfooded as a framework window+control rather than a pile of function statics:
  // the example toggles and their "applied" bookkeeping live in PersistData. ShowAppLayerDemo reads these flags to
  // compose the mirrored example app. The MenuItems are ImGui-bound writes (MenuItem takes a bool*), so the menu is
  // the one render-time mutation here -- the same category as the Show-live checkbox.
  struct DemoMenuData
  {
    bool ShowBaseWindow;
    bool ShowStatusBar;
    bool ShowRandomTime;
    bool ShowBreathing;
    bool ShowMetrics;
    bool AppliedBaseWindow;
    bool AppliedStatusBar;
    bool AppliedRandomTime;
    bool AppliedBreathing;
  };
  struct DemoMenuTempData {};
  struct DemoMenuControl : ImGuiAppControl<DemoMenuData, DemoMenuTempData>
  {
    virtual void OnRender(const DemoMenuData* cdata, DemoMenuTempData*) const override final
    {
      DemoMenuData* data = const_cast<DemoMenuData*>(cdata);
      if (ImGui::BeginMenuBar())
      {
        if (ImGui::BeginMenu("Examples"))
        {
          ImGui::MenuItem("Base window",          nullptr, &data->ShowBaseWindow);
          ImGui::MenuItem("Status bar",           nullptr, &data->ShowStatusBar);
          ImGui::MenuItem("Random Time control",  nullptr, &data->ShowRandomTime);
          ImGui::MenuItem("Breathing control",    nullptr, &data->ShowBreathing);
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
          ImGui::MenuItem("Metrics/Debugger", nullptr, &data->ShowMetrics);
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
        ImGui::TextDisabled("See Tools > Metrics/Debugger -> status strip for composition, lifecycle and FPS.");
      }
    }
  };

  static DemoMenuData* GetDemoMenu(ImGuiApp* app)
  {
    return static_cast<DemoMenuData*>(app->Data.GetVoidPtr(ImGuiType<DemoMenuData>::ID));
  }

  struct DemoPanelWindow : ImGuiAppWindow<DemoPanelWindow>
  {
    DemoPanelWindow() { ImStrncpy(this->Label, "ImGuiAppLayer Demo", sizeof(this->Label)); this->Flags = ImGuiWindowFlags_MenuBar; }
    virtual void OnRender(const ImGuiApp*) const override final {}
  };
}

namespace ImGui
{
  IMGUI_API void SetAppCodeFont(ImFont* font) { g_AppCodeFont = font; }

  IMGUI_API void ShowAppLayerDemo(bool* p_open)
  {
      static ImGuiApp app;          // the mirrored "example" app the demo composes

      // imnodes shares the current ImGui context; create its context once (lives for the session).
      static bool imnodes_ready = false;
      if (!imnodes_ready)
      {
        ImNodes::CreateContext();
        imnodes_ready = true;
      }

      // ---- editor_app: a dedicated, never-rebuilt ImGuiApp hosting BOTH the demo's control panel and the
      //      dogfooded Metrics/Debugger. Pushed once so the graph + toggle state survive example rebuilds. Toggle
      //      state lives in DemoMenuData (PersistData), not function statics. Only the WindowLayer is needed.
      static ImGuiApp editor_app;
      static bool editor_ready = false;
      if (!editor_ready)
      {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::PushAppLayer<ImGuiAppWindowLayer>(&editor_app);

        // Demo control panel (menu + blurb).
        ImGui::PushAppWindow<DemoPanelWindow>(&editor_app);
        ImGuiAppWindowBase* panel = editor_app.Windows.back();
        panel->HasInitialPlacement = true;
        panel->InitialSize = ImVec2(vp->WorkSize.x * 0.30f, vp->WorkSize.y * 0.40f);
        panel->InitialPos  = vp->WorkPos + ImVec2(vp->WorkSize.x * 0.02f, vp->WorkSize.y * 0.04f);
        ImGui::PushWindowControl<DemoMenuControl>(&editor_app, panel);

        // Metrics/Debugger (GraphDoc + Toolbar + StatusStrip + EditorBody).
        ImGui::PushAppWindow<MetricsDebuggerWindow>(&editor_app);
        ImGuiAppWindowBase* metrics = editor_app.Windows.back();
        metrics->HasInitialPlacement = true;
        metrics->InitialSize = ImVec2(vp->WorkSize.x * 0.66f, vp->WorkSize.y * 0.66f);
        metrics->InitialPos  = vp->WorkPos + ImVec2(vp->WorkSize.x * 0.10f, vp->WorkSize.y * 0.10f);
        ImGui::PushWindowControl<GraphDocControl>(&editor_app, metrics);   // producer: owns the doc (push first)
        ImGui::PushWindowControl<ToolbarControl>(&editor_app, metrics);    // consumers depend on GraphDocData
        ImGui::PushWindowControl<StatusStripControl>(&editor_app, metrics);
        ImGui::PushWindowControl<EditorBodyControl>(&editor_app, metrics);
        if (GraphDocData* d = GetGraphDoc(&editor_app))
        {
          d->Mirror = &app;
        }
        editor_ready = true;
      }

      ImGuiAppWindowBase* panel   = editor_app.Windows[0];
      ImGuiAppWindowBase* metrics = editor_app.Windows[1];
      DemoMenuData* st = GetDemoMenu(&editor_app);

      // Drive the framework windows' Open from the external/menu flags, tick the editor app, then read the X
      // buttons back out. The menu (a control on `panel`) toggles st->* during RenderApp.
      panel->Open   = (p_open == nullptr) || *p_open;
      metrics->Open = st->ShowMetrics;
      ImGui::UpdateApp(&editor_app);
      ImGui::RenderApp(&editor_app);
      if (p_open != nullptr)     // panel X closes the whole demo
      {
        *p_open = panel->Open;
      }
      if (!metrics->Open)        // metrics X syncs the menu toggle
      {
        st->ShowMetrics = false;
      }

      // Reconcile desired -> live app. Full rebuild keeps app storage consistent across toggles. Also fire on
      // first frame (!Initialized) so the framework layers (Task/Command/Status/Window) exist immediately and
      // are visible in the tree/canvas without needing to toggle an example.
      if (!app.IsInitialized() ||
          st->AppliedBaseWindow != st->ShowBaseWindow ||
          st->AppliedStatusBar  != st->ShowStatusBar  ||
          st->AppliedRandomTime != st->ShowRandomTime ||
          st->AppliedBreathing  != st->ShowBreathing)
      {
        ShutdownApp(&app);
        InitializeApp(&app);

        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float em = ImGui::GetFontSize();       // text size: drives all sizing, scales with DPI/font

        if (st->ShowBaseWindow)
        {
          PushAppWindow<BaseWindow>(&app);
          ImGuiAppWindowBase* w = app.Windows.back();
          w->HasInitialPlacement = true;
          w->InitialSize = ImVec2(em * 16.0f, em * 8.0f);
          w->InitialPos  = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + em * 2.0f);
        }

        if (st->ShowStatusBar)
        {
          PushAppSidebar<StatusBar>(&app, vp, ImGuiDir_Down, 0.0f, ImGuiWindowFlags_AlwaysAutoResize);
        }

        // Controls are app-level: they render their own windows, no host sidebar needed.
        if (st->ShowRandomTime)
        {
          PushAppControl<RandomTimeControlDemo>(&app);
        }
        if (st->ShowBreathing)
        {
          PushAppControl<BreathingControlDemo>(&app);
        }

        st->AppliedBaseWindow = st->ShowBaseWindow;
        st->AppliedStatusBar  = st->ShowStatusBar;
        st->AppliedRandomTime = st->ShowRandomTime;
        st->AppliedBreathing  = st->ShowBreathing;
      }

      UpdateApp(&app);
      RenderApp(&app);

      if (app.ShutdownPending)
      {
        ShutdownApp(&app);
      }
  }
}
