// ImGuiAppLayer data-driven node tooling: imnodes scaffolding. Keeping imnodes confined to
// this translation unit lets imgui_applayer_nodes.h stay free of the imnodes dependency, so
// callers reflect over their data without pulling in the node-editor backend at the header level.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer_nodes.h"
#include "imnodes.h"

#include <stdio.h>                         // sscanf (graph text parse)
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
}
