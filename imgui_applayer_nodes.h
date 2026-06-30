#pragma once

/*

Index of this file:
// [SECTION] Header mess
// [SECTION] Reflection field helpers (VisitAppFields, DrawAppField, EditAppField)
// [SECTION] Node rendering (BeginAppNode, EndAppNode, DrawAppNodeFields, EditAppNodeFields)
// [SECTION] Design-phase node drafts (ImGuiAppFieldType, ImGuiAppFieldDesc, ImGuiAppNodeDraft)
// [SECTION] Graph topology and persistence (ImGuiAppNodeLink, link capture, save/load)
// [SECTION] Code generation (GenerateAppControlCode)

This header isolates the reflection-driven, data-driven node tooling for ImGuiAppLayer.
Like ImStructTable, it pulls in the C++20 reflection library and a little STL; that
dependency is deliberately kept out of the lean core (imgui_applayer.h). Public entry
points stay imgui-shaped: pointer parameters, char[] buffers, ImGuiID, ImVector.

Reflection-capable subset: the C++20 reflection library only reflects aggregates, and it
miscounts (explodes) on raw array members such as char Label[128] -- the same limitation
ImStructTable documents (wrap arrays in a struct). So data driven through these helpers
should be a plain-scalar aggregate. Codegen (later steps) emits exactly such aggregates.

*/

//-----------------------------------------------------------------------------
// [SECTION] Header mess
//-----------------------------------------------------------------------------

#include "imgui.h"
#include "imgui_internal.h"               // ImFormatString
#include "imgui_applayer.h"               // ImGuiApp, IM_LABEL_SIZE, ImGuiType<>
#include "reflect/reflect"                // reflect::for_each, member_name, get, type_name

#include <format>                         // std::format (field stringify, confined to this header)
#include <string>                         // std::string (transient, copied into char[] at the boundary)
#include <string_view>                    // reflect member/type names
#include <type_traits>                    // dispatch traits

//-----------------------------------------------------------------------------
// [SECTION] Reflection field helpers (VisitAppFields, DrawAppField, EditAppField)
//-----------------------------------------------------------------------------

namespace ImGui
{
  // True when std::format can stringify U with "{}". The disabled primary std::formatter
  // is not default-constructible; enabled specializations are. Standard detection idiom.
  template <typename U>
  inline constexpr bool ImIsFormattable = std::is_default_constructible_v<std::formatter<std::remove_cvref_t<U>, char>>;

  // True for a fixed-size char buffer member (e.g. char Label[128]) -> edited as text.
  template <typename U>
  inline constexpr bool ImIsCharArray = std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<U>>, char>;

  // Read-only render of a single reflected field value. Never mutates.
  template <typename T>
  inline void DrawAppField(const char* label, const T* value)
  {
    IM_ASSERT(value != nullptr);

    if constexpr (ImIsCharArray<T>)
    {
      ImGui::Text("%s: %s", label, *value);
    }
    else if constexpr (ImIsFormattable<T>)
    {
      char buf[IM_LABEL_SIZE];
      std::string s = std::format("{}", *value);
      ImFormatString(buf, IM_ARRAYSIZE(buf), "%s", s.c_str());
      ImGui::Text("%s: %s", label, buf);
    }
    else
    {
      std::string_view tn = reflect::type_name(*value);
      ImGui::Text("%s: <%.*s>", label, (int)tn.size(), tn.data());
    }
  }

  // Editable widget for a single reflected field value. Returns true if the value changed.
  // Falls back to a read-only render for types with no obvious editor.
  template <typename T>
  inline bool EditAppField(const char* label, T* value)
  {
    IM_ASSERT(value != nullptr);

    if constexpr (ImIsCharArray<T>)
      return ImGui::InputText(label, *value, std::extent_v<T>);
    else if constexpr (std::is_same_v<T, bool>)
      return ImGui::Checkbox(label, value);
    else if constexpr (std::is_same_v<T, float>)
      return ImGui::DragFloat(label, value);
    else if constexpr (std::is_same_v<T, double>)
      return ImGui::InputDouble(label, value);
    else if constexpr (std::is_same_v<T, ImVec2>)
      return ImGui::DragFloat2(label, &value->x);
    else if constexpr (std::is_same_v<T, ImVec4>)
      return ImGui::ColorEdit4(label, &value->x);
    else if constexpr (std::is_integral_v<T>)
    {
      int v = (int)*value;
      bool changed = ImGui::DragInt(label, &v);
      if (changed)
        *value = (T)v;
      return changed;
    }
    else
    {
      DrawAppField(label, value);
      return false;
    }
  }

  // Visit each reflected field of an aggregate: visitor(int index, std::string_view name, auto& value).
  // The value is passed by reference so visitors may read or mutate it in place. Pass a const T*
  // to visit fields read-only (value deduces const).
  template <typename T, typename Visitor>
  inline void VisitAppFields(T* obj, Visitor visitor)
  {
    IM_ASSERT(obj != nullptr);

    reflect::for_each([&](auto I)
    {
      visitor((int)I, reflect::member_name<I>(*obj), reflect::get<I>(*obj));
    }, *obj);
  }
}

//-----------------------------------------------------------------------------
// [SECTION] Node rendering (BeginAppNode, EndAppNode, DrawAppNodeFields, EditAppNodeFields)
//-----------------------------------------------------------------------------

namespace ImGui
{
  // imnodes node scaffold. imnodes itself stays confined to imgui_applayer_nodes.cpp.
  IMGUI_API void BeginAppNode(int id, const char* title);
  IMGUI_API void EndAppNode();

  // Renamable node scaffold: the title bar shows *name and turns into an inline text box on
  // double-click (list-view rename). Commits on Enter or focus loss, cancels on Escape. *editing_node_id
  // is caller-owned single-slot state holding the id of the node being renamed (or -1 for none); the
  // helper sets it on double-click and clears it when the edit ends. Returns true if *name changed this
  // frame. Pair with EndAppNode(). imnodes suppresses node-drag while the box is active, so typing and
  // text-selection drags do not move the node.
  IMGUI_API bool BeginAppNodeRenamable(int id, char* name, int name_size, int* editing_node_id);

  // Render every reflected field of an aggregate as read-only labelled rows in the current node.
  template <typename T>
  inline void DrawAppNodeFields(const T* data)
  {
    IM_ASSERT(data != nullptr);

    VisitAppFields(data, [](int idx, std::string_view name, const auto& value)
    {
      IM_UNUSED(idx);
      char label[IM_LABEL_SIZE];
      ImFormatString(label, IM_ARRAYSIZE(label), "%.*s", (int)name.size(), name.data());
      DrawAppField(label, &value);
    });
  }

  // Editable reflected field rows inside the current node. Returns true if any value changed.
  // Item width is clamped so nodes stay compact (imnodes nodes auto-size to their content).
  template <typename T>
  inline bool EditAppNodeFields(T* data)
  {
    IM_ASSERT(data != nullptr);

    bool changed = false;
    VisitAppFields(data, [&changed](int idx, std::string_view name, auto& value)
    {
      IM_UNUSED(idx);
      char label[IM_LABEL_SIZE];
      ImFormatString(label, IM_ARRAYSIZE(label), "%.*s", (int)name.size(), name.data());
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
      changed |= ImGui::EditAppField(label, &value);
    });
    return changed;
  }
}

//-----------------------------------------------------------------------------
// [SECTION] Design-phase node drafts (ImGuiAppFieldType, ImGuiAppFieldDesc, ImGuiAppNodeDraft)
//-----------------------------------------------------------------------------

// A draft describes a node whose backing C++ type does not exist yet: the user designs its
// fields here, then codegen (later steps) emits a reflection-capable aggregate from it. Once
// emitted and compiled, that aggregate is reflected by the helpers above. Drafts therefore use
// the same plain-scalar field vocabulary the codegen can emit.
typedef int ImGuiAppFieldType;
enum ImGuiAppFieldType_
{
  ImGuiAppFieldType_Float = 0,
  ImGuiAppFieldType_Int,
  ImGuiAppFieldType_Bool,
  ImGuiAppFieldType_Double,
  ImGuiAppFieldType_Vec2,    // ImVec2
  ImGuiAppFieldType_Vec4,    // ImVec4
  ImGuiAppFieldType_String,  // char[ArraySize]
  ImGuiAppFieldType_COUNT,
};

struct ImGuiAppFieldDesc
{
  char              Name[IM_LABEL_SIZE];
  ImGuiAppFieldType Type;
  int               ArraySize;   // buffer length for ImGuiAppFieldType_String; ignored otherwise

  ImGuiAppFieldDesc() { Name[0] = 0; Type = ImGuiAppFieldType_Float; ArraySize = 128; }
};

// One drafted control: a name plus its persisted and per-frame field sets. Ports/links are
// derived from graph edges in a later step, so they are not stored here yet.
struct ImGuiAppNodeDraft
{
  char                        Name[IM_LABEL_SIZE];
  ImVector<ImGuiAppFieldDesc> PersistFields;
  ImVector<ImGuiAppFieldDesc> TempFields;

  ImGuiAppNodeDraft() { ImStrncpy(Name, "NewControl", IM_ARRAYSIZE(Name)); }
};

namespace ImGui
{
  // C++ base type spelling for a field type. Shared by the draft editor and codegen.
  IMGUI_API const char* AppFieldTypeName(ImGuiAppFieldType type);

  // Draft field-list mutators (pointer-in, imgui-style).
  IMGUI_API void AppNodeDraftAddField(ImVector<ImGuiAppFieldDesc>* fields, const char* name, ImGuiAppFieldType type);
  IMGUI_API void AppNodeDraftRemoveField(ImVector<ImGuiAppFieldDesc>* fields, int index);

  // Inspector UI: rename the draft and add/remove/edit its persist and temp fields.
  IMGUI_API void EditAppNodeDraft(ImGuiAppNodeDraft* draft);

  // Just the persist/temp field editors (no Name input). Used where the name is edited elsewhere --
  // e.g. inside a renamable node title -- so the body would otherwise show a duplicate Name field.
  IMGUI_API void EditAppNodeDraftFields(ImGuiAppNodeDraft* draft);

  // Render a draft's fields as read-only node rows (no reflection: the type does not exist yet).
  IMGUI_API void DrawAppNodeDraft(const ImGuiAppNodeDraft* draft);
}

//-----------------------------------------------------------------------------
// [SECTION] Typed graph kinds (node/layer/port/edge discriminators)
//-----------------------------------------------------------------------------

// A node mirrors one slot of the live ImGuiApp object model. App is the singleton root that owns the
// Layers/Windows/Sidebars/Controls vectors (imgui_applayer.h: ImGuiApp). Layers are fixed C++ types
// (Task/Command/Status/Window) pushed via PushAppLayer<T>; Windows/Sidebars via PushAppWindow/Sidebar<T>;
// Controls (the only draftable kind) via PushAppControl<T>.
typedef int ImGuiAppNodeKind;
enum ImGuiAppNodeKind_
{
  ImGuiAppNodeKind_App = 0,
  ImGuiAppNodeKind_Layer,
  ImGuiAppNodeKind_Window,
  ImGuiAppNodeKind_Sidebar,
  ImGuiAppNodeKind_Control,
  ImGuiAppNodeKind_Struct,    // a standalone data struct (PersistData/TempData), wired into a control's DataIn
  ImGuiAppNodeKind_Field,     // one field of a struct, "exploded" out for per-field wiring (drives bindings)
  ImGuiAppNodeKind_COUNT,
};

// The four fixed layer classes (imgui_applayer.h: ImGuiAppTaskLayer..ImGuiAppWindowLayer). A Layer node
// selects one; codegen emits PushAppLayer<ImGuiAppXxxLayer>.
typedef int ImGuiAppLayerType;
enum ImGuiAppLayerType_
{
  ImGuiAppLayerType_Task = 0,
  ImGuiAppLayerType_Command,
  ImGuiAppLayerType_Status,
  ImGuiAppLayerType_Window,
  ImGuiAppLayerType_COUNT,
};

// A port is an imnodes attribute with a role. DataOut/DataIn carry the runtime data flow; ChildOut/ChildIn
// carry containment (which window/sidebar/app owns a node). Layers are root composition slots and have no
// containment sockets. A Control has one DataOut (its PersistData), one multi-link DataIn (all its dependencies
// -- the runtime keys app->Data by PersistData TYPE, so one type-keyed intake is faithful), and one ChildOut.
typedef int ImGuiAppPortKind;
enum ImGuiAppPortKind_
{
  ImGuiAppPortKind_DataOut = 0,
  ImGuiAppPortKind_DataIn,
  ImGuiAppPortKind_ChildOut,
  ImGuiAppPortKind_ChildIn,
  ImGuiAppPortKind_COUNT,
};

// An edge is either a data dependency (producer PersistData -> consumer dependency) or a containment edge
// (child -> parent). The kind is derived from the linked ports' kinds at capture time.
typedef int ImGuiAppEdgeKind;
enum ImGuiAppEdgeKind_
{
  ImGuiAppEdgeKind_Data = 0,
  ImGuiAppEdgeKind_Containment,
  ImGuiAppEdgeKind_COUNT,
};

//-----------------------------------------------------------------------------
// [SECTION] Graph topology and persistence (ImGuiAppNodeLink, link capture, save/load)
//-----------------------------------------------------------------------------

// A user-created edge between two node ports (source -> target). StartAttr/EndAttr are STABLE port ids
// (ImGuiAppNodePort::Id), not array-derived, so they survive node reorder/delete. Kind is a trailing field
// with a default member initializer: brace-init like { id, start, end } still compiles (value-inits Kind to
// _Data) so the legacy 3-int aggregate usage and save/load format keep working.
struct ImGuiAppNodeLink
{
  int Id;
  int StartAttr;
  int EndAttr;
  ImGuiAppEdgeKind Kind = ImGuiAppEdgeKind_Data;
};

namespace ImGui
{
  // Draw the model's links inside the current node editor. Wraps imnodes (confined to the .cpp).
  IMGUI_API void DrawAppNodeLinks(const ImVector<ImGuiAppNodeLink>* links);

  // After EndNodeEditor: fold imnodes create/destroy events into the link model. New links take
  // ids from *next_link_id (incremented). Returns true if the model changed.
  IMGUI_API bool CaptureAppNodeLinks(ImVector<ImGuiAppNodeLink>* links, int* next_link_id);

  // Persist / restore a draft and its links as imgui-style text. Return false on file error.
  IMGUI_API bool SaveAppNodeGraph(const char* path, const ImGuiAppNodeDraft* draft, const ImVector<ImGuiAppNodeLink>* links);
  IMGUI_API bool LoadAppNodeGraph(const char* path, ImGuiAppNodeDraft* draft, ImVector<ImGuiAppNodeLink>* links);

  // Multi-draft variants: persist / restore a whole graph of drafts plus its links. Same text
  // format as the single-draft pair -- each draft is one "[Draft]" section -- so a single-draft
  // file loads as a one-element vector and round-trips. *drafts is cleared before loading.
  IMGUI_API bool SaveAppNodeGraphMulti(const char* path, const ImVector<ImGuiAppNodeDraft>* drafts, const ImVector<ImGuiAppNodeLink>* links);
  IMGUI_API bool LoadAppNodeGraphMulti(const char* path, ImVector<ImGuiAppNodeDraft>* drafts, ImVector<ImGuiAppNodeLink>* links);
}

//-----------------------------------------------------------------------------
// [SECTION] Code generation (GenerateAppControlCode)
//-----------------------------------------------------------------------------

namespace ImGui
{
  // Emit a hand-written-looking ImGuiAppControl from a draft: the persist aggregate (<Name>Data),
  // the per-frame aggregate (<Name>TempData) and the control skeleton with stubbed overrides.
  // Scalar-only emissions are reflection-capable and round-trip back into the helpers above; a
  // String field emits char[N], which (like any raw array) falls outside the reflection subset.
  // Output is appended to *out.
  IMGUI_API void GenerateAppControlCode(const ImGuiAppNodeDraft* draft, ImGuiTextBuffer* out);
}

//-----------------------------------------------------------------------------
// [SECTION] Typed node graph (ports, nodes, graph model, factory, codegen, persistence)
//-----------------------------------------------------------------------------

// One imnodes attribute on a node, stored (never index-derived) so its id is stable across reorder/delete.
struct ImGuiAppNodePort
{
  int              Id;          // from ImGuiAppGraph::NextId; == imnodes attribute id
  ImGuiAppPortKind Kind;
  char             Name[IM_LABEL_SIZE];
  ImGuiID          DataTypeId;  // ImGuiType<PersistData>::ID for DataOut/DataIn (the data-flow key); 0 otherwise

  ImGuiAppNodePort() { Id = 0; Kind = ImGuiAppPortKind_DataOut; Name[0] = 0; DataTypeId = 0; }
};

struct ImGuiAppCommandDesc
{
  char Name[IM_LABEL_SIZE];

  ImGuiAppCommandDesc() { ImStrncpy(Name, "NewCommand", IM_ARRAYSIZE(Name)); }
};

// One node in the authored graph. Embeds ImGuiAppNodeDraft so the existing rename/field-edit/codegen
// helpers apply verbatim and a legacy "[Draft]" maps 1:1 to a Control node. Most fields are kind-specific.
struct ImGuiAppNode
{
  int               Id;            // from NextId; == imnodes node id
  ImGuiAppNodeKind  Kind;
  ImGuiAppNodeDraft Draft;         // Draft.Name is the node label; PersistFields/TempFields used by Control
  bool              IsBuiltin;     // true: backed by a compiled C++ type (palette), not drafted
  char              TypeName[IM_LABEL_SIZE];      // C++ type to Push<> (builtin window/sidebar/layer/control)
  char              DataTypeName[IM_LABEL_SIZE];  // builtin control PersistData type name; empty => "<Name>Data"
  ImGuiAppLayerType LayerType;     // Layer nodes only
  bool              HasInitialPlacement;          // Window/Sidebar first-use placement
  ImVec2            InitialPos;
  ImVec2            InitialSize;
  ImGuiDir          DockDir;       // Sidebar dock direction
  float             DockSize;      // Sidebar size
  ImGuiWindowFlags  Flags;         // Window/Sidebar flags
  ImVec2            GridPos;        // persisted canvas position
  bool              HasGridPos;
  bool              _NeedsPlace;    // apply GridPos to imnodes before the next BeginNode
  int               BodyAttrId;     // dedicated non-port static-attribute id for the node body
  bool              IsLive;         // mirrored from a running app object (read-only)
  bool              IsPromoted;     // design control whose emitted data type matches a live node (transient)
  ImGuiID           LiveKey;        // stable upsert key for a live node (so its position survives re-mirroring)
  ImVector<ImGuiAppCommandDesc> Commands; // CommandLayer: definitions. Control: selected commands emitted by OnGetCommand.
  ImVector<ImGuiAppNodePort> Ports;
  int               FieldList;       // Field node: which list it belongs to on its owner (0 = Persist, 1 = Temp)
  int               PersistStructId; // Control: Struct node its PersistData was exploded into (-1 = inline)
  int               TempStructId;    // Control: Struct node its TempData was exploded into (-1 = inline)

  ImGuiAppNode()
  {
    Id = 0; Kind = ImGuiAppNodeKind_Control; IsBuiltin = false; TypeName[0] = 0; DataTypeName[0] = 0;
    LayerType = ImGuiAppLayerType_Task; HasInitialPlacement = false; InitialPos = ImVec2(0.0f, 0.0f);
    InitialSize = ImVec2(0.0f, 0.0f); DockDir = ImGuiDir_Down; DockSize = 0.0f; Flags = ImGuiWindowFlags_None;
    GridPos = ImVec2(0.0f, 0.0f); HasGridPos = false; _NeedsPlace = false; BodyAttrId = 0;
    IsLive = false; IsPromoted = false; LiveKey = 0; FieldList = 0; PersistStructId = -1; TempStructId = -1;
  }
};

// Optional per-data-edge field assignment: emits one "data->Dst = dep->Src;" line in OnUpdate. Kept off the
// link (an ImVector member would make ImGuiAppNodeLink a non-aggregate and break brace-init); keyed by LinkId.
struct ImGuiAppFieldBinding
{
  int  LinkId;
  char DstField[IM_LABEL_SIZE];
  char SrcField[IM_LABEL_SIZE];

  ImGuiAppFieldBinding() { LinkId = 0; DstField[0] = 0; SrcField[0] = 0; }
};

// The whole authored graph: nodes, typed links, field bindings, and one monotonic id allocator shared by
// every node/port/body-attr/link so ids are globally unique and never reused.
struct ImGuiAppGraph
{
  ImVector<ImGuiAppNode>         Nodes;
  ImVector<ImGuiAppNodeLink>     Links;
  ImVector<ImGuiAppFieldBinding> Bindings;
  int NextId;
  int EditingNodeId;             // node whose title is being renamed inline, or -1
  char LastLinkErr[IM_LABEL_SIZE];  // last refused-link reason; transient UI state, NOT in Save/Load
  int  LastLinkErrSeq;              // bumped on every rejection -> demo edge-triggers the fade

  ImGuiAppGraph() { NextId = 1; EditingNodeId = -1; LastLinkErr[0] = 0; LastLinkErrSeq = 0; }
};

namespace ImGui
{
  // Allocation / factory. AddNode/AddBuiltin push an empty node then stamp the kind's mandatory ports and a
  // body-attr id, returning a pointer valid until the next node is added (Nodes may reallocate). Find* resolve
  // by search, never by index. RemoveNode also sweeps incident links and orphaned bindings.
  IMGUI_API int                 AppGraphAllocId(ImGuiAppGraph* g);
  IMGUI_API ImGuiAppNode*       AppGraphAddNode(ImGuiAppGraph* g, ImGuiAppNodeKind kind, const char* name);
  IMGUI_API ImGuiAppNode*       AppGraphAddBuiltin(ImGuiAppGraph* g, ImGuiAppNodeKind kind, const char* type_name, const char* data_type_name);
  IMGUI_API void                AppGraphRemoveNode(ImGuiAppGraph* g, int node_id);
  IMGUI_API ImGuiAppNode*       AppGraphFindNode(ImGuiAppGraph* g, int node_id);
  IMGUI_API ImGuiAppNodePort*   AppGraphFindPort(ImGuiAppGraph* g, int port_id, ImGuiAppNode** out_owner);
  IMGUI_API bool                AppGraphHasLayerType(const ImGuiAppGraph* g, ImGuiAppLayerType type);
  IMGUI_API void                AppNodeAddCommand(ImGuiAppNode* n, const char* name);
  IMGUI_API void                AppNodeRemoveCommand(ImGuiAppNode* n, int index);

  // The runtime data-flow key for a node named <node_name>: ConstantHash of the sanitized "<Name>Data" the
  // codegen emits -- equals ImGuiType<<Name>Data>::ID, so a design DataOut port shares the live storage key.
  IMGUI_API ImGuiID             AppNodeStructTypeId(const char* node_name);

  // Stable fold of the codegen-DETERMINING authored (!IsLive) graph state: changes iff the emitted C++ would
  // change, so a panel can show fresh|STALE. Excludes positions/ids/live-mirror churn. char[] hashed as
  // NUL-terminated string (ctors zero only byte 0, so ImHashData over the fixed buffer would be unstable).
  IMGUI_API ImGuiID             AppGraphSignature(const ImGuiAppGraph* g);

  // Typed links. CanLink validates an attempted edge (kind pairing, no self/dup, no duplicate dep type, no
  // cycle) and writes a reason to err on rejection. CaptureAppGraphLinks folds imnodes create/destroy events,
  // refusing illegal creations; returns true if the model changed.
  IMGUI_API bool                AppGraphCanLink(ImGuiAppGraph* g, int start_port, int end_port, char* err, int err_size);
  IMGUI_API bool                CaptureAppGraphLinks(ImGuiAppGraph* g, char* err, int err_size);

  // Per-edge field-binding editor (call inside an attribute). Lists/edits the bindings for one data link.
  IMGUI_API void                EditAppDataEdgeBindings(ImGuiAppGraph* g, int link_id);

  // Render the whole typed graph inside the current window (wraps BeginNodeEditor..EndNodeEditor). app may be
  // null (design-only); when non-null, builtin control bodies can reflect live data. *selected_node_id is the
  // window-level selection (caller-owned, -1 = none): the editor reconciles it both ways (canvas<->tree) and
  // clears dangling ids. show_live hides (never deletes) read-only live-mirror nodes when false.
  IMGUI_API void                ShowAppGraphEditor(ImGuiApp* app, ImGuiAppGraph* g, int* selected_node_id, bool show_live);

  // Roomy inspector for one node's authored data (name, Persist/Temp fields, window/sidebar props). Live nodes
  // are read-only. node_id < 0 -> a "select a node" hint. Edits mutate the graph in place.
  IMGUI_API void                EditAppNodeInspector(ImGuiAppGraph* g, int node_id);

  // Origin breadcrumb for a selected node: "sel: MainWindow > Mixer [design]" / "[live]" / "[promoted]" /
  // "sel: -" when id < 0 or unknown. char[] out, no references; encapsulates the containment-parent walk.
  IMGUI_API void                AppGraphSelectionBreadcrumb(const ImGuiAppGraph* g, int node_id, char* buf, int buf_size);

  // Mirror the running app's controls into *g WITHOUT reflection: remove all prior live nodes, then add one
  // read-only live Control node per pushed control (keyed by GetControlDataID) plus the data edges between
  // them, and flag design control nodes whose emitted data type matches a live node. Design (non-live) nodes
  // are untouched. Safe to call every frame.
  IMGUI_API void                BuildAppLiveGraph(const ImGuiApp* app, ImGuiAppGraph* g);

  // Scene-hierarchy / outliner panel (call inside your own child/window): a tree of the running app's
  // composition -- Layers, Windows, Sidebars, Controls -- plus the graph's authored nodes. Clicking a graph
  // row selects that node in the editor. *selected_node_id is caller-owned selection state (-1 = none).
  IMGUI_API void                ShowAppGraphTree(const ImGuiApp* app, ImGuiAppGraph* g, int* selected_node_id);

  // Topologically order the Control nodes by data dependency (producers before consumers). Returns false and
  // writes err on a cycle. out_control_ids receives node ids in push order.
  IMGUI_API bool                AppGraphTopoOrder(const ImGuiAppGraph* g, ImVector<int>* out_control_ids, char* err, int err_size);

  // Whole-graph codegen: data structs + controls with derived DataDependencies (topo order) + one bring-up
  // function pushing layers, then windows/sidebars, then controls. Appends to *out.
  IMGUI_API void                GenerateAppGraphCode(const ImGuiAppGraph* g, ImGuiTextBuffer* out);

  // Per-node codegen for the inspector: emits only the code a single selected node produces -- a Control's
  // struct(s) with derived deps, the CommandLayer's ClientAppCommand enum + dispatch, a window/sidebar/layer
  // bring-up line, or (App node) the whole composition. Appends to *out.
  IMGUI_API void                GenerateAppNodeCode(const ImGuiAppGraph* g, const ImGuiAppNode* n, ImGuiTextBuffer* out);

  // Persist / restore the whole graph as imgui-style text. LoadAppGraph also ingests the legacy single/multi
  // "[Draft]" format (each becomes a Control node). The four legacy Save/Load*Graph[Multi] functions above are
  // unchanged. Return false on file error.
  IMGUI_API bool                SaveAppGraph(const char* path, const ImGuiAppGraph* g);
  IMGUI_API bool                LoadAppGraph(const char* path, ImGuiAppGraph* g);
}
