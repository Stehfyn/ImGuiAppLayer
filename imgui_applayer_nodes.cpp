// ImGuiAppLayer data-driven node tooling: imnodes scaffolding. Keeping imnodes confined to
// this translation unit lets imgui_applayer_nodes.h stay free of the imnodes dependency, so
// callers reflect over their data without pulling in the node-editor backend at the header level.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer_nodes.h"
#include "imnodes.h"

#include <stdio.h>                         // sscanf (graph text parse)
#include <stdlib.h>                        // atoi (graph text parse)
#include <string.h>                        // strncmp (graph text parse)

namespace ImGui
{
  void BeginAppNode(int id, const char* title)
  {
    IM_ASSERT(title != nullptr);

    ImNodes::BeginNode(id);
    ImNodes::BeginNodeTitleBar();
    ImGui::TextUnformatted(title);
    ImNodes::EndNodeTitleBar();
  }

  void EndAppNode()
  {
    ImNodes::EndNode();
  }

  // Title-bar content for a renamable node: static label that becomes a focused text box while this
  // node is the one being renamed. Lives inside BeginNodeTitleBar/EndNodeTitleBar.
  static bool AppNodeTitleField(int id, char* name, int name_size, int* editing_node_id)
  {
    bool changed = false;

    if (*editing_node_id == id)
    {
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);

      // Grab keyboard focus (and select-all) only on the first edit frame. Re-grabbing every frame
      // would trap focus and defeat click-away-to-commit. One node renames at a time, so a single
      // shared latch is enough.
      static int focused_id = -1;
      if (focused_id != id)
      {
        ImGui::SetKeyboardFocusHere();
        focused_id = id;
      }

      ImGui::PushID(id);
      changed = ImGui::InputText("##rename", name, (size_t)name_size,
          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
      if (ImGui::IsItemDeactivated())   // Enter, Escape, or click-away ends the rename
      {
        *editing_node_id = -1;
        focused_id = -1;
      }
      ImGui::PopID();
    }
    else
    {
      const char* label = (name && name[0]) ? name : "(unnamed)";

      // Convey editability by color: the title is plain text at rest and only grows a text-field
      // frame on hover, so hovering reveals it is an input. Clicking swaps in the real InputText.
      // The resting title reserves the SAME footprint as that InputText -- width em*10, height
      // GetFrameHeight() -- so the node does not resize when editing starts. The label is drawn by
      // hand into that rect (clipped like an input) and a Dummy reserves the area; a Dummy is not an
      // active widget, so dragging the title still moves the node. Hover is sampled from the previous
      // frame so the frame paints *behind* the text; the one-frame latency is imperceptible.
      static int hovered_title_id = -1;

      const ImGuiStyle& style = ImGui::GetStyle();
      const float w = ImGui::GetFontSize() * 10.0f;
      const float h = ImGui::GetFrameHeight();
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      const ImVec2 mn = pos;
      const ImVec2 mx = ImVec2(pos.x + w, pos.y + h);
      ImDrawList* dl = ImGui::GetWindowDrawList();

      if (hovered_title_id == id)
        dl->AddRectFilled(mn, mx, ImGui::GetColorU32(ImGuiCol_FrameBgHovered), style.FrameRounding);

      dl->PushClipRect(mn, mx, true);
      dl->AddText(ImVec2(pos.x + style.FramePadding.x, pos.y + style.FramePadding.y),
                  ImGui::GetColorU32(ImGuiCol_Text), label);
      dl->PopClipRect();

      ImGui::Dummy(ImVec2(w, h));   // reserve the InputText footprint so the node size stays constant

      const bool hovered = ImGui::IsItemHovered();
      if (hovered)
      {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
          *editing_node_id = id;
      }
      ImGui::SetItemTooltip("Click to rename");

      hovered_title_id = hovered ? id : (hovered_title_id == id ? -1 : hovered_title_id);
    }

    return changed;
  }

  bool BeginAppNodeRenamable(int id, char* name, int name_size, int* editing_node_id)
  {
    IM_ASSERT(name != nullptr && editing_node_id != nullptr);

    ImNodes::BeginNode(id);
    ImNodes::BeginNodeTitleBar();
    const bool changed = AppNodeTitleField(id, name, name_size, editing_node_id);
    ImNodes::EndNodeTitleBar();
    return changed;
  }

  const char* AppFieldTypeName(ImGuiAppFieldType type)
  {
    switch (type)
    {
    case ImGuiAppFieldType_Float:  return "float";
    case ImGuiAppFieldType_Int:    return "int";
    case ImGuiAppFieldType_Bool:   return "bool";
    case ImGuiAppFieldType_Double: return "double";
    case ImGuiAppFieldType_Vec2:   return "ImVec2";
    case ImGuiAppFieldType_Vec4:   return "ImVec4";
    case ImGuiAppFieldType_String: return "char";
    default:                       return "float";
    }
  }

  void AppNodeDraftAddField(ImVector<ImGuiAppFieldDesc>* fields, const char* name, ImGuiAppFieldType type)
  {
    IM_ASSERT(fields != nullptr);

    ImGuiAppFieldDesc desc;
    ImStrncpy(desc.Name, (name && name[0]) ? name : "field", IM_ARRAYSIZE(desc.Name));
    desc.Type = type;
    fields->push_back(desc);
  }

  void AppNodeDraftRemoveField(ImVector<ImGuiAppFieldDesc>* fields, int index)
  {
    IM_ASSERT(fields != nullptr);

    if (index >= 0 && index < fields->Size)
      fields->erase(fields->Data + index);
  }

  // Shared field-list editor used for both the persist and temp sets.
  static void EditAppFieldList(const char* list_label, ImVector<ImGuiAppFieldDesc>* fields)
  {
    // TextDisabled (not SeparatorText) as the section label: a separator fills the content-region
    // width, which would blow up the node when this editor is hosted inside an imnodes node.
    ImGui::PushID(list_label);
    ImGui::TextDisabled("%s", list_label);

    for (int i = 0; i < fields->Size; i++)
    {
      ImGuiAppFieldDesc* f = &fields->Data[i];
      ImGui::PushID(i);

      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
      ImGui::InputText("##name", f->Name, IM_ARRAYSIZE(f->Name));

      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
      if (ImGui::BeginCombo("##type", AppFieldTypeName(f->Type)))
      {
        for (int t = 0; t < ImGuiAppFieldType_COUNT; t++)
          if (ImGui::Selectable(AppFieldTypeName((ImGuiAppFieldType)t), f->Type == t))
            f->Type = (ImGuiAppFieldType)t;
        ImGui::EndCombo();
      }

      ImGui::SameLine();
      if (ImGui::SmallButton("X"))
      {
        AppNodeDraftRemoveField(fields, i);
        ImGui::PopID();
        i--;
        continue;
      }

      ImGui::PopID();
    }

    if (ImGui::SmallButton("Add field"))
      AppNodeDraftAddField(fields, "field", ImGuiAppFieldType_Float);

    ImGui::PopID();
  }

  void EditAppNodeDraftFields(ImGuiAppNodeDraft* draft)
  {
    IM_ASSERT(draft != nullptr);

    EditAppFieldList("Persist", &draft->PersistFields);
    EditAppFieldList("Temp", &draft->TempFields);
  }

  void EditAppNodeDraft(ImGuiAppNodeDraft* draft)
  {
    IM_ASSERT(draft != nullptr);

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
    ImGui::InputText("Name", draft->Name, IM_ARRAYSIZE(draft->Name));

    EditAppNodeDraftFields(draft);
  }

  void DrawAppNodeDraft(const ImGuiAppNodeDraft* draft)
  {
    IM_ASSERT(draft != nullptr);

    // imnodes positions the cursor at the node content origin in EndNodeTitleBar; the body must
    // submit at least one item or EndNode trips imgui's "SetCursorPos extends boundaries" assert.
    // A field-less draft therefore still emits a placeholder row.
    if (draft->PersistFields.Size == 0 && draft->TempFields.Size == 0)
    {
      ImGui::TextDisabled("(no fields)");
      return;
    }

    for (int i = 0; i < draft->PersistFields.Size; i++)
    {
      const ImGuiAppFieldDesc* f = &draft->PersistFields.Data[i];
      ImGui::Text("%s: %s", f->Name, AppFieldTypeName(f->Type));
    }
    for (int i = 0; i < draft->TempFields.Size; i++)
    {
      const ImGuiAppFieldDesc* f = &draft->TempFields.Data[i];
      ImGui::Text("%s: %s (temp)", f->Name, AppFieldTypeName(f->Type));
    }
  }

  void DrawAppNodeLinks(const ImVector<ImGuiAppNodeLink>* links)
  {
    IM_ASSERT(links != nullptr);

    for (int i = 0; i < links->Size; i++)
      ImNodes::Link(links->Data[i].Id, links->Data[i].StartAttr, links->Data[i].EndAttr);
  }

  bool CaptureAppNodeLinks(ImVector<ImGuiAppNodeLink>* links, int* next_link_id)
  {
    IM_ASSERT(links != nullptr && next_link_id != nullptr);

    bool changed = false;

    ImGuiAppNodeLink created;
    if (ImNodes::IsLinkCreated(&created.StartAttr, &created.EndAttr))
    {
      created.Id = (*next_link_id)++;
      links->push_back(created);
      changed = true;
    }

    int destroyed_id = 0;
    if (ImNodes::IsLinkDestroyed(&destroyed_id))
    {
      for (int i = 0; i < links->Size; i++)
        if (links->Data[i].Id == destroyed_id)
        {
          links->erase(links->Data + i);
          changed = true;
          break;
        }
    }

    return changed;
  }

  // Parse one "name,typeint,arraysize" field record into fields.
  static void AppNodeParseField(ImVector<ImGuiAppFieldDesc>* fields, const char* line)
  {
    ImGuiAppFieldDesc f;
    int type = ImGuiAppFieldType_Float;
    int array_size = f.ArraySize;
    if (sscanf(line, "%255[^,],%d,%d", f.Name, &type, &array_size) >= 1)
    {
      f.Type = (ImGuiAppFieldType)type;
      f.ArraySize = array_size;
      fields->push_back(f);
    }
  }

  bool SaveAppNodeGraph(const char* path, const ImGuiAppNodeDraft* draft, const ImVector<ImGuiAppNodeLink>* links)
  {
    IM_ASSERT(path != nullptr && draft != nullptr && links != nullptr);

    ImGuiTextBuffer buf;
    buf.appendf("[Draft]\n");
    buf.appendf("Name=%s\n", draft->Name);
    for (int i = 0; i < draft->PersistFields.Size; i++)
    {
      const ImGuiAppFieldDesc* f = &draft->PersistFields.Data[i];
      buf.appendf("Persist=%s,%d,%d\n", f->Name, (int)f->Type, f->ArraySize);
    }
    for (int i = 0; i < draft->TempFields.Size; i++)
    {
      const ImGuiAppFieldDesc* f = &draft->TempFields.Data[i];
      buf.appendf("Temp=%s,%d,%d\n", f->Name, (int)f->Type, f->ArraySize);
    }
    for (int i = 0; i < links->Size; i++)
      buf.appendf("Link=%d,%d,%d\n", links->Data[i].Id, links->Data[i].StartAttr, links->Data[i].EndAttr);

    ImFileHandle fh = ImFileOpen(path, "wt");
    if (fh == nullptr)
      return false;
    ImFileWrite(buf.c_str(), sizeof(char), (ImU64)buf.size(), fh);
    ImFileClose(fh);
    return true;
  }

  // Append one draft as a "[Draft]" section to buf. Shared by the single- and multi-draft savers.
  static void AppNodeWriteDraft(ImGuiTextBuffer* buf, const ImGuiAppNodeDraft* draft)
  {
    buf->appendf("[Draft]\n");
    buf->appendf("Name=%s\n", draft->Name);
    for (int i = 0; i < draft->PersistFields.Size; i++)
    {
      const ImGuiAppFieldDesc* f = &draft->PersistFields.Data[i];
      buf->appendf("Persist=%s,%d,%d\n", f->Name, (int)f->Type, f->ArraySize);
    }
    for (int i = 0; i < draft->TempFields.Size; i++)
    {
      const ImGuiAppFieldDesc* f = &draft->TempFields.Data[i];
      buf->appendf("Temp=%s,%d,%d\n", f->Name, (int)f->Type, f->ArraySize);
    }
  }

  bool SaveAppNodeGraphMulti(const char* path, const ImVector<ImGuiAppNodeDraft>* drafts, const ImVector<ImGuiAppNodeLink>* links)
  {
    IM_ASSERT(path != nullptr && drafts != nullptr && links != nullptr);

    ImGuiTextBuffer buf;
    for (int i = 0; i < drafts->Size; i++)
      AppNodeWriteDraft(&buf, &drafts->Data[i]);
    for (int i = 0; i < links->Size; i++)
      buf.appendf("Link=%d,%d,%d\n", links->Data[i].Id, links->Data[i].StartAttr, links->Data[i].EndAttr);

    ImFileHandle fh = ImFileOpen(path, "wt");
    if (fh == nullptr)
      return false;
    ImFileWrite(buf.c_str(), sizeof(char), (ImU64)buf.size(), fh);
    ImFileClose(fh);
    return true;
  }

  bool LoadAppNodeGraphMulti(const char* path, ImVector<ImGuiAppNodeDraft>* drafts, ImVector<ImGuiAppNodeLink>* links)
  {
    IM_ASSERT(path != nullptr && drafts != nullptr && links != nullptr);

    size_t data_size = 0;
    char* data = (char*)ImFileLoadToMemory(path, "rb", &data_size, 1); // +1 zero terminator
    if (data == nullptr)
      return false;

    drafts->clear();
    links->clear();

    // "[Draft]" opens a new draft; the Name/Persist/Temp lines that follow apply to it. A file with
    // no "[Draft]" header but Name/Persist lines (older single-draft format) still works: the first
    // such line lazily opens draft 0.
    ImGuiAppNodeDraft* cur = nullptr;
    char* p = data;
    while (*p)
    {
      char* eol = p;
      while (*eol != 0 && *eol != '\n')
        eol++;
      const char saved = *eol;
      *eol = 0;
      if (eol > p && eol[-1] == '\r')   // text-mode writes \r\n; binary read keeps the \r -> trim it
        eol[-1] = 0;

      if (strncmp(p, "[Draft]", 7) == 0)
      {
        drafts->push_back(ImGuiAppNodeDraft());
        cur = &drafts->Data[drafts->Size - 1];
        cur->PersistFields.clear();
        cur->TempFields.clear();
      }
      else if (strncmp(p, "Name=", 5) == 0)
      {
        if (cur == nullptr) { drafts->push_back(ImGuiAppNodeDraft()); cur = &drafts->Data[drafts->Size - 1]; cur->PersistFields.clear(); cur->TempFields.clear(); }
        ImStrncpy(cur->Name, p + 5, IM_ARRAYSIZE(cur->Name));
      }
      else if (cur != nullptr && strncmp(p, "Persist=", 8) == 0)
        AppNodeParseField(&cur->PersistFields, p + 8);
      else if (cur != nullptr && strncmp(p, "Temp=", 5) == 0)
        AppNodeParseField(&cur->TempFields, p + 5);
      else if (strncmp(p, "Link=", 5) == 0)
      {
        ImGuiAppNodeLink l;
        if (sscanf(p + 5, "%d,%d,%d", &l.Id, &l.StartAttr, &l.EndAttr) == 3)
          links->push_back(l);
      }

      if (saved == 0)
        break;
      p = eol + 1;
    }

    IM_FREE(data);
    return true;
  }

  bool LoadAppNodeGraph(const char* path, ImGuiAppNodeDraft* draft, ImVector<ImGuiAppNodeLink>* links)
  {
    IM_ASSERT(path != nullptr && draft != nullptr && links != nullptr);

    size_t data_size = 0;
    char* data = (char*)ImFileLoadToMemory(path, "rb", &data_size, 1); // +1 zero terminator
    if (data == nullptr)
      return false;

    draft->PersistFields.clear();
    draft->TempFields.clear();
    links->clear();

    char* p = data;
    while (*p)
    {
      char* eol = p;
      while (*eol != 0 && *eol != '\n')
        eol++;
      const char saved = *eol;
      *eol = 0;

      if (strncmp(p, "Name=", 5) == 0)
        ImStrncpy(draft->Name, p + 5, IM_ARRAYSIZE(draft->Name));
      else if (strncmp(p, "Persist=", 8) == 0)
        AppNodeParseField(&draft->PersistFields, p + 8);
      else if (strncmp(p, "Temp=", 5) == 0)
        AppNodeParseField(&draft->TempFields, p + 5);
      else if (strncmp(p, "Link=", 5) == 0)
      {
        ImGuiAppNodeLink l;
        if (sscanf(p + 5, "%d,%d,%d", &l.Id, &l.StartAttr, &l.EndAttr) == 3)
          links->push_back(l);
      }

      if (saved == 0)
        break;
      p = eol + 1;
    }

    IM_FREE(data);
    return true;
  }

  // Copy src into dst as a valid C++ identifier: alnum/underscore kept, anything else -> '_',
  // a leading digit prefixed with '_'. Empty input becomes "Control".
  static void AppSanitizeIdentifier(char* dst, size_t dst_size, const char* src)
  {
    IM_ASSERT(dst != nullptr && dst_size > 0);

    size_t n = 0;
    if (src == nullptr || src[0] == 0)
    {
      ImStrncpy(dst, "Control", dst_size);
      return;
    }

    if (src[0] >= '0' && src[0] <= '9' && n + 1 < dst_size)
      dst[n++] = '_';

    for (const char* s = src; *s != 0 && n + 1 < dst_size; s++)
    {
      const char c = *s;
      const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
      dst[n++] = keep ? c : '_';
    }
    dst[n] = 0;
  }

  // Emit a single struct member declaration for a drafted field.
  static void AppEmitFieldDecl(ImGuiTextBuffer* out, const ImGuiAppFieldDesc* f)
  {
    char name[IM_LABEL_SIZE];
    AppSanitizeIdentifier(name, IM_ARRAYSIZE(name), f->Name);

    if (f->Type == ImGuiAppFieldType_String)
      out->appendf("  char %s[%d];\n", name, f->ArraySize > 0 ? f->ArraySize : 128);
    else
      out->appendf("  %s %s;\n", AppFieldTypeName(f->Type), name);
  }

  void GenerateAppControlCode(const ImGuiAppNodeDraft* draft, ImGuiTextBuffer* out)
  {
    IM_ASSERT(draft != nullptr && out != nullptr);

    char base[IM_LABEL_SIZE];
    AppSanitizeIdentifier(base, IM_ARRAYSIZE(base), draft->Name);

    // Persisted data aggregate (<Name>Data).
    out->appendf("struct %sData\n{\n", base);
    for (int i = 0; i < draft->PersistFields.Size; i++)
      AppEmitFieldDecl(out, &draft->PersistFields.Data[i]);
    out->appendf("};\n\n");

    // Per-frame data aggregate (<Name>TempData).
    out->appendf("struct %sTempData\n{\n", base);
    for (int i = 0; i < draft->TempFields.Size; i++)
      AppEmitFieldDecl(out, &draft->TempFields.Data[i]);
    out->appendf("};\n\n");

    // Control skeleton. Dependencies (extra ImGuiAppControl<> template args) would be derived from
    // incoming graph links once nodes map to drafted types; a single draft emits none.
    out->appendf("struct %s : ImGuiAppControl<%sData, %sTempData>\n{\n", base, base, base);
    out->appendf("  virtual void OnInitialize(ImGuiApp* app, %sData* data) const override final\n", base);
    out->appendf("  {\n    IM_UNUSED(app); IM_UNUSED(data);\n    // TODO: initialize persistent data\n  }\n\n");
    out->appendf("  virtual void OnUpdate(float dt, %sData* data, const %sTempData* temp_data, const %sTempData* last_temp_data) const override final\n", base, base, base);
    out->appendf("  {\n    IM_UNUSED(dt); IM_UNUSED(data); IM_UNUSED(temp_data); IM_UNUSED(last_temp_data);\n    // TODO: update state from temp_data\n  }\n\n");
    out->appendf("  virtual void OnRender(const %sData* data, %sTempData* temp_data) const override final\n", base, base);
    out->appendf("  {\n    IM_UNUSED(data); IM_UNUSED(temp_data);\n    // TODO: render widgets, write temp_data\n  }\n};\n");
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Typed node graph: allocation, factory, lookup
  //-----------------------------------------------------------------------------

  // ConstantHash mirror of ImGuiStatic::_ConstantHash (imgui_applayer.h) so a design port can carry the
  // exact runtime data-flow key ImGuiType<PersistData>::ID. Identifiers are ASCII, so char signedness is moot.
  static ImGuiID AppConstantHash(const char* s)
  {
    return *s ? (ImGuiID)(unsigned char)*s + 33u * AppConstantHash(s + 1) : 5381u;
  }

  ImGuiID AppNodeStructTypeId(const char* node_name)
  {
    char base[IM_LABEL_SIZE];
    AppSanitizeIdentifier(base, IM_ARRAYSIZE(base), node_name);
    char data[IM_LABEL_SIZE];
    ImFormatString(data, IM_ARRAYSIZE(data), "%sData", base);
    return AppConstantHash(data);
  }

  int AppGraphAllocId(ImGuiAppGraph* g)
  {
    IM_ASSERT(g != nullptr);
    return g->NextId++;
  }

  static void AppGraphPushPort(ImGuiAppGraph* g, ImGuiAppNode* n, ImGuiAppPortKind kind, const char* name, ImGuiID data_type)
  {
    ImGuiAppNodePort p;
    p.Id = AppGraphAllocId(g);
    p.Kind = kind;
    ImStrncpy(p.Name, name, IM_ARRAYSIZE(p.Name));
    p.DataTypeId = data_type;
    n->Ports.push_back(p);
  }

  // Stamp the mandatory ports for a node kind. For a Control, DataOut/DataIn carry the node's own data type id
  // (DataIn is a wildcard multi-link intake -> 0). Containment ports wire the ownership tree.
  static void AppGraphStampPorts(ImGuiAppGraph* g, ImGuiAppNode* n)
  {
    const ImGuiID self_data = n->IsBuiltin && n->DataTypeName[0] ? AppConstantHash(n->DataTypeName)
                                                                 : AppNodeStructTypeId(n->Draft.Name);
    switch (n->Kind)
    {
    case ImGuiAppNodeKind_App:
      AppGraphPushPort(g, n, ImGuiAppPortKind_ChildIn, "children", 0);
      break;
    case ImGuiAppNodeKind_Layer:
      AppGraphPushPort(g, n, ImGuiAppPortKind_ChildOut, "parent", 0);
      break;
    case ImGuiAppNodeKind_Window:
    case ImGuiAppNodeKind_Sidebar:
      AppGraphPushPort(g, n, ImGuiAppPortKind_ChildIn, "children", 0);
      AppGraphPushPort(g, n, ImGuiAppPortKind_ChildOut, "parent", 0);
      break;
    case ImGuiAppNodeKind_Control:
    default:
      AppGraphPushPort(g, n, ImGuiAppPortKind_DataIn, "deps", 0);
      AppGraphPushPort(g, n, ImGuiAppPortKind_DataOut, "data", self_data);
      AppGraphPushPort(g, n, ImGuiAppPortKind_ChildOut, "parent", 0);
      break;
    }
  }

  static ImGuiAppNode* AppGraphInitNode(ImGuiAppGraph* g, ImGuiAppNodeKind kind, const char* name)
  {
    // Push an EMPTY node first (its ImVector<Port> is null), then populate in place: ImVector relocates by
    // memcpy on grow, which moves the inner vectors' heap pointers without double-freeing.
    g->Nodes.push_back(ImGuiAppNode());
    ImGuiAppNode* n = &g->Nodes.back();
    n->Id = AppGraphAllocId(g);
    n->Kind = kind;
    ImStrncpy(n->Draft.Name, (name && name[0]) ? name : "Node", IM_ARRAYSIZE(n->Draft.Name));
    n->BodyAttrId = AppGraphAllocId(g);
    const int idx = g->Nodes.Size - 1;
    n->GridPos = ImVec2(40.0f + (idx % 4) * 210.0f, 40.0f + (idx / 4) * 120.0f);   // grid, not a diagonal pile
    n->HasGridPos = true;
    n->_NeedsPlace = true;
    AppGraphStampPorts(g, n);
    return n;
  }

  ImGuiAppNode* AppGraphAddNode(ImGuiAppGraph* g, ImGuiAppNodeKind kind, const char* name)
  {
    IM_ASSERT(g != nullptr);
    return AppGraphInitNode(g, kind, name);
  }

  ImGuiAppNode* AppGraphAddBuiltin(ImGuiAppGraph* g, ImGuiAppNodeKind kind, const char* type_name, const char* data_type_name)
  {
    IM_ASSERT(g != nullptr && type_name != nullptr);

    // Build the node manually so IsBuiltin/DataTypeName are set BEFORE ports are stamped (so the DataOut port
    // gets the builtin's real data type id).
    g->Nodes.push_back(ImGuiAppNode());
    ImGuiAppNode* n = &g->Nodes.back();
    n->Id = AppGraphAllocId(g);
    n->Kind = kind;
    n->IsBuiltin = true;
    ImStrncpy(n->TypeName, type_name, IM_ARRAYSIZE(n->TypeName));
    ImStrncpy(n->Draft.Name, type_name, IM_ARRAYSIZE(n->Draft.Name));
    if (data_type_name && data_type_name[0])
      ImStrncpy(n->DataTypeName, data_type_name, IM_ARRAYSIZE(n->DataTypeName));
    n->BodyAttrId = AppGraphAllocId(g);
    const int idx = g->Nodes.Size - 1;
    n->GridPos = ImVec2(40.0f + (idx % 4) * 210.0f, 40.0f + (idx / 4) * 120.0f);   // grid, not a diagonal pile
    n->HasGridPos = true;
    n->_NeedsPlace = true;
    AppGraphStampPorts(g, n);
    return n;
  }

  ImGuiAppNode* AppGraphFindNode(ImGuiAppGraph* g, int node_id)
  {
    IM_ASSERT(g != nullptr);
    for (int i = 0; i < g->Nodes.Size; i++)
      if (g->Nodes.Data[i].Id == node_id)
        return &g->Nodes.Data[i];
    return nullptr;
  }

  ImGuiAppNodePort* AppGraphFindPort(ImGuiAppGraph* g, int port_id, ImGuiAppNode** out_owner)
  {
    IM_ASSERT(g != nullptr);
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      ImGuiAppNode* n = &g->Nodes.Data[i];
      for (int p = 0; p < n->Ports.Size; p++)
        if (n->Ports.Data[p].Id == port_id)
        {
          if (out_owner) *out_owner = n;
          return &n->Ports.Data[p];
        }
    }
    if (out_owner) *out_owner = nullptr;
    return nullptr;
  }

  // Owner node id for a port id (const-friendly; -1 if unknown). Used by topo/codegen which take const graphs.
  static int AppGraphPortOwnerId(const ImGuiAppGraph* g, int port_id)
  {
    for (int i = 0; i < g->Nodes.Size; i++)
      for (int p = 0; p < g->Nodes.Data[i].Ports.Size; p++)
        if (g->Nodes.Data[i].Ports.Data[p].Id == port_id)
          return g->Nodes.Data[i].Id;
    return -1;
  }

  void AppGraphRemoveNode(ImGuiAppGraph* g, int node_id)
  {
    IM_ASSERT(g != nullptr);

    ImGuiAppNode* n = AppGraphFindNode(g, node_id);
    if (n == nullptr)
      return;

    // Sweep every link incident on one of this node's ports, and any binding orphaned by those links.
    for (int li = g->Links.Size - 1; li >= 0; li--)
    {
      const int owner_a = AppGraphPortOwnerId(g, g->Links.Data[li].StartAttr);
      const int owner_b = AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr);
      if (owner_a == node_id || owner_b == node_id)
      {
        const int link_id = g->Links.Data[li].Id;
        for (int bi = g->Bindings.Size - 1; bi >= 0; bi--)
          if (g->Bindings.Data[bi].LinkId == link_id)
            g->Bindings.erase(g->Bindings.Data + bi);
        g->Links.erase(g->Links.Data + li);
      }
    }
    g->Nodes.erase(n);   // surviving nodes/ports/links keep their ids; ids are never reused
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Typed links: resolve / validate / capture
  //-----------------------------------------------------------------------------

  static bool AppPortIsOutput(ImGuiAppPortKind k) { return k == ImGuiAppPortKind_DataOut || k == ImGuiAppPortKind_ChildOut; }
  static bool AppPortIsInput(ImGuiAppPortKind k)  { return k == ImGuiAppPortKind_DataIn  || k == ImGuiAppPortKind_ChildIn; }

  // Does control 'from' reach control 'to' along existing data edges (producer -> consumer)? Used to reject a
  // new edge that would close a cycle. Bounded by Nodes.Size iterations (no recursion / visited set needed for
  // small graphs; we cap to avoid pathological loops if the model is ever inconsistent).
  static bool AppGraphDataReaches(const ImGuiAppGraph* g, int from_node, int to_node)
  {
    if (from_node == to_node)
      return true;

    ImVector<int> frontier;
    frontier.push_back(from_node);
    ImVector<int> seen;
    seen.push_back(from_node);

    for (int guard = 0; guard < g->Links.Size + 1 && frontier.Size > 0; guard++)
    {
      ImVector<int> next;
      for (int f = 0; f < frontier.Size; f++)
      {
        const int cur = frontier.Data[f];
        for (int li = 0; li < g->Links.Size; li++)
        {
          if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Data)
            continue;
          if (AppGraphPortOwnerId(g, g->Links.Data[li].StartAttr) != cur)
            continue;
          const int consumer = AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr);
          if (consumer == to_node)
            return true;
          bool known = false;
          for (int s = 0; s < seen.Size; s++) if (seen.Data[s] == consumer) { known = true; break; }
          if (!known) { seen.push_back(consumer); next.push_back(consumer); }
        }
      }
      frontier.swap(next);
    }
    return false;
  }

  static void AppSetErr(char* err, int err_size, const char* msg)
  {
    if (err && err_size > 0)
      ImStrncpy(err, msg, (size_t)err_size);
  }

  // Resolve an attempted (a -> b) imnodes link (drag order arbitrary) into a normalized source->target edge.
  // Writes the output port id to out_src, input port id to out_dst, and the derived edge kind. err on reject.
  static bool AppGraphResolveLink(ImGuiAppGraph* g, int a, int b, int* out_src, int* out_dst, ImGuiAppEdgeKind* out_kind, char* err, int err_size)
  {
    ImGuiAppNode* na = nullptr; ImGuiAppNode* nb = nullptr;
    ImGuiAppNodePort* pa = AppGraphFindPort(g, a, &na);
    ImGuiAppNodePort* pb = AppGraphFindPort(g, b, &nb);
    if (pa == nullptr || pb == nullptr) { AppSetErr(err, err_size, "unknown port"); return false; }
    if (na == nb) { AppSetErr(err, err_size, "cannot link a node to itself"); return false; }

    // Normalize so src is the output side, dst the input side.
    ImGuiAppNodePort* src = nullptr; ImGuiAppNodePort* dst = nullptr;
    ImGuiAppNode* src_owner = nullptr; ImGuiAppNode* dst_owner = nullptr;
    if (AppPortIsOutput(pa->Kind) && AppPortIsInput(pb->Kind)) { src = pa; dst = pb; src_owner = na; dst_owner = nb; }
    else if (AppPortIsOutput(pb->Kind) && AppPortIsInput(pa->Kind)) { src = pb; dst = pa; src_owner = nb; dst_owner = na; }
    else { AppSetErr(err, err_size, "must connect an output port to an input port"); return false; }

    // Reject an exact duplicate edge (same two ports).
    for (int li = 0; li < g->Links.Size; li++)
      if (g->Links.Data[li].StartAttr == src->Id && g->Links.Data[li].EndAttr == dst->Id)
      { AppSetErr(err, err_size, "duplicate link"); return false; }

    if (src->Kind == ImGuiAppPortKind_DataOut && dst->Kind == ImGuiAppPortKind_DataIn)
    {
      // Data dependency: producer (src_owner) -> consumer (dst_owner).
      // No duplicate dependency TYPE: the runtime keys app->Data by PersistData type.
      for (int li = 0; li < g->Links.Size; li++)
      {
        if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Data) continue;
        if (AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr) != dst_owner->Id) continue;
        ImGuiAppNode* existing_producer = nullptr;
        AppGraphFindPort(g, g->Links.Data[li].StartAttr, &existing_producer);
        if (existing_producer && existing_producer->Ports.Size > 0)
        {
          // compare the producers' data type ids (their DataOut port)
          ImGuiID existing_tid = 0;
          for (int pp = 0; pp < existing_producer->Ports.Size; pp++)
            if (existing_producer->Ports.Data[pp].Kind == ImGuiAppPortKind_DataOut) existing_tid = existing_producer->Ports.Data[pp].DataTypeId;
          if (existing_tid != 0 && existing_tid == src->DataTypeId)
          { AppSetErr(err, err_size, "already depends on this data type"); return false; }
        }
      }
      // Cycle guard: edge is producer->consumer. Illegal if consumer already reaches producer.
      if (AppGraphDataReaches(g, dst_owner->Id, src_owner->Id))
      { AppSetErr(err, err_size, "would create a dependency cycle"); return false; }

      *out_src = src->Id; *out_dst = dst->Id; *out_kind = ImGuiAppEdgeKind_Data;
      return true;
    }

    if (src->Kind == ImGuiAppPortKind_ChildOut && dst->Kind == ImGuiAppPortKind_ChildIn)
    {
      // Containment: a node has at most one parent.
      for (int li = 0; li < g->Links.Size; li++)
        if (g->Links.Data[li].Kind == ImGuiAppEdgeKind_Containment && g->Links.Data[li].StartAttr == src->Id)
        { AppSetErr(err, err_size, "node already has a parent"); return false; }
      *out_src = src->Id; *out_dst = dst->Id; *out_kind = ImGuiAppEdgeKind_Containment;
      return true;
    }

    AppSetErr(err, err_size, "incompatible port kinds");
    return false;
  }

  bool AppGraphCanLink(ImGuiAppGraph* g, int start_port, int end_port, char* err, int err_size)
  {
    IM_ASSERT(g != nullptr);
    int s = 0, d = 0; ImGuiAppEdgeKind k = ImGuiAppEdgeKind_Data;
    return AppGraphResolveLink(g, start_port, end_port, &s, &d, &k, err, err_size);
  }

  bool CaptureAppGraphLinks(ImGuiAppGraph* g, char* err, int err_size)
  {
    IM_ASSERT(g != nullptr);
    bool changed = false;
    if (err && err_size > 0) err[0] = 0;

    int sa = 0, ea = 0;
    if (ImNodes::IsLinkCreated(&sa, &ea))
    {
      int s = 0, d = 0; ImGuiAppEdgeKind k = ImGuiAppEdgeKind_Data;
      if (AppGraphResolveLink(g, sa, ea, &s, &d, &k, err, err_size))
      {
        ImGuiAppNodeLink link;
        link.Id = AppGraphAllocId(g);
        link.StartAttr = s;
        link.EndAttr = d;
        link.Kind = k;
        g->Links.push_back(link);
        changed = true;
      }
    }

    int destroyed = 0;
    if (ImNodes::IsLinkDestroyed(&destroyed))
    {
      for (int i = 0; i < g->Links.Size; i++)
        if (g->Links.Data[i].Id == destroyed)
        {
          for (int bi = g->Bindings.Size - 1; bi >= 0; bi--)
            if (g->Bindings.Data[bi].LinkId == destroyed)
              g->Bindings.erase(g->Bindings.Data + bi);
          g->Links.erase(g->Links.Data + i);
          changed = true;
          break;
        }
    }
    return changed;
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Per-edge field bindings editor
  //-----------------------------------------------------------------------------

  void EditAppDataEdgeBindings(ImGuiAppGraph* g, int link_id)
  {
    IM_ASSERT(g != nullptr);

    // Only data edges carry field bindings.
    bool is_data = false;
    for (int i = 0; i < g->Links.Size; i++)
      if (g->Links.Data[i].Id == link_id && g->Links.Data[i].Kind == ImGuiAppEdgeKind_Data) { is_data = true; break; }
    if (!is_data)
      return;

    ImGui::PushID(link_id);
    ImGui::TextDisabled("bindings");
    int remove = -1;
    int row = 0;
    for (int i = 0; i < g->Bindings.Size; i++)
    {
      if (g->Bindings.Data[i].LinkId != link_id)
        continue;
      ImGui::PushID(row++);
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
      ImGui::InputText("##dst", g->Bindings.Data[i].DstField, IM_ARRAYSIZE(g->Bindings.Data[i].DstField));
      ImGui::SameLine(); ImGui::TextUnformatted("=");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
      ImGui::InputText("##src", g->Bindings.Data[i].SrcField, IM_ARRAYSIZE(g->Bindings.Data[i].SrcField));
      ImGui::SameLine();
      if (ImGui::SmallButton("X"))
        remove = i;
      ImGui::PopID();
    }
    if (remove >= 0)
      g->Bindings.erase(g->Bindings.Data + remove);
    if (ImGui::SmallButton("Add binding"))
    {
      ImGuiAppFieldBinding b;
      b.LinkId = link_id;
      g->Bindings.push_back(b);
    }
    ImGui::PopID();
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Whole-graph editor render
  //-----------------------------------------------------------------------------

  static const char* AppLayerTypeName(ImGuiAppLayerType t)
  {
    switch (t)
    {
    case ImGuiAppLayerType_Task:    return "ImGuiAppTaskLayer";
    case ImGuiAppLayerType_Command: return "ImGuiAppCommandLayer";
    case ImGuiAppLayerType_Status:  return "ImGuiAppStatusLayer";
    case ImGuiAppLayerType_Window:  return "ImGuiAppWindowLayer";
    default:                        return "ImGuiAppLayer";
    }
  }

  static const char* AppNodeKindName(ImGuiAppNodeKind k)
  {
    switch (k)
    {
    case ImGuiAppNodeKind_App:     return "App";
    case ImGuiAppNodeKind_Layer:   return "Layer";
    case ImGuiAppNodeKind_Window:  return "Window";
    case ImGuiAppNodeKind_Sidebar: return "Sidebar";
    case ImGuiAppNodeKind_Control: return "Control";
    default:                       return "Node";
    }
  }

  // The data link feeding a given consumer node (its DataIn), if any -- used to host its binding editor.
  static int AppGraphIncomingDataLink(const ImGuiAppGraph* g, int consumer_node_id)
  {
    for (int li = 0; li < g->Links.Size; li++)
      if (g->Links.Data[li].Kind == ImGuiAppEdgeKind_Data && AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr) == consumer_node_id)
        return g->Links.Data[li].Id;
    return -1;
  }

  void ShowAppGraphEditor(ImGuiApp* app, ImGuiAppGraph* g)
  {
    IM_ASSERT(g != nullptr);
    IM_UNUSED(app);

    ImNodes::BeginNodeEditor();

    for (int i = 0; i < g->Nodes.Size; i++)
    {
      ImGuiAppNode* n = &g->Nodes.Data[i];

      // Position must be set BEFORE BeginNode (imnodes lays out content at the node origin).
      if (n->_NeedsPlace)
      {
        ImNodes::SetNodeGridSpacePos(n->Id, n->GridPos);
        n->_NeedsPlace = false;
      }

      // Live nodes mirror the running app and are read-only: static (non-renamable) title.
      if (n->IsLive)
        ImGui::BeginAppNode(n->Id, n->Draft.Name[0] ? n->Draft.Name : "(live)");
      else
        ImGui::BeginAppNodeRenamable(n->Id, n->Draft.Name, IM_ARRAYSIZE(n->Draft.Name), &g->EditingNodeId);

      // Input-side ports first (left), then the body, then output-side ports (right).
      for (int p = 0; p < n->Ports.Size; p++)
      {
        ImGuiAppNodePort* port = &n->Ports.Data[p];
        if (port->Kind == ImGuiAppPortKind_DataIn)  { ImNodes::BeginInputAttribute(port->Id); ImGui::TextUnformatted(port->Name); ImNodes::EndInputAttribute(); }
        else if (port->Kind == ImGuiAppPortKind_ChildIn) { ImNodes::BeginInputAttribute(port->Id); ImGui::TextUnformatted(port->Name); ImNodes::EndInputAttribute(); }
      }

      // Body (a dedicated static attribute id; always submits >=1 item so imnodes' empty-body assert can't fire).
      ImNodes::BeginStaticAttribute(n->BodyAttrId);
      ImGui::PushID(n->Id);
      if (n->Kind == ImGuiAppNodeKind_Control)
      {
        if (n->IsBuiltin)
        {
          ImGui::TextDisabled("%s", n->TypeName[0] ? n->TypeName : "(builtin)");
          if (n->DataTypeName[0]) ImGui::TextDisabled("data: %s", n->DataTypeName);
        }
        else
        {
          ImGui::EditAppNodeDraftFields(&n->Draft);
        }

        const int in_link = AppGraphIncomingDataLink(g, n->Id);
        if (in_link >= 0)
          EditAppDataEdgeBindings(g, in_link);
      }
      else if (n->Kind == ImGuiAppNodeKind_Layer)
      {
        if (n->IsLive)
        {
          // Framework-managed: show the layer type, not editable.
          ImGui::TextDisabled("%s", AppLayerTypeName(n->LayerType));
        }
        else
        {
          ImGui::SetNextItemWidth(ImGui::GetFontSize() * 11.0f);
          if (ImGui::BeginCombo("##layertype", AppLayerTypeName(n->LayerType)))
          {
            for (int t = 0; t < ImGuiAppLayerType_COUNT; t++)
              if (ImGui::Selectable(AppLayerTypeName((ImGuiAppLayerType)t), n->LayerType == t))
                n->LayerType = (ImGuiAppLayerType)t;
            ImGui::EndCombo();
          }
        }
      }
      else if (n->Kind == ImGuiAppNodeKind_Window || n->Kind == ImGuiAppNodeKind_Sidebar)
      {
        ImGui::TextDisabled("%s", AppNodeKindName(n->Kind));
        if (n->IsBuiltin && n->TypeName[0]) ImGui::TextDisabled("type: %s", n->TypeName);
      }
      else
      {
        ImGui::TextDisabled("%s", AppNodeKindName(n->Kind));
      }
      ImGui::PopID();
      ImNodes::EndStaticAttribute();

      // Output-side ports (right).
      for (int p = 0; p < n->Ports.Size; p++)
      {
        ImGuiAppNodePort* port = &n->Ports.Data[p];
        if (port->Kind == ImGuiAppPortKind_DataOut) { ImNodes::BeginOutputAttribute(port->Id); ImGui::TextUnformatted(port->Name); ImNodes::EndOutputAttribute(); }
        else if (port->Kind == ImGuiAppPortKind_ChildOut) { ImNodes::BeginOutputAttribute(port->Id); ImGui::TextUnformatted(port->Name); ImNodes::EndOutputAttribute(); }
      }

      ImGui::EndAppNode();
    }

    ImGui::DrawAppNodeLinks(&g->Links);
    ImNodes::EndNodeEditor();

    // Persist canvas layout for save/load.
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      g->Nodes.Data[i].GridPos = ImNodes::GetNodeGridSpacePos(g->Nodes.Data[i].Id);
      g->Nodes.Data[i].HasGridPos = true;
    }

    // In-widget node palette: right-click the canvas to add any node kind. The graph is the app, so every
    // pillar -- layers, windows, sidebars, controls -- is created here, inside the editor itself.
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
      ImGui::OpenPopup("##AppGraphAdd");
    if (ImGui::BeginPopup("##AppGraphAdd"))
    {
      ImGui::TextDisabled("Add node");
      ImGui::Separator();
      ImGuiAppNode* added = nullptr;
      if (ImGui::MenuItem("Control"))   added = AppGraphAddNode(g, ImGuiAppNodeKind_Control, "NewControl");
      if (ImGui::BeginMenu("Layer"))
      {
        if (ImGui::MenuItem("Task"))    { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "TaskLayer");    added->LayerType = ImGuiAppLayerType_Task;    }
        if (ImGui::MenuItem("Command")) { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "CommandLayer"); added->LayerType = ImGuiAppLayerType_Command; }
        if (ImGui::MenuItem("Status"))  { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "StatusLayer");  added->LayerType = ImGuiAppLayerType_Status;  }
        if (ImGui::MenuItem("Window"))  { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "WindowLayer");  added->LayerType = ImGuiAppLayerType_Window;  }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Window"))    added = AppGraphAddNode(g, ImGuiAppNodeKind_Window,  "Window");
      if (ImGui::MenuItem("Sidebar"))   added = AppGraphAddNode(g, ImGuiAppNodeKind_Sidebar, "Sidebar");
      IM_UNUSED(added);                 // AppGraphAddNode already stages a staggered grid position
      ImGui::EndPopup();
    }

    char err[128];
    ImGui::CaptureAppGraphLinks(g, err, IM_ARRAYSIZE(err));
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Topological order + whole-graph codegen
  //-----------------------------------------------------------------------------

  static void AppNodeBaseName(const ImGuiAppNode* n, char* out, size_t out_size)
  {
    if (n->IsBuiltin && n->TypeName[0])
      ImStrncpy(out, n->TypeName, out_size);
    else
      AppSanitizeIdentifier(out, out_size, n->Draft.Name);
  }

  // Persist-field type of a named field in a draft, or -1 if absent. Used to gate binding emission on a
  // matching field type (design section 5: drop a binding whose endpoints disagree on type).
  static int AppDraftFieldType(const ImGuiAppNodeDraft* d, const char* name)
  {
    for (int i = 0; i < d->PersistFields.Size; i++)
      if (strcmp(d->PersistFields.Data[i].Name, name) == 0)
        return (int)d->PersistFields.Data[i].Type;
    return -1;
  }

  static void AppNodeDataTypeName(const ImGuiAppNode* n, char* out, size_t out_size)
  {
    if (n->DataTypeName[0])
      ImStrncpy(out, n->DataTypeName, out_size);
    else
    {
      char base[IM_LABEL_SIZE];
      AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
      ImFormatString(out, (int)out_size, "%sData", base);
    }
  }

  // "RandomTime" -> "random_time" (a readable dependency parameter name).
  static void AppToSnake(char* dst, size_t dst_size, const char* src)
  {
    char id[IM_LABEL_SIZE];
    AppSanitizeIdentifier(id, IM_ARRAYSIZE(id), src);
    size_t n = 0;
    for (const char* s = id; *s != 0 && n + 1 < dst_size; s++)
    {
      const char c = *s;
      if (c >= 'A' && c <= 'Z')
      {
        if (s != id && n + 2 < dst_size) dst[n++] = '_';
        dst[n++] = (char)(c - 'A' + 'a');
      }
      else
        dst[n++] = c;
    }
    dst[n] = 0;
  }

  static const ImGuiAppNode* AppGraphFindNodeConst(const ImGuiAppGraph* g, int node_id)
  {
    for (int i = 0; i < g->Nodes.Size; i++)
      if (g->Nodes.Data[i].Id == node_id) return &g->Nodes.Data[i];
    return nullptr;
  }

  bool AppGraphTopoOrder(const ImGuiAppGraph* g, ImVector<int>* out_control_ids, char* err, int err_size)
  {
    IM_ASSERT(g != nullptr && out_control_ids != nullptr);
    out_control_ids->clear();
    if (err && err_size > 0) err[0] = 0;

    // Collect control node ids (stable order = node order, for deterministic output).
    ImVector<int> ctrl;
    for (int i = 0; i < g->Nodes.Size; i++)
      if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_Control)
        ctrl.push_back(g->Nodes.Data[i].Id);

    // In-degree = number of incoming data edges (producer -> this consumer).
    ImVector<int> indeg;
    indeg.resize(ctrl.Size);
    for (int i = 0; i < ctrl.Size; i++) indeg.Data[i] = 0;

    auto ctrl_index = [&](int node_id) -> int
    {
      for (int i = 0; i < ctrl.Size; i++) if (ctrl.Data[i] == node_id) return i;
      return -1;
    };

    for (int li = 0; li < g->Links.Size; li++)
    {
      if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Data) continue;
      const int consumer = AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr);
      const int ci = ctrl_index(consumer);
      if (ci >= 0) indeg.Data[ci]++;
    }

    // Kahn: repeatedly take a zero-in-degree control in stable order.
    ImVector<bool> done;
    done.resize(ctrl.Size);
    for (int i = 0; i < ctrl.Size; i++) done.Data[i] = false;

    for (int produced = 0; produced < ctrl.Size; produced++)
    {
      int pick = -1;
      for (int i = 0; i < ctrl.Size; i++)
        if (!done.Data[i] && indeg.Data[i] == 0) { pick = i; break; }

      if (pick < 0)
      {
        // Remaining nodes form a cycle; name one for the message.
        const char* who = "control";
        for (int i = 0; i < ctrl.Size; i++)
          if (!done.Data[i]) { const ImGuiAppNode* nn = AppGraphFindNodeConst(g, ctrl.Data[i]); if (nn) who = nn->Draft.Name; break; }
        char msg[160];
        ImFormatString(msg, IM_ARRAYSIZE(msg), "dependency cycle at %s", who);
        AppSetErr(err, err_size, msg);
        out_control_ids->clear();
        return false;
      }

      done.Data[pick] = true;
      out_control_ids->push_back(ctrl.Data[pick]);

      // Decrement consumers fed by the picked producer.
      for (int li = 0; li < g->Links.Size; li++)
      {
        if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Data) continue;
        if (AppGraphPortOwnerId(g, g->Links.Data[li].StartAttr) != ctrl.Data[pick]) continue;
        const int consumer = AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr);
        const int ci = ctrl_index(consumer);
        if (ci >= 0 && !done.Data[ci] && indeg.Data[ci] > 0) indeg.Data[ci]--;
      }
    }
    return true;
  }

  // Distinct producer node ids feeding a consumer control via data edges, in node order (deterministic).
  static void AppGraphConsumerDeps(const ImGuiAppGraph* g, int consumer_node_id, ImVector<int>* out_producers)
  {
    out_producers->clear();
    for (int i = 0; i < g->Nodes.Size; i++)   // iterate nodes for stable order
    {
      const int producer_id = g->Nodes.Data[i].Id;
      for (int li = 0; li < g->Links.Size; li++)
      {
        if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Data) continue;
        if (AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr) != consumer_node_id) continue;
        if (AppGraphPortOwnerId(g, g->Links.Data[li].StartAttr) != producer_id) continue;
        bool dup = false;
        for (int d = 0; d < out_producers->Size; d++) if (out_producers->Data[d] == producer_id) { dup = true; break; }
        if (!dup) out_producers->push_back(producer_id);
      }
    }
  }

  // Emit a control struct (and its data structs) with derived dependencies + binding assignment lines.
  static void AppEmitControlWithDeps(const ImGuiAppGraph* g, const ImGuiAppNode* n, ImGuiTextBuffer* out)
  {
    char base[IM_LABEL_SIZE];
    AppNodeBaseName(n, base, IM_ARRAYSIZE(base));

    // Data structs.
    out->appendf("struct %sData\n{\n", base);
    for (int i = 0; i < n->Draft.PersistFields.Size; i++)
      AppEmitFieldDecl(out, &n->Draft.PersistFields.Data[i]);
    out->appendf("};\n\n");
    out->appendf("struct %sTempData\n{\n", base);
    for (int i = 0; i < n->Draft.TempFields.Size; i++)
      AppEmitFieldDecl(out, &n->Draft.TempFields.Data[i]);
    out->appendf("};\n\n");

    // Dependency producers.
    ImVector<int> deps;
    AppGraphConsumerDeps(g, n->Id, &deps);

    // Control struct header with template dependency args.
    out->appendf("struct %s : ImGuiAppControl<%sData, %sTempData", base, base, base);
    for (int d = 0; d < deps.Size; d++)
    {
      const ImGuiAppNode* dn = AppGraphFindNodeConst(g, deps.Data[d]);
      char dtype[IM_LABEL_SIZE]; AppNodeDataTypeName(dn, dtype, IM_ARRAYSIZE(dtype));
      out->appendf(", %s", dtype);
    }
    out->appendf(">\n{\n");

    // Build dep (type, param) pairs once.
    auto emit_dep_params = [&](ImGuiTextBuffer* o)
    {
      for (int d = 0; d < deps.Size; d++)
      {
        const ImGuiAppNode* dn = AppGraphFindNodeConst(g, deps.Data[d]);
        char dtype[IM_LABEL_SIZE]; AppNodeDataTypeName(dn, dtype, IM_ARRAYSIZE(dtype));
        char dparam[IM_LABEL_SIZE]; AppToSnake(dparam, IM_ARRAYSIZE(dparam), dn->Draft.Name);
        o->appendf(", const %s* %s", dtype, dparam);
      }
    };

    out->appendf("  virtual void OnInitialize(ImGuiApp* app, %sData* data", base);
    emit_dep_params(out);
    out->appendf(") const override final\n  {\n    IM_UNUSED(app); IM_UNUSED(data);\n    // TODO: initialize persistent data\n  }\n\n");

    out->appendf("  virtual void OnUpdate(float dt, %sData* data, const %sTempData* temp_data, const %sTempData* last_temp_data", base, base, base);
    emit_dep_params(out);
    out->appendf(") const override final\n  {\n    IM_UNUSED(dt); IM_UNUSED(data); IM_UNUSED(temp_data); IM_UNUSED(last_temp_data);\n");
    // Field bindings -> assignment lines.
    for (int d = 0; d < deps.Size; d++)
    {
      const ImGuiAppNode* dn = AppGraphFindNodeConst(g, deps.Data[d]);
      char dparam[IM_LABEL_SIZE]; AppToSnake(dparam, IM_ARRAYSIZE(dparam), dn->Draft.Name);
      // Find the data link producer==dn -> consumer==n, then its bindings.
      for (int li = 0; li < g->Links.Size; li++)
      {
        if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Data) continue;
        if (AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr) != n->Id) continue;
        if (AppGraphPortOwnerId(g, g->Links.Data[li].StartAttr) != dn->Id) continue;
        const int link_id = g->Links.Data[li].Id;
        for (int bi = 0; bi < g->Bindings.Size; bi++)
        {
          if (g->Bindings.Data[bi].LinkId != link_id) continue;
          char dst_id[IM_LABEL_SIZE]; AppSanitizeIdentifier(dst_id, IM_ARRAYSIZE(dst_id), g->Bindings.Data[bi].DstField);
          char src_id[IM_LABEL_SIZE]; AppSanitizeIdentifier(src_id, IM_ARRAYSIZE(src_id), g->Bindings.Data[bi].SrcField);
          // Type gate: a builtin producer's fields are unknown here (its struct already exists) so trust it;
          // for a drafted producer require both fields to resolve and share a type.
          const int dst_t = AppDraftFieldType(&n->Draft, g->Bindings.Data[bi].DstField);
          const int src_t = AppDraftFieldType(&dn->Draft, g->Bindings.Data[bi].SrcField);
          const bool types_ok = dn->IsBuiltin || (dst_t >= 0 && src_t >= 0 && dst_t == src_t);
          if (dst_id[0] && src_id[0] && types_ok)
            out->appendf("    data->%s = %s->%s;\n", dst_id, dparam, src_id);
        }
      }
    }
    out->appendf("  }\n\n");

    out->appendf("  virtual void OnRender(const %sData* data, %sTempData* temp_data", base, base);
    emit_dep_params(out);
    out->appendf(") const override final\n  {\n    IM_UNUSED(data); IM_UNUSED(temp_data);\n    // TODO: render widgets, write temp_data\n  }\n};\n\n");
  }

  // Containment parent node id for a child node (its ChildOut -> a ChildIn), or -1.
  static int AppGraphParentOf(const ImGuiAppGraph* g, int child_node_id)
  {
    for (int li = 0; li < g->Links.Size; li++)
    {
      if (g->Links.Data[li].Kind != ImGuiAppEdgeKind_Containment) continue;
      if (AppGraphPortOwnerId(g, g->Links.Data[li].StartAttr) != child_node_id) continue;
      return AppGraphPortOwnerId(g, g->Links.Data[li].EndAttr);
    }
    return -1;
  }

  void GenerateAppGraphCode(const ImGuiAppGraph* g, ImGuiTextBuffer* out)
  {
    IM_ASSERT(g != nullptr && out != nullptr);

    // 1) Topo order of controls (producers before consumers).
    ImVector<int> order;
    char err[160];
    if (!AppGraphTopoOrder(g, &order, err, IM_ARRAYSIZE(err)))
    {
      out->appendf("// codegen aborted: %s\n", err[0] ? err : "dependency cycle");
      return;
    }

    // 2) Emit data structs + control structs for DRAFTED controls in topo order (builtin types already exist).
    for (int i = 0; i < order.Size; i++)
    {
      const ImGuiAppNode* n = AppGraphFindNodeConst(g, order.Data[i]);
      if (n && !n->IsBuiltin)
        AppEmitControlWithDeps(g, n, out);
    }

    // 3) Bring-up function: layers, then windows/sidebars, then controls in topo order.
    out->appendf("void SetupApp(ImGuiApp* app, ImGuiViewport* vp)\n{\n  IM_UNUSED(vp);\n");

    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Kind == ImGuiAppNodeKind_Layer)
        out->appendf("  PushAppLayer<%s>(app);\n", AppLayerTypeName(n->LayerType));
    }
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Kind == ImGuiAppNodeKind_Window)
      {
        char base[IM_LABEL_SIZE]; AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
        out->appendf("  PushAppWindow<%s>(app);\n", base);
      }
    }
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Kind == ImGuiAppNodeKind_Sidebar)
      {
        char base[IM_LABEL_SIZE]; AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
        out->appendf("  PushAppSidebar<%s>(app, vp, ImGuiDir_Down, 0.0f, ImGuiWindowFlags_None);\n", base);
      }
    }
    for (int i = 0; i < order.Size; i++)
    {
      const ImGuiAppNode* n = AppGraphFindNodeConst(g, order.Data[i]);
      if (n == nullptr) continue;
      char base[IM_LABEL_SIZE]; AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
      const int parent = AppGraphParentOf(g, n->Id);
      const ImGuiAppNode* pn = parent >= 0 ? AppGraphFindNodeConst(g, parent) : nullptr;
      if (pn && pn->Kind == ImGuiAppNodeKind_Sidebar)
        out->appendf("  PushSidebarControl<%s>(app, app->Sidebars.back()); // hosted by %s\n", base, pn->Draft.Name);
      else if (pn && pn->Kind == ImGuiAppNodeKind_Window)
        out->appendf("  PushAppControl<%s>(app); // TODO: host in window '%s' (PushWindowControl unimplemented)\n", base, pn->Draft.Name);
      else
        out->appendf("  PushAppControl<%s>(app);\n", base);
    }
    out->appendf("}\n");
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Whole-graph persistence (SaveAppGraph / LoadAppGraph, legacy [Draft] ingest)
  //-----------------------------------------------------------------------------

  bool SaveAppGraph(const char* path, const ImGuiAppGraph* g)
  {
    IM_ASSERT(path != nullptr && g != nullptr);

    ImGuiTextBuffer buf;
    buf.appendf("[Graph]\n");
    buf.appendf("NextId=%d\n", g->NextId);
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      buf.appendf("[Node]\n");
      buf.appendf("Id=%d\n", n->Id);
      buf.appendf("Kind=%d\n", (int)n->Kind);
      buf.appendf("Name=%s\n", n->Draft.Name);
      buf.appendf("Builtin=%d\n", n->IsBuiltin ? 1 : 0);
      buf.appendf("Type=%s\n", n->TypeName);
      buf.appendf("DataType=%s\n", n->DataTypeName);
      buf.appendf("LayerType=%d\n", (int)n->LayerType);
      buf.appendf("Body=%d\n", n->BodyAttrId);
      buf.appendf("Pos=%.1f,%.1f\n", n->GridPos.x, n->GridPos.y);
      for (int p = 0; p < n->Ports.Size; p++)
        buf.appendf("Port=%d,%d,%s,%u\n", n->Ports.Data[p].Id, (int)n->Ports.Data[p].Kind, n->Ports.Data[p].Name, (unsigned)n->Ports.Data[p].DataTypeId);
      for (int f = 0; f < n->Draft.PersistFields.Size; f++)
        buf.appendf("Persist=%s,%d,%d\n", n->Draft.PersistFields.Data[f].Name, (int)n->Draft.PersistFields.Data[f].Type, n->Draft.PersistFields.Data[f].ArraySize);
      for (int f = 0; f < n->Draft.TempFields.Size; f++)
        buf.appendf("Temp=%s,%d,%d\n", n->Draft.TempFields.Data[f].Name, (int)n->Draft.TempFields.Data[f].Type, n->Draft.TempFields.Data[f].ArraySize);
    }
    for (int i = 0; i < g->Links.Size; i++)
      buf.appendf("Link=%d,%d,%d,%d\n", g->Links.Data[i].Id, g->Links.Data[i].StartAttr, g->Links.Data[i].EndAttr, (int)g->Links.Data[i].Kind);
    for (int i = 0; i < g->Bindings.Size; i++)
      buf.appendf("Bind=%d,%s,%s\n", g->Bindings.Data[i].LinkId, g->Bindings.Data[i].DstField, g->Bindings.Data[i].SrcField);

    ImFileHandle fh = ImFileOpen(path, "wt");
    if (fh == nullptr)
      return false;
    ImFileWrite(buf.c_str(), sizeof(char), (ImU64)buf.size(), fh);
    ImFileClose(fh);
    return true;
  }

  // Parse "id,kind,name,datatype" into a port record.
  static void AppGraphParsePort(ImGuiAppNode* n, const char* line)
  {
    ImGuiAppNodePort p;
    int id = 0, kind = 0; unsigned tid = 0;
    if (sscanf(line, "%d,%d,%255[^,],%u", &id, &kind, p.Name, &tid) >= 3)
    {
      p.Id = id;
      p.Kind = (ImGuiAppPortKind)kind;
      p.DataTypeId = (ImGuiID)tid;
      n->Ports.push_back(p);
    }
  }

  // Synthesize the standard ports for a legacy [Draft]-migrated Control node (no [Port] lines on disk).
  static void AppGraphSynthesizeControlPorts(ImGuiAppGraph* g, ImGuiAppNode* n)
  {
    AppGraphStampPorts(g, n);
  }

  bool LoadAppGraph(const char* path, ImGuiAppGraph* g)
  {
    IM_ASSERT(path != nullptr && g != nullptr);

    size_t data_size = 0;
    char* data = (char*)ImFileLoadToMemory(path, "rb", &data_size, 1);
    if (data == nullptr)
      return false;

    g->Nodes.clear();
    g->Links.clear();
    g->Bindings.clear();
    g->NextId = 1;
    g->EditingNodeId = -1;

    bool is_legacy = true;          // until we see a [Graph]/[Node] header
    bool legacy_needs_ports = false;
    ImGuiAppNode* cur = nullptr;
    int max_id = 0;

    char* p = data;
    while (*p)
    {
      char* eol = p;
      while (*eol != 0 && *eol != '\n') eol++;
      const char saved = *eol;
      *eol = 0;
      if (eol > p && eol[-1] == '\r') eol[-1] = 0;

      if (strncmp(p, "[Graph]", 7) == 0)
      {
        is_legacy = false;
      }
      else if (strncmp(p, "NextId=", 7) == 0)
      {
        g->NextId = atoi(p + 7);
        is_legacy = false;
      }
      else if (strncmp(p, "[Node]", 6) == 0)
      {
        is_legacy = false;
        if (cur && legacy_needs_ports) { AppGraphSynthesizeControlPorts(g, cur); legacy_needs_ports = false; }
        g->Nodes.push_back(ImGuiAppNode());
        cur = &g->Nodes.back();
        cur->Kind = ImGuiAppNodeKind_Control;
      }
      else if (strncmp(p, "[Draft]", 7) == 0)
      {
        // Legacy single/multi draft -> a Control node; ports are synthesized when the node closes.
        if (cur && legacy_needs_ports) { AppGraphSynthesizeControlPorts(g, cur); legacy_needs_ports = false; }
        g->Nodes.push_back(ImGuiAppNode());
        cur = &g->Nodes.back();
        cur->Kind = ImGuiAppNodeKind_Control;
        cur->Id = g->NextId++;
        cur->BodyAttrId = g->NextId++;
        const int idx = g->Nodes.Size - 1;
        cur->GridPos = ImVec2(40.0f + (idx % 5) * 36.0f, 200.0f + (idx % 5) * 36.0f);
        cur->HasGridPos = true; cur->_NeedsPlace = true;
        legacy_needs_ports = true;
      }
      else if (strncmp(p, "Id=", 3) == 0)        { if (cur) { cur->Id = atoi(p + 3); if (cur->Id > max_id) max_id = cur->Id; } }
      else if (strncmp(p, "Kind=", 5) == 0)      { if (cur) cur->Kind = (ImGuiAppNodeKind)atoi(p + 5); }
      else if (strncmp(p, "Name=", 5) == 0)      { if (cur) ImStrncpy(cur->Draft.Name, p + 5, IM_ARRAYSIZE(cur->Draft.Name)); }
      else if (strncmp(p, "Builtin=", 8) == 0)   { if (cur) cur->IsBuiltin = atoi(p + 8) != 0; }
      else if (strncmp(p, "Type=", 5) == 0)      { if (cur) ImStrncpy(cur->TypeName, p + 5, IM_ARRAYSIZE(cur->TypeName)); }
      else if (strncmp(p, "DataType=", 9) == 0)  { if (cur) ImStrncpy(cur->DataTypeName, p + 9, IM_ARRAYSIZE(cur->DataTypeName)); }
      else if (strncmp(p, "LayerType=", 10) == 0){ if (cur) cur->LayerType = (ImGuiAppLayerType)atoi(p + 10); }
      else if (strncmp(p, "Body=", 5) == 0)      { if (cur) { cur->BodyAttrId = atoi(p + 5); if (cur->BodyAttrId > max_id) max_id = cur->BodyAttrId; } }
      else if (strncmp(p, "Pos=", 4) == 0)       { if (cur) { float x = 0, y = 0; if (sscanf(p + 4, "%f,%f", &x, &y) == 2) { cur->GridPos = ImVec2(x, y); cur->HasGridPos = true; cur->_NeedsPlace = true; } } }
      else if (strncmp(p, "Port=", 5) == 0)      { if (cur) { AppGraphParsePort(cur, p + 5); int last = cur->Ports.Size ? cur->Ports.Data[cur->Ports.Size - 1].Id : 0; if (last > max_id) max_id = last; } }
      else if (strncmp(p, "Persist=", 8) == 0)   { if (cur) AppNodeParseField(&cur->Draft.PersistFields, p + 8); }
      else if (strncmp(p, "Temp=", 5) == 0)      { if (cur) AppNodeParseField(&cur->Draft.TempFields, p + 5); }
      else if (strncmp(p, "Link=", 5) == 0)
      {
        ImGuiAppNodeLink l; int kind = ImGuiAppEdgeKind_Data;
        const int got = sscanf(p + 5, "%d,%d,%d,%d", &l.Id, &l.StartAttr, &l.EndAttr, &kind);
        if (got >= 3) { l.Kind = (ImGuiAppEdgeKind)kind; g->Links.push_back(l); if (l.Id > max_id) max_id = l.Id; }
      }
      else if (strncmp(p, "Bind=", 5) == 0)
      {
        ImGuiAppFieldBinding b;
        if (sscanf(p + 5, "%d,%255[^,],%255[^\n]", &b.LinkId, b.DstField, b.SrcField) >= 1)
          g->Bindings.push_back(b);
      }

      if (saved == 0) break;
      p = eol + 1;
    }
    if (cur && legacy_needs_ports)
      AppGraphSynthesizeControlPorts(g, cur);

    IM_FREE(data);

    // Drop links whose endpoints don't resolve to any port (e.g. legacy inert attr ids that no synthesized
    // port matches), so the loaded model is always self-consistent.
    for (int li = g->Links.Size - 1; li >= 0; li--)
    {
      ImGuiAppNode* dummy = nullptr;
      const bool ok = AppGraphFindPort(g, g->Links.Data[li].StartAttr, &dummy) != nullptr
                   && AppGraphFindPort(g, g->Links.Data[li].EndAttr, &dummy) != nullptr;
      if (!ok)
      {
        const int link_id = g->Links.Data[li].Id;
        for (int bi = g->Bindings.Size - 1; bi >= 0; bi--)
          if (g->Bindings.Data[bi].LinkId == link_id) g->Bindings.erase(g->Bindings.Data + bi);
        g->Links.erase(g->Links.Data + li);
      }
    }

    // Ensure NextId stays ahead of every id present (legacy files carried none).
    if (g->NextId <= max_id)
      g->NextId = max_id + 1;
    IM_UNUSED(is_legacy);
    return true;
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Live mirror: reflect the running app's controls into the model
  //-----------------------------------------------------------------------------
  // Memory discipline (this is what corrupted the heap last time, now fixed): snapshot the running controls
  // into plain value records BEFORE mutating the graph; capture every node/port id by VALUE right after a node
  // is created; never dereference a node pointer across another AppGraph* call that can reallocate g->Nodes.

  // One desired live entry, snapshotted from the running app BEFORE any graph mutation.
  struct AppLiveWant
  {
    ImGuiID          Key;        // stable upsert key (so the node keeps its canvas position across frames)
    ImGuiAppNodeKind Kind;
    char             Name[IM_LABEL_SIZE];
    ImGuiAppLayerType LayerType;
    ImGuiID          DataId;     // control PersistData id (0 for non-controls)
    ImGuiID          Deps[16];
    int              DepCount;
  };

  static const char* AppLayerTypeName(ImGuiAppLayerType t);   // fwd (defined above)

  void BuildAppLiveGraph(const ImGuiApp* app, ImGuiAppGraph* g)
  {
    IM_ASSERT(g != nullptr);
    if (app == nullptr)
      return;

    // 1) Snapshot the WHOLE app composition (layers, windows, sidebars, controls). No graph access here.
    ImVector<AppLiveWant> want;

    // Layers: stable order from InitializeApp (Task, Command, Status, Window). Keyed by index.
    for (int i = 0; i < app->Layers.Size; i++)
    {
      AppLiveWant w; w.Kind = ImGuiAppNodeKind_Layer; w.Key = 0x4C000000u + (ImGuiID)i;
      w.DataId = 0; w.DepCount = 0;
      w.LayerType = (i < ImGuiAppLayerType_COUNT) ? (ImGuiAppLayerType)i : ImGuiAppLayerType_Task;
      if (i < ImGuiAppLayerType_COUNT) ImFormatString(w.Name, IM_ARRAYSIZE(w.Name), "%s", AppLayerTypeName((ImGuiAppLayerType)i));
      else                              ImFormatString(w.Name, IM_ARRAYSIZE(w.Name), "Layer %d", i);
      want.push_back(w);
    }
    // Windows / Sidebars: keyed by their unique Label.
    for (int i = 0; i < app->Windows.Size; i++)
    {
      const char* lbl = app->Windows.Data[i]->Label;
      AppLiveWant w; w.Kind = ImGuiAppNodeKind_Window; w.Key = AppConstantHash(lbl[0] ? lbl : "Window");
      w.DataId = 0; w.DepCount = 0; w.LayerType = ImGuiAppLayerType_Task;
      ImStrncpy(w.Name, lbl[0] ? lbl : "Window", IM_ARRAYSIZE(w.Name));
      want.push_back(w);
    }
    for (int i = 0; i < app->Sidebars.Size; i++)
    {
      const char* lbl = app->Sidebars.Data[i]->Label;
      AppLiveWant w; w.Kind = ImGuiAppNodeKind_Sidebar; w.Key = AppConstantHash(lbl[0] ? lbl : "Sidebar") + 1u;
      w.DataId = 0; w.DepCount = 0; w.LayerType = ImGuiAppLayerType_Task;
      ImStrncpy(w.Name, lbl[0] ? lbl : "Sidebar", IM_ARRAYSIZE(w.Name));
      want.push_back(w);
    }
    // Controls: keyed by runtime PersistData id; carry their dependency ids for edge discovery.
    for (int c = 0; c < app->Controls.Size; c++)
    {
      const ImGuiAppControlBase* ctrl = app->Controls.Data[c];
      if (ctrl == nullptr) continue;
      ImGuiID id = ctrl->GetControlDataID();
      if (id == 0) continue;
      AppLiveWant w; w.Kind = ImGuiAppNodeKind_Control; w.Key = id; w.DataId = id; w.LayerType = ImGuiAppLayerType_Task;
      ctrl->GetControlDataTypeName(w.Name, IM_ARRAYSIZE(w.Name));
      if (w.Name[0] == 0) ImStrncpy(w.Name, "Control", IM_ARRAYSIZE(w.Name));
      int n = ctrl->GetControlDependencyIDs(w.Deps, IM_ARRAYSIZE(w.Deps));
      if (n < 0) n = 0; if (n > IM_ARRAYSIZE(w.Deps)) n = IM_ARRAYSIZE(w.Deps);
      w.DepCount = n;
      want.push_back(w);
    }

    // 2) Remove live nodes whose key is no longer wanted (collect ids first; removal mutates g->Nodes).
    ImVector<int> stale;
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      if (!g->Nodes.Data[i].IsLive) continue;
      bool wanted = false;
      for (int w = 0; w < want.Size; w++) if (want.Data[w].Key == g->Nodes.Data[i].LiveKey) { wanted = true; break; }
      if (!wanted) stale.push_back(g->Nodes.Data[i].Id);
    }
    for (int i = 0; i < stale.Size; i++)
      AppGraphRemoveNode(g, stale.Data[i]);

    // 3) Upsert: keep existing live nodes (DO NOT touch their position); add only the missing ones, placed
    //    once via the staggered default. Capture per-key port ids by value for edge building.
    struct AppLiveMade { ImGuiID Key; int OutPort; int InPort; };
    ImVector<AppLiveMade> made;
    for (int w = 0; w < want.Size; w++)
    {
      // already present?
      bool present = false;
      for (int i = 0; i < g->Nodes.Size; i++)
        if (g->Nodes.Data[i].IsLive && g->Nodes.Data[i].LiveKey == want.Data[w].Key) { present = true; break; }

      if (!present)
      {
        ImGuiAppNode* node = (want.Data[w].Kind == ImGuiAppNodeKind_Control)
          ? AppGraphAddBuiltin(g, ImGuiAppNodeKind_Control, want.Data[w].Name, want.Data[w].Name)
          : AppGraphAddNode(g, want.Data[w].Kind, want.Data[w].Name);
        node->IsLive = true;
        node->LiveKey = want.Data[w].Key;
        node->LayerType = want.Data[w].LayerType;
        for (int p = 0; p < node->Ports.Size; p++)
          if (node->Ports.Data[p].Kind == ImGuiAppPortKind_DataOut)
            node->Ports.Data[p].DataTypeId = want.Data[w].DataId;

        // Lay live nodes out in clean per-kind columns (layers | windows | sidebars | controls), one row per
        // node. Only new nodes are placed; existing live nodes keep wherever the user dragged them.
        const int col = (want.Data[w].Kind == ImGuiAppNodeKind_Layer)   ? 0
                      : (want.Data[w].Kind == ImGuiAppNodeKind_Window)  ? 1
                      : (want.Data[w].Kind == ImGuiAppNodeKind_Sidebar) ? 2 : 3;
        int row = 0;
        for (int j = 0; j < w; j++) if (want.Data[j].Kind == want.Data[w].Kind) row++;
        node->GridPos = ImVec2(30.0f + col * 230.0f, 30.0f + row * 100.0f);
        node->_NeedsPlace = true;
      }
    }
    // Build the per-key port map (after all adds, pointers are stable until next mutation; we only read here).
    for (int w = 0; w < want.Size; w++)
    {
      AppLiveMade lm; lm.Key = want.Data[w].Key; lm.OutPort = 0; lm.InPort = 0;
      for (int i = 0; i < g->Nodes.Size; i++)
      {
        if (!g->Nodes.Data[i].IsLive || g->Nodes.Data[i].LiveKey != want.Data[w].Key) continue;
        for (int p = 0; p < g->Nodes.Data[i].Ports.Size; p++)
        {
          if (g->Nodes.Data[i].Ports.Data[p].Kind == ImGuiAppPortKind_DataOut) lm.OutPort = g->Nodes.Data[i].Ports.Data[p].Id;
          else if (g->Nodes.Data[i].Ports.Data[p].Kind == ImGuiAppPortKind_DataIn) lm.InPort = g->Nodes.Data[i].Ports.Data[p].Id;
        }
        break;
      }
      made.push_back(lm);
    }

    // 4) Rebuild live data edges (between two live nodes). Tear down old, re-derive from control deps.
    for (int li = g->Links.Size - 1; li >= 0; li--)
    {
      ImGuiAppNode* a = nullptr; ImGuiAppNode* b = nullptr;
      AppGraphFindPort(g, g->Links.Data[li].StartAttr, &a);
      AppGraphFindPort(g, g->Links.Data[li].EndAttr, &b);
      if (a && b && a->IsLive && b->IsLive)
        g->Links.erase(g->Links.Data + li);
    }
    for (int w = 0; w < want.Size; w++)
    {
      if (want.Data[w].Kind != ImGuiAppNodeKind_Control || want.Data[w].DepCount == 0) continue;
      int consumer_in = 0;
      for (int m = 0; m < made.Size; m++) if (made.Data[m].Key == want.Data[w].Key) { consumer_in = made.Data[m].InPort; break; }
      if (consumer_in == 0) continue;
      for (int d = 0; d < want.Data[w].DepCount; d++)
      {
        int producer_out = 0;
        for (int m = 0; m < made.Size; m++) if (made.Data[m].Key == want.Data[w].Deps[d]) { producer_out = made.Data[m].OutPort; break; }
        if (producer_out == 0) continue;
        ImGuiAppNodeLink l; l.Id = AppGraphAllocId(g); l.StartAttr = producer_out; l.EndAttr = consumer_in; l.Kind = ImGuiAppEdgeKind_Data;
        g->Links.push_back(l);
      }
    }

    // 5) Promotion preview: a design control node whose emitted data type name matches a live node.
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      ImGuiAppNode* n = &g->Nodes.Data[i];
      n->IsPromoted = false;
      if (n->IsLive || n->Kind != ImGuiAppNodeKind_Control) continue;
      char wantname[IM_LABEL_SIZE];
      AppNodeDataTypeName(n, wantname, IM_ARRAYSIZE(wantname));
      for (int j = 0; j < g->Nodes.Size; j++)
        if (g->Nodes.Data[j].IsLive && g->Nodes.Data[j].DataTypeName[0] && strcmp(g->Nodes.Data[j].DataTypeName, wantname) == 0)
        { n->IsPromoted = true; break; }
    }
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Scene-hierarchy tree (outliner)
  //-----------------------------------------------------------------------------

  void ShowAppGraphTree(const ImGuiApp* app, ImGuiAppGraph* g, int* selected_node_id)
  {
    IM_ASSERT(g != nullptr);

    if (app != nullptr)
    {
      ImGui::SeparatorText("Live app");

      if (ImGui::TreeNodeEx("##layers", ImGuiTreeNodeFlags_DefaultOpen, "Layers (%d)", app->Layers.Size))
      {
        for (int i = 0; i < app->Layers.Size; i++)
          ImGui::BulletText("%s", (i < ImGuiAppLayerType_COUNT) ? AppLayerTypeName((ImGuiAppLayerType)i) : "Layer");
        ImGui::TreePop();
      }
      if (ImGui::TreeNodeEx("##windows", ImGuiTreeNodeFlags_DefaultOpen, "Windows (%d)", app->Windows.Size))
      {
        for (int i = 0; i < app->Windows.Size; i++)
          ImGui::BulletText("%s", app->Windows.Data[i]->Label[0] ? app->Windows.Data[i]->Label : "Window");
        ImGui::TreePop();
      }
      if (ImGui::TreeNodeEx("##sidebars", ImGuiTreeNodeFlags_DefaultOpen, "Sidebars (%d)", app->Sidebars.Size))
      {
        for (int i = 0; i < app->Sidebars.Size; i++)
          ImGui::BulletText("%s", app->Sidebars.Data[i]->Label[0] ? app->Sidebars.Data[i]->Label : "Sidebar");
        ImGui::TreePop();
      }
      if (ImGui::TreeNodeEx("##controls", ImGuiTreeNodeFlags_DefaultOpen, "Controls (%d)", app->Controls.Size))
      {
        for (int i = 0; i < app->Controls.Size; i++)
        {
          char nm[IM_LABEL_SIZE]; app->Controls.Data[i]->GetControlDataTypeName(nm, IM_ARRAYSIZE(nm));
          ImGui::BulletText("%s", nm[0] ? nm : "Control");
        }
        ImGui::TreePop();
      }
    }

    ImGui::SeparatorText("Graph");
    if (ImGui::TreeNodeEx("##graph", ImGuiTreeNodeFlags_DefaultOpen, "Nodes (%d)", g->Nodes.Size))
    {
      for (int i = 0; i < g->Nodes.Size; i++)
      {
        ImGuiAppNode* n = &g->Nodes.Data[i];
        const char* kind = AppNodeKindName(n->Kind);
        char label[IM_LABEL_SIZE + 32];
        ImFormatString(label, IM_ARRAYSIZE(label), "%s  (%s%s)##n%d",
            n->Draft.Name[0] ? n->Draft.Name : "(unnamed)", kind, n->IsLive ? ", live" : "", n->Id);
        const bool sel = selected_node_id && *selected_node_id == n->Id;
        if (ImGui::Selectable(label, sel))
        {
          if (selected_node_id) *selected_node_id = n->Id;
          ImNodes::ClearNodeSelection();
          ImNodes::SelectNode(n->Id);
        }
      }
      ImGui::TreePop();
    }
  }
}
