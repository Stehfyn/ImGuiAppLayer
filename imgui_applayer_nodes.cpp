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

  // Compact row-delete button: an X drawn from two strokes over a hover/active disc, instead of a
  // SmallButton literally labeled "X" (which read as a stray glyph and crowded the row). Square and
  // sized to the row's frame height so it lines up with the adjacent InputText/Combo. Returns true on click.
  static bool AppRowDeleteButton(const char* str_id)
  {
    const float sz = ImGui::GetFrameHeight();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(str_id, ImVec2(sz, sz));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 center = ImVec2(pos.x + sz * 0.5f, pos.y + sz * 0.5f);
    if (hovered || held)
      dl->AddCircleFilled(center, sz * 0.5f, ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered));

    // Brighten the cross on hover so it reads as "active delete"; dim at rest so the row isn't noisy.
    const ImU32 cross = ImGui::GetColorU32(ImGuiCol_Text, (hovered || held) ? 1.0f : 0.55f);
    const float arm = sz * 0.22f;
    const float th = ImMax(1.0f, IM_ROUND(sz * 0.09f));
    dl->AddLine(ImVec2(center.x - arm, center.y - arm), ImVec2(center.x + arm, center.y + arm), cross, th);
    dl->AddLine(ImVec2(center.x + arm, center.y - arm), ImVec2(center.x - arm, center.y + arm), cross, th);
    return clicked;
  }

  //-----------------------------------------------------------------------------
  // [SECTION] Blender-style field widgets (node body)
  // Flat, rounded, hover-lit fields. Text: left-aligned, click to edit in place. Enum: centered value with
  // prev/next step arrows on hover + a dropdown. Int: centered value, horizontal drag to scrub (Shift = fine),
  // click to type, arrows to step. Self-drawn so the node body reads like Blender's property editor.
  //-----------------------------------------------------------------------------
  namespace
  {
    const ImU32 kBlFill      = IM_COL32( 88,  88,  88, 255);  // field at rest
    const ImU32 kBlFillHover = IM_COL32(112, 112, 112, 255);  // hover
    const ImU32 kBlFillEdit  = IM_COL32( 46,  46,  46, 255);  // recessed while typing
    const ImU32 kBlBorder    = IM_COL32(  0,   0,   0,  60);  // faint outline
    const ImU32 kBlText      = IM_COL32(229, 229, 229, 255);

    // Single text/number-edit focus across the whole UI (one at a time), plus the per-active-id drag scratch.
    ImGuiID g_bl_editing = 0;
    bool    g_bl_focus_pending = false;
    ImGuiID g_bl_drag_id = 0;
    float   g_bl_drag_accum = 0.0f;
    float   g_bl_press_x = 0.0f;
    bool    g_bl_dragged = false;
  }

  // Rounded fill + faint outline; returns the rounding so an in-place editor can match it.
  static float AppBlFieldBg(ImDrawList* dl, ImVec2 mn, ImVec2 mx, bool hovered, bool editing)
  {
    const float r = (mx.y - mn.y) * 0.28f;
    dl->AddRectFilled(mn, mx, editing ? kBlFillEdit : (hovered ? kBlFillHover : kBlFill), r);
    dl->AddRect(mn, mx, kBlBorder, r);
    return r;
  }

  static void AppBlText(ImDrawList* dl, ImVec2 mn, ImVec2 mx, const char* text, bool centered)
  {
    const float pad = (mx.y - mn.y) * 0.30f;
    const ImVec2 ts = ImGui::CalcTextSize(text);
    ImVec2 tp(centered ? mn.x + ((mx.x - mn.x) - ts.x) * 0.5f : mn.x + pad,
              mn.y + ((mx.y - mn.y) - ts.y) * 0.5f);
    if (tp.x < mn.x + pad) tp.x = mn.x + pad;
    dl->PushClipRect(mn, mx, true);
    dl->AddText(tp, kBlText, text);
    dl->PopClipRect();
  }

  // Left/right triangles centered in the arrow_w-wide strips at each edge (drawn only on hover).
  static void AppBlStepArrows(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float arrow_w)
  {
    const float cy = (mn.y + mx.y) * 0.5f;
    const float a = (mx.y - mn.y) * 0.16f;
    const float lx = mn.x + arrow_w * 0.5f, rx = mx.x - arrow_w * 0.5f;
    dl->AddTriangleFilled(ImVec2(lx + a, cy - a), ImVec2(lx + a, cy + a), ImVec2(lx - a, cy), kBlText);
    dl->AddTriangleFilled(ImVec2(rx - a, cy - a), ImVec2(rx - a, cy + a), ImVec2(rx + a, cy), kBlText);
  }

  // Flat rounded text field: left-aligned at rest, click to edit in place.
  static bool AppBlInputText(const char* str_id, char* buf, size_t buf_size, float width)
  {
    const float h = ImGui::GetFrameHeight();
    const ImVec2 mn = ImGui::GetCursorScreenPos();
    const ImVec2 mx(mn.x + width, mn.y + h);
    const ImGuiID id = ImGui::GetID(str_id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;

    if (g_bl_editing == id)
    {
      const float r = AppBlFieldBg(dl, mn, mx, true, true);
      ImGui::SetCursorScreenPos(mn);
      ImGui::SetNextItemWidth(width);
      ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
      ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0);
      ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 0);
      ImGui::PushStyleColor(ImGuiCol_Text, kBlText);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, r);
      if (g_bl_focus_pending) { ImGui::SetKeyboardFocusHere(); g_bl_focus_pending = false; }
      ImGui::PushID(id);
      changed = ImGui::InputText("##e", buf, buf_size, ImGuiInputTextFlags_AutoSelectAll);
      const bool done = ImGui::IsItemDeactivated();
      ImGui::PopID();
      ImGui::PopStyleVar();
      ImGui::PopStyleColor(4);
      if (done) g_bl_editing = 0;
    }
    else
    {
      ImGui::InvisibleButton(str_id, ImVec2(width, h));
      const bool hovered = ImGui::IsItemHovered();
      AppBlFieldBg(dl, mn, mx, hovered, false);
      AppBlText(dl, mn, mx, buf, false);
      if (ImGui::IsItemActivated()) { g_bl_editing = id; g_bl_focus_pending = true; }
      if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    }
    return changed;
  }

  // Flat rounded enum: centered value, hover shows L/R step arrows (click edges to step), click center for dropdown.
  static bool AppBlEnum(const char* str_id, float width, int* v, const char* (*name_of)(int), int count)
  {
    const float h = ImGui::GetFrameHeight();
    const ImVec2 mn = ImGui::GetCursorScreenPos();
    const ImVec2 mx(mn.x + width, mn.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;

    ImGui::InvisibleButton(str_id, ImVec2(width, h));
    const bool hovered = ImGui::IsItemHovered();
    AppBlFieldBg(dl, mn, mx, hovered, false);
    const float arrow_w = h * 0.7f;
    if (hovered) AppBlStepArrows(dl, mn, mx, arrow_w);
    AppBlText(dl, mn, mx, name_of(*v), true);

    if (ImGui::IsItemActivated())
    {
      const float rx = ImGui::GetIO().MousePos.x - mn.x;
      if (rx < arrow_w)             { *v = (*v - 1 + count) % count; changed = true; }
      else if (rx > width - arrow_w){ *v = (*v + 1) % count;        changed = true; }
      else                            ImGui::OpenPopup(str_id);
    }
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::BeginPopup(str_id))
    {
      for (int i = 0; i < count; i++)
        if (ImGui::Selectable(name_of(i), i == *v)) { *v = i; changed = true; }
      ImGui::EndPopup();
    }
    return changed;
  }

  // Flat rounded int: drag horizontally to scrub (Shift = fine), click edges to step, click center to type.
  static bool AppBlDragInt(const char* str_id, float width, int* v, int vmin, int vmax)
  {
    const float h = ImGui::GetFrameHeight();
    const ImVec2 mn = ImGui::GetCursorScreenPos();
    const ImVec2 mx(mn.x + width, mn.y + h);
    const ImGuiID id = ImGui::GetID(str_id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool changed = false;

    if (g_bl_editing == id)
    {
      const float r = AppBlFieldBg(dl, mn, mx, true, true);
      ImGui::SetCursorScreenPos(mn);
      ImGui::SetNextItemWidth(width);
      ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
      ImGui::PushStyleColor(ImGuiCol_Text, kBlText);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, r);
      if (g_bl_focus_pending) { ImGui::SetKeyboardFocusHere(); g_bl_focus_pending = false; }
      ImGui::PushID(id);
      changed = ImGui::InputInt("##e", v, 0, 0, ImGuiInputTextFlags_AutoSelectAll);
      const bool done = ImGui::IsItemDeactivated();
      ImGui::PopID();
      ImGui::PopStyleVar();
      ImGui::PopStyleColor(2);
      if (done) g_bl_editing = 0;
      *v = ImClamp(*v, vmin, vmax);
      return changed;
    }

    ImGui::InvisibleButton(str_id, ImVec2(width, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    AppBlFieldBg(dl, mn, mx, hovered || active, false);
    const float arrow_w = h * 0.7f;

    if (ImGui::IsItemActivated())
    {
      g_bl_drag_id = id; g_bl_drag_accum = 0.0f; g_bl_dragged = false;
      g_bl_press_x = ImGui::GetIO().MousePos.x - mn.x;
    }
    if (active && g_bl_drag_id == id)
    {
      const float dx = ImGui::GetIO().MouseDelta.x;
      if (!g_bl_dragged && ImAbs(ImGui::GetIO().MousePos.x - mn.x - g_bl_press_x) > 3.0f) g_bl_dragged = true;
      if (g_bl_dragged && dx != 0.0f)
      {
        g_bl_drag_accum += dx * (ImGui::GetIO().KeyShift ? 0.05f : 0.25f);
        const int d = (int)g_bl_drag_accum;
        if (d != 0) { *v = ImClamp(*v + d, vmin, vmax); g_bl_drag_accum -= d; changed = true; }
      }
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemDeactivated() && g_bl_drag_id == id)
    {
      if (!g_bl_dragged)
      {
        if (g_bl_press_x < arrow_w)              { *v = ImClamp(*v - 1, vmin, vmax); changed = true; }
        else if (g_bl_press_x > width - arrow_w) { *v = ImClamp(*v + 1, vmin, vmax); changed = true; }
        else                                     { g_bl_editing = id; g_bl_focus_pending = true; }
      }
      g_bl_drag_id = 0;
    }

    if (hovered && !active) { AppBlStepArrows(dl, mn, mx, arrow_w); ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
    char lbl[32]; ImFormatString(lbl, IM_ARRAYSIZE(lbl), "%d", *v);
    AppBlText(dl, mn, mx, lbl, true);
    return changed;
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

      const float em = ImGui::GetFontSize();
      AppBlInputText("##name", f->Name, IM_ARRAYSIZE(f->Name), em * 8.0f);

      ImGui::SameLine();
      int t = (int)f->Type;
      if (AppBlEnum("##type", em * 5.0f, &t, &AppFieldTypeName, ImGuiAppFieldType_COUNT))
        f->Type = (ImGuiAppFieldType)t;

      // String fields carry a buffer length -> a Blender drag-int for char[ArraySize].
      if (f->Type == ImGuiAppFieldType_String)
      {
        if (f->ArraySize <= 0) f->ArraySize = 128;
        ImGui::SameLine();
        AppBlDragInt("##size", em * 4.0f, &f->ArraySize, 1, 65536);
      }

      ImGui::SameLine();
      if (AppRowDeleteButton("##del"))
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
  // (DataIn is a wildcard multi-link intake -> 0). Layers are fixed root composition slots, so they deliberately
  // have no containment ports.
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

  static int AppGraphPlacementColumn(const ImGuiAppNode* n)
  {
    switch (n->Kind)
    {
    case ImGuiAppNodeKind_App:
    case ImGuiAppNodeKind_Layer:   return 0;
    case ImGuiAppNodeKind_Window:
    case ImGuiAppNodeKind_Sidebar: return 1;
    case ImGuiAppNodeKind_Control: return 2;
    default:                       return 2;
    }
  }

  static const float kAppGraphX0 = 40.0f;
  static const float kAppGraphY0 = 40.0f;
  static const float kAppGraphLayerNodeWidth = 520.0f;
  static const float kAppGraphLayerRowH = 145.0f;

  static int AppGraphPlacementRowHint(const ImGuiAppNode* n)
  {
    if (n->Kind == ImGuiAppNodeKind_App)
      return 0;
    if (n->Kind == ImGuiAppNodeKind_Layer)
      return 1 + (int)n->LayerType;
    return 0;
  }

  static bool AppGraphPlacementOccupied(const ImGuiAppGraph* g, const ImGuiAppNode* self, const ImVec2& pos)
  {
    const float min_dx = 230.0f;
    const float min_dy = 125.0f;
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* other = &g->Nodes.Data[i];
      if (other == self || !other->HasGridPos)
        continue;
      if (ImAbs(other->GridPos.x - pos.x) < min_dx && ImAbs(other->GridPos.y - pos.y) < min_dy)
        return true;
    }
    return false;
  }

  static ImVec2 AppGraphFindOpenPlacement(const ImGuiAppGraph* g, const ImGuiAppNode* n, const ImVec2& preferred, bool has_preferred)
  {
    const float col_w = 600.0f;
    const float row_h = kAppGraphLayerRowH;

    ImVec2 start = preferred;
    if (!has_preferred)
    {
      start.x = kAppGraphX0 + AppGraphPlacementColumn(n) * col_w;
      start.y = kAppGraphY0 + AppGraphPlacementRowHint(n) * row_h;
    }

    // Search down first: columns remain meaningful, and extra nodes under the same role stay grouped.
    for (int pass = 0; pass < 2; pass++)
    {
      for (int row = 0; row < 64; row++)
      {
        const ImVec2 pos = ImVec2(start.x + pass * col_w * 0.5f, start.y + row * row_h);
        if (!AppGraphPlacementOccupied(g, n, pos))
          return pos;
      }
    }
    return start;
  }

  static void AppGraphCollectVisibleLayerStack(const ImGuiAppGraph* g, bool show_live, ImVector<int>* out_node_ids)
  {
    out_node_ids->clear();
    for (int t = 0; t < ImGuiAppLayerType_COUNT; t++)
    {
      for (int i = 0; i < g->Nodes.Size; i++)
      {
        const ImGuiAppNode* n = &g->Nodes.Data[i];
        if (n->Kind != ImGuiAppNodeKind_Layer || n->LayerType != t)
          continue;
        if (!show_live && n->IsLive)
          continue;
        out_node_ids->push_back(n->Id);
      }
    }
  }

  static ImGuiAppNode* AppGraphFindNodeById(ImGuiAppGraph* g, int node_id)
  {
    for (int i = 0; i < g->Nodes.Size; i++)
      if (g->Nodes.Data[i].Id == node_id)
        return &g->Nodes.Data[i];
    return nullptr;
  }

  static float AppGraphLayerNodeHeight(int node_id)
  {
    const float h = ImNodes::GetNodeDimensions(node_id).y;
    return h > 1.0f ? h : kAppGraphLayerRowH;
  }

  static void AppGraphConstrainLayerColumn(ImGuiAppGraph* g, bool show_live, int anchor_node_id, const ImVec2* anchor_pos)
  {
    ImVector<int> ids;
    AppGraphCollectVisibleLayerStack(g, show_live, &ids);
    if (ids.Size == 0)
      return;

    const float x = kAppGraphX0;
    const float gap = 12.0f;

    // Default placement: pack the stack tight -- each fresh node sits directly below the previous one,
    // separated by exactly `gap`, using actual node heights (not a fixed row pitch that left big holes).
    // imnodes reports a node's height as ~0 until it has been submitted once, so AppGraphLayerNodeHeight
    // falls back to kAppGraphLayerRowH on the first frame. Only FINALIZE (HasGridPos) once every fresh
    // node's height is real; otherwise keep the placement provisional and re-pack next frame with true
    // heights -- finalizing early would freeze the fallback pitch and leave the holes we're removing.
    bool all_heights_known = true;
    for (int i = 0; i < ids.Size; i++)
    {
      ImGuiAppNode* n = AppGraphFindNodeById(g, ids.Data[i]);
      if (n != nullptr && !n->HasGridPos && ImNodes::GetNodeDimensions(n->Id).y <= 1.0f)
      {
        all_heights_known = false;
        break;
      }
    }
    float pack_y = kAppGraphY0;
    for (int i = 0; i < ids.Size; i++)
    {
      ImGuiAppNode* n = AppGraphFindNodeById(g, ids.Data[i]);
      if (n == nullptr)
        continue;
      if (!n->HasGridPos)
      {
        n->GridPos = ImVec2(x, pack_y);
        if (all_heights_known)
          n->HasGridPos = true;
      }
      pack_y = n->GridPos.y + AppGraphLayerNodeHeight(n->Id) + gap;
    }

    for (int i = 1; i < ids.Size; i++)
    {
      const int id = ids.Data[i];
      ImGuiAppNode* n = AppGraphFindNodeById(g, id);
      const float y = n ? n->GridPos.y : 0.0f;
      int j = i - 1;
      while (j >= 0)
      {
        ImGuiAppNode* prev = AppGraphFindNodeById(g, ids.Data[j]);
        if (prev == nullptr || prev->GridPos.y <= y)
          break;
        ids.Data[j + 1] = ids.Data[j];
        j--;
      }
      ids.Data[j + 1] = id;
    }

    int anchor_index = 0;
    if (anchor_node_id != 0)
      for (int i = 0; i < ids.Size; i++)
        if (ids.Data[i] == anchor_node_id)
        {
          anchor_index = i;
          break;
        }

    const bool anchor_is_interior = anchor_node_id != 0 && anchor_pos != nullptr && anchor_index > 0 && anchor_index + 1 < ids.Size;
    if (anchor_node_id != 0 && anchor_pos != nullptr)
    {
      ImGuiAppNode* anchor = AppGraphFindNodeById(g, anchor_node_id);
      if (anchor != nullptr && anchor_is_interior)
      {
        const ImGuiAppNode* prev = AppGraphFindNodeById(g, ids.Data[anchor_index - 1]);
        const ImGuiAppNode* next = AppGraphFindNodeById(g, ids.Data[anchor_index + 1]);
        float y = anchor_pos->y;
        if (prev != nullptr && next != nullptr)
        {
          const float min_y = prev->GridPos.y + AppGraphLayerNodeHeight(prev->Id) + gap;
          const float max_y = next->GridPos.y - AppGraphLayerNodeHeight(anchor->Id) - gap;
          if (min_y <= max_y)
          {
            if (y < min_y) y = min_y;
            if (y > max_y) y = max_y;
          }
          else
          {
            y = anchor->GridPos.y;
          }
        }
        anchor->GridPos = ImVec2(x, y);
      }
      else if (anchor != nullptr)
      {
        anchor->GridPos = ImVec2(x, anchor_pos->y);
      }
    }

    if (!anchor_is_interior)
    {
      for (int i = anchor_index - 1; i >= 0; i--)
      {
        ImGuiAppNode* n = AppGraphFindNodeById(g, ids.Data[i]);
        ImGuiAppNode* next = AppGraphFindNodeById(g, ids.Data[i + 1]);
        if (n == nullptr || next == nullptr)
          continue;
        const float max_y = next->GridPos.y - AppGraphLayerNodeHeight(n->Id) - gap;
        if (n->GridPos.y > max_y)
          n->GridPos.y = max_y;
      }
      for (int i = anchor_index + 1; i < ids.Size; i++)
      {
        ImGuiAppNode* n = AppGraphFindNodeById(g, ids.Data[i]);
        ImGuiAppNode* prev = AppGraphFindNodeById(g, ids.Data[i - 1]);
        if (n == nullptr || prev == nullptr)
          continue;
        const float min_y = prev->GridPos.y + AppGraphLayerNodeHeight(prev->Id) + gap;
        if (n->GridPos.y < min_y)
          n->GridPos.y = min_y;
      }
    }

    for (int i = 0; i < ids.Size; i++)
    {
      ImGuiAppNode* n = AppGraphFindNodeById(g, ids.Data[i]);
      if (n == nullptr)
        continue;
      const ImVec2 pos(x, n->GridPos.y);
      n->GridPos = pos;
      // Do NOT force HasGridPos here: a node placed provisionally this frame (heights not yet known)
      // must stay un-finalized so the default-placement pass re-packs it next frame with real heights.
      n->_NeedsPlace = true;
      ImNodes::SetNodeGridSpacePos(n->Id, pos);
    }
  }

  static void AppGraphPlaceNode(ImGuiAppGraph* g, ImGuiAppNode* n, const ImVec2* preferred)
  {
    n->GridPos = AppGraphFindOpenPlacement(g, n, preferred ? *preferred : ImVec2(0.0f, 0.0f), preferred != nullptr);
    // Layer nodes live in the tight-packed master column -- leave them UN-finalized so AppGraphConstrainLayerColumn
    // owns their Y (each layer packed directly below the previous by real height). Finalizing here froze the 145px
    // grid pitch FindOpenPlacement uses, which left big holes between layers ("not packed").
    n->HasGridPos = (n->Kind != ImGuiAppNodeKind_Layer);
    n->_NeedsPlace = true;
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
    if (kind == ImGuiAppNodeKind_Layer && name != nullptr)
    {
      if (strstr(name, "Command") != nullptr) n->LayerType = ImGuiAppLayerType_Command;
      else if (strstr(name, "Status") != nullptr) n->LayerType = ImGuiAppLayerType_Status;
      else if (strstr(name, "Window") != nullptr) n->LayerType = ImGuiAppLayerType_Window;
      else n->LayerType = ImGuiAppLayerType_Task;
    }
    n->BodyAttrId = AppGraphAllocId(g);
    AppGraphPlaceNode(g, n, nullptr);
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
    AppGraphPlaceNode(g, n, nullptr);
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

  static const ImGuiAppNode* AppGraphFindLayerOfType(const ImGuiAppGraph* g, ImGuiAppLayerType type, int ignore_node_id = 0)
  {
    IM_ASSERT(g != nullptr);
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Id != ignore_node_id && n->Kind == ImGuiAppNodeKind_Layer && n->LayerType == type)
        return n;
    }
    return nullptr;
  }

  bool AppGraphHasLayerType(const ImGuiAppGraph* g, ImGuiAppLayerType type)
  {
    return AppGraphFindLayerOfType(g, type) != nullptr;
  }

  static bool AppNodeIsCommandLayer(const ImGuiAppNode* n)
  {
    return n != nullptr && n->Kind == ImGuiAppNodeKind_Layer && n->LayerType == ImGuiAppLayerType_Command;
  }

  static int AppGraphCommandDefinitionCount(const ImGuiAppGraph* g)
  {
    int count = 0;
    for (int i = 0; i < g->Nodes.Size; i++)
      if (AppNodeIsCommandLayer(&g->Nodes.Data[i]))
        count += g->Nodes.Data[i].Commands.Size;
    return count;
  }

  static const ImGuiAppCommandDesc* AppGraphCommandDefinitionAt(const ImGuiAppGraph* g, int index)
  {
    int seen = 0;
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (!AppNodeIsCommandLayer(n))
        continue;
      for (int c = 0; c < n->Commands.Size; c++, seen++)
        if (seen == index)
          return &n->Commands.Data[c];
    }
    return nullptr;
  }

  static const ImGuiAppCommandDesc* AppGraphFindCommandDefinition(const ImGuiAppGraph* g, const char* name)
  {
    if (name == nullptr || name[0] == 0)
      return nullptr;
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (!AppNodeIsCommandLayer(n))
        continue;
      for (int c = 0; c < n->Commands.Size; c++)
        if (strcmp(n->Commands.Data[c].Name, name) == 0)
          return &n->Commands.Data[c];
    }
    return nullptr;
  }

  static bool AppNodeCommandNameUsed(const ImGuiAppNode* n, const char* name, int ignore_index = -1)
  {
    if (n == nullptr || name == nullptr || name[0] == 0)
      return false;
    for (int i = 0; i < n->Commands.Size; i++)
      if (i != ignore_index && strcmp(n->Commands.Data[i].Name, name) == 0)
        return true;
    return false;
  }

  void AppNodeAddCommand(ImGuiAppNode* n, const char* name)
  {
    IM_ASSERT(n != nullptr);
    ImGuiAppCommandDesc cmd;
    ImStrncpy(cmd.Name, (name && name[0]) ? name : "NewCommand", IM_ARRAYSIZE(cmd.Name));
    n->Commands.push_back(cmd);
  }

  static void AppNodeAddCommandUnique(ImGuiAppNode* n, const char* base_name)
  {
    char name[IM_LABEL_SIZE];
    ImStrncpy(name, (base_name && base_name[0]) ? base_name : "NewCommand", IM_ARRAYSIZE(name));
    if (AppNodeCommandNameUsed(n, name))
      for (int suffix = 2; suffix < 1000; suffix++)
      {
        ImFormatString(name, IM_ARRAYSIZE(name), "%s%d", (base_name && base_name[0]) ? base_name : "NewCommand", suffix);
        if (!AppNodeCommandNameUsed(n, name))
          break;
      }
    AppNodeAddCommand(n, name);
  }

  void AppNodeRemoveCommand(ImGuiAppNode* n, int index)
  {
    IM_ASSERT(n != nullptr);
    if (index >= 0 && index < n->Commands.Size)
      n->Commands.erase(n->Commands.Data + index);
  }

  static void EditAppNodeCommands(ImGuiAppNode* n)
  {
    IM_ASSERT(n != nullptr);

    ImGui::TextDisabled("app commands");
    int remove = -1;
    for (int i = 0; i < n->Commands.Size; i++)
    {
      ImGui::PushID(i);
      AppBlInputText("##cmd", n->Commands.Data[i].Name, IM_ARRAYSIZE(n->Commands.Data[i].Name), ImGui::GetFontSize() * 16.0f);
      if (AppNodeCommandNameUsed(n, n->Commands.Data[i].Name, i))
      {
        ImGui::SameLine();
        ImGui::TextDisabled("duplicate");
      }
      ImGui::SameLine();
      if (AppRowDeleteButton("##del"))
        remove = i;
      ImGui::PopID();
    }
    if (remove >= 0)
      AppNodeRemoveCommand(n, remove);
    if (ImGui::Button("+ Command"))
      AppNodeAddCommandUnique(n, "NewCommand");
  }

  static void EditAppControlCommandChoices(const ImGuiAppGraph* g, ImGuiAppNode* n)
  {
    IM_ASSERT(g != nullptr && n != nullptr);

    const int def_count = AppGraphCommandDefinitionCount(g);
    if (def_count == 0)
    {
      ImGui::TextDisabled("commands: define commands on CommandLayer");
      return;
    }

    ImGui::TextDisabled("emits commands");
    int remove = -1;
    for (int i = 0; i < n->Commands.Size; i++)
    {
      ImGui::PushID(i);
      const char* current = n->Commands.Data[i].Name;
      const bool missing = current[0] != 0 && AppGraphFindCommandDefinition(g, current) == nullptr;
      char preview[IM_LABEL_SIZE + 32];
      if (missing)
        ImFormatString(preview, IM_ARRAYSIZE(preview), "%s (missing)", current);
      else if (current[0] != 0)
        ImFormatString(preview, IM_ARRAYSIZE(preview), "%s", current);
      else
        ImFormatString(preview, IM_ARRAYSIZE(preview), "<command>");

      ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
      if (ImGui::BeginCombo("##cmdref", preview))
      {
        for (int d = 0; d < def_count; d++)
        {
          const ImGuiAppCommandDesc* def = AppGraphCommandDefinitionAt(g, d);
          if (def == nullptr)
            continue;
          const bool selected = strcmp(current, def->Name) == 0;
          const bool taken = AppNodeCommandNameUsed(n, def->Name, i);
          if (taken) ImGui::BeginDisabled();
          if (ImGui::Selectable(def->Name, selected) && !taken)
            ImStrncpy(n->Commands.Data[i].Name, def->Name, IM_ARRAYSIZE(n->Commands.Data[i].Name));
          if (taken) ImGui::EndDisabled();
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      if (AppRowDeleteButton("##del"))
        remove = i;
      ImGui::PopID();
    }
    if (remove >= 0)
      AppNodeRemoveCommand(n, remove);

    const ImGuiAppCommandDesc* first_available = nullptr;
    for (int d = 0; d < def_count; d++)
    {
      const ImGuiAppCommandDesc* def = AppGraphCommandDefinitionAt(g, d);
      if (def != nullptr && !AppNodeCommandNameUsed(n, def->Name))
      {
        first_available = def;
        break;
      }
    }
    if (first_available == nullptr)
      ImGui::BeginDisabled();
    if (ImGui::Button("+ Command") && first_available != nullptr)
      AppNodeAddCommand(n, first_available->Name);
    if (first_available == nullptr)
    {
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextDisabled("all commands selected");
    }
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
        if (k == ImGuiAppEdgeKind_Containment)
        {
          ImGuiAppNode* child = AppGraphFindNode(g, AppGraphPortOwnerId(g, s));
          ImGuiAppNode* parent = AppGraphFindNode(g, AppGraphPortOwnerId(g, d));
          if (child != nullptr && parent != nullptr)
          {
            const ImVec2 preferred = parent->GridPos + ImVec2(280.0f, 0.0f);
            AppGraphPlaceNode(g, child, &preferred);
          }
        }
        changed = true;
        g->LastLinkErr[0] = 0;                                  // a successful create silences any standing toast
      }
      else
      {
        // The reason is otherwise discarded. Drive the demo's fade off the rejection branch, NOT the function's
        // `changed` return (also true on link destroy below), and bump the seq so identical back-to-back
        // rejections still re-fire the toast.
        ImStrncpy(g->LastLinkErr, err, IM_ARRAYSIZE(g->LastLinkErr));
        g->LastLinkErrSeq++;
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
      AppBlInputText("##dst", g->Bindings.Data[i].DstField, IM_ARRAYSIZE(g->Bindings.Data[i].DstField), ImGui::GetFontSize() * 6.0f);
      ImGui::SameLine(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("="); ImGui::SameLine();
      AppBlInputText("##src", g->Bindings.Data[i].SrcField, IM_ARRAYSIZE(g->Bindings.Data[i].SrcField), ImGui::GetFontSize() * 6.0f);
      ImGui::SameLine();
      if (AppRowDeleteButton("##del"))
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

  static const char* AppLayerNodeName(ImGuiAppLayerType t)
  {
    switch (t)
    {
    case ImGuiAppLayerType_Task:    return "TaskLayer";
    case ImGuiAppLayerType_Command: return "CommandLayer";
    case ImGuiAppLayerType_Status:  return "StatusLayer";
    case ImGuiAppLayerType_Window:  return "WindowLayer";
    default:                        return "Layer";
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

  static void AppNodeDataTypeName(const ImGuiAppNode* n, char* out, size_t out_size);   // fwd (defined below)
  static const ImGuiAppNode* AppGraphFindNodeConst(const ImGuiAppGraph* g, int node_id);   // fwd
  static int AppGraphParentOf(const ImGuiAppGraph* g, int child_node_id);                   // fwd

  // One origin vocabulary shared by the canvas title-bar tint, the tree text tint, and the demo legend, so the
  // three surfaces cannot drift. design = default (no push).
  static const ImU32 kAppLiveTint     = IM_COL32(90, 120, 165, 255);   // steel-blue: read-only live mirror
  static const ImU32 kAppPromotedTint = IM_COL32(80, 150, 90, 255);    // green: design matches a live type

  ImU32 AppGraphOriginColor(const ImGuiAppNode* n)   // exposed so the demo legend reads the same constants
  {
    if (n->IsLive)     return kAppLiveTint;
    if (n->IsPromoted) return kAppPromotedTint;
    return 0;
  }

  // Push the origin title-bar tint for a node; returns the push count so the caller can pop in balance. Does NOT
  // touch TitleBarSelected -- the selection cue must stay legible (Theme C depends on it).
  static int AppNodePushOriginStyle(const ImGuiAppNode* n)
  {
    const ImU32 c = AppGraphOriginColor(n);
    if (c == 0) return 0;
    ImNodes::PushColorStyle(ImNodesCol_TitleBar, c);
    ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, c);
    return 2;
  }

  static void AppDrawLayerGroupBox(const ImGuiAppGraph* g, bool show_live)
  {
    bool any = false;
    ImVec2 bb_min(FLT_MAX, FLT_MAX);
    ImVec2 bb_max(-FLT_MAX, -FLT_MAX);
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Kind != ImGuiAppNodeKind_Layer)
        continue;
      if (!show_live && n->IsLive)
        continue;
      const ImVec2 pos = ImNodes::GetNodeScreenSpacePos(n->Id);
      const ImVec2 size = ImNodes::GetNodeDimensions(n->Id);
      bb_min.x = ImMin(bb_min.x, pos.x);
      bb_min.y = ImMin(bb_min.y, pos.y);
      bb_max.x = ImMax(bb_max.x, pos.x + size.x);
      bb_max.y = ImMax(bb_max.y, pos.y + size.y);
      any = true;
    }
    if (!any)
      return;

    const float em = ImGui::GetFontSize();
    const float pad = em * 0.75f;
    const float title_h = ImGui::GetFrameHeight();
    bb_min.x -= pad;
    bb_min.y -= title_h + pad;
    bb_max.x += pad;
    bb_max.y += pad;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fill = ImGui::GetColorU32(ImVec4(0.35f, 0.35f, 0.35f, 0.08f));
    const ImU32 outline = ImGui::GetColorU32(ImVec4(0.70f, 0.70f, 0.70f, 0.55f));
    const ImU32 title_bg = ImGui::GetColorU32(ImVec4(0.20f, 0.20f, 0.20f, 0.75f));
    const ImU32 title_fg = ImGui::GetColorU32(ImGuiCol_Text);
    const float rounding = 4.0f;

    dl->AddRectFilled(bb_min, bb_max, fill, rounding);
    dl->AddRect(bb_min, bb_max, outline, rounding, 0, 1.5f);

    const char* title = "ImGuiAppLayer";
    const ImVec2 text_size = ImGui::CalcTextSize(title);
    const ImVec2 title_min = ImVec2(bb_min.x + pad, bb_min.y);
    const ImVec2 title_max = ImVec2(title_min.x + text_size.x + pad, bb_min.y + title_h);
    dl->AddRectFilled(title_min, title_max, title_bg, 3.0f);
    dl->AddText(ImVec2(title_min.x + pad * 0.5f, title_min.y + (title_h - text_size.y) * 0.5f), title_fg, title);
  }

  static bool AppHandleLayerVerticalDrag(ImGuiAppGraph* g, bool show_live, int* out_anchor_id, ImVec2* out_anchor_pos)
  {
    static int dragging_id = 0;
    static float drag_start_mouse_y = 0.0f;
    static float drag_start_node_y = 0.0f;
    // Clamp edges captured at grab time (neighbor positions move during the drag, so we must not
    // re-sample them). top layer -> top of the node below; bottom layer -> bottom of the node above.
    static float drag_clamp_max_y = FLT_MAX;   // top layer can't move below this
    static float drag_clamp_min_y = -FLT_MAX;  // bottom layer can't move above this

    if (out_anchor_id != nullptr) *out_anchor_id = 0;
    if (out_anchor_pos != nullptr) *out_anchor_pos = ImVec2(0.0f, 0.0f);

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
      dragging_id = 0;

    int hovered = 0;
    if (dragging_id == 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImNodes::IsNodeHovered(&hovered))
    {
      ImGuiAppNode* n = AppGraphFindNodeById(g, hovered);
      if (n != nullptr && n->Kind == ImGuiAppNodeKind_Layer && (show_live || !n->IsLive))
      {
        dragging_id = n->Id;
        drag_start_mouse_y = ImGui::GetIO().MousePos.y;
        drag_start_node_y = ImNodes::GetNodeGridSpacePos(n->Id).y;

        // Capture the immediate neighbor edges by POSITION (no stack-index detection -- that misfired and
        // left a boundary node unclamped, letting the master constrain cascade-push the whole box). The
        // dragged node may approach but not overlap either neighbor, so the constrain never shoves the stack.
        //   - nearest node ABOVE  -> bottom of dragged stays >  that node's bottom  (min_y)
        //   - nearest node BELOW  -> bottom of dragged stays <  that node's top     (max_y)
        // A missing neighbor leaves the bound at +/-inf (top node free upward, bottom node free downward).
        drag_clamp_max_y = FLT_MAX;
        drag_clamp_min_y = -FLT_MAX;
        const float self_h = AppGraphLayerNodeHeight(n->Id);
        float above_y = -FLT_MAX, above_bottom = 0.0f;
        float below_y = FLT_MAX;
        for (int i = 0; i < g->Nodes.Size; i++)
        {
          const ImGuiAppNode* o = &g->Nodes.Data[i];
          if (o->Id == n->Id || o->Kind != ImGuiAppNodeKind_Layer || (!show_live && o->IsLive))
            continue;
          if (o->GridPos.y < drag_start_node_y && o->GridPos.y > above_y)
          {
            above_y = o->GridPos.y;
            above_bottom = o->GridPos.y + AppGraphLayerNodeHeight(o->Id);
          }
          if (o->GridPos.y > drag_start_node_y && o->GridPos.y < below_y)
            below_y = o->GridPos.y;
        }
        // Use the SAME inter-node gap the master constrain enforces (12px). A tighter clamp (e.g. 1px)
        // lets the dragged node enter the constrain's gap zone, so the constrain shoves the whole stack
        // to restore its 12px spacing -- which looked like "the box moves with the drag".
        const float gap = 12.0f;
        if (above_y != -FLT_MAX) drag_clamp_min_y = above_bottom + gap;
        if (below_y != FLT_MAX)  drag_clamp_max_y = below_y - self_h - gap;
      }
    }

    if (dragging_id != 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
    {
      ImGuiAppNode* n = AppGraphFindNodeById(g, dragging_id);
      if (n != nullptr)
      {
        float y = drag_start_node_y + (ImGui::GetIO().MousePos.y - drag_start_mouse_y);
        if (y > drag_clamp_max_y) y = drag_clamp_max_y;
        if (y < drag_clamp_min_y) y = drag_clamp_min_y;
        const ImVec2 pos(kAppGraphX0, y);
        AppGraphConstrainLayerColumn(g, show_live, n->Id, &pos);

        const ImVec2 accepted_pos = n->GridPos;
        if (out_anchor_id != nullptr) *out_anchor_id = n->Id;
        if (out_anchor_pos != nullptr) *out_anchor_pos = accepted_pos;
        return true;
      }
    }
    return false;
  }

  void ShowAppGraphEditor(ImGuiApp* app, ImGuiAppGraph* g, int* selected_node_id, bool show_live)
  {
    IM_ASSERT(g != nullptr);
    IM_UNUSED(app);

    // While hidden, live nodes are not submitted, so imnodes evicts them from its pool. On the hidden->shown
    // transition, re-arm _NeedsPlace so each one is re-placed at its saved GridPos (otherwise imnodes recreates
    // it at the default origin). Single editor instance in the demo, so a function-local latch is enough.
    static bool prev_show_live = true;
    if (show_live && !prev_show_live)
      for (int i = 0; i < g->Nodes.Size; i++)
        if (g->Nodes.Data[i].IsLive)
          g->Nodes.Data[i]._NeedsPlace = true;
    prev_show_live = show_live;

    // Drive the layer vertical drag BEFORE submission so the clamped position lands in THIS frame's
    // placement (the handler sets _NeedsPlace). Running it after EndNodeEditor lagged a frame, letting a
    // fast drag render one unclamped frame before snapping back. Hover state is from last frame -- fine.
    int dragged_layer_id = 0;
    ImVec2 dragged_layer_pos(0.0f, 0.0f);
    AppHandleLayerVerticalDrag(g, show_live, &dragged_layer_id, &dragged_layer_pos);

    ImNodes::BeginNodeEditor();

    for (int i = 0; i < g->Nodes.Size; i++)
    {
      ImGuiAppNode* n = &g->Nodes.Data[i];

      // Hidden live mirror: skip the ENTIRE per-node submission (incl. placement). Merely skipping the body
      // would leave the grid-pos read-back below dereferencing a node imnodes evicted -> hard assert.
      if (!show_live && n->IsLive)
        continue;

      if (n->Kind == ImGuiAppNodeKind_Layer)
        ImNodes::SetNodeDraggable(n->Id, false);

      // Position must be set BEFORE BeginNode (imnodes lays out content at the node origin).
      if (n->_NeedsPlace)
      {
        ImNodes::SetNodeGridSpacePos(n->Id, n->GridPos);
        n->_NeedsPlace = false;
      }

      const int origin_styles = AppNodePushOriginStyle(n);   // balanced pop after EndAppNode

      // Live nodes mirror the running app and are read-only: static (non-renamable) title.
      if (n->IsLive)
        ImGui::BeginAppNode(n->Id, n->Draft.Name[0] ? n->Draft.Name : "(live)");
      else if (n->Kind == ImGuiAppNodeKind_Layer)
        ImGui::BeginAppNode(n->Id, AppLayerNodeName(n->LayerType));
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
      if (n->IsLive)
        ImGui::TextDisabled("live - read-only (mirrors running app)");
      else if (n->IsPromoted)
      {
        char dt[IM_LABEL_SIZE];
        AppNodeDataTypeName(n, dt, IM_ARRAYSIZE(dt));
        ImGui::TextDisabled("promoted -> matches live %s", dt);
      }
      if (n->Kind == ImGuiAppNodeKind_Control)
      {
        if (n->IsBuiltin)
        {
          // The title already names the type; only show the data type when it adds something (differs from the
          // title), with a minimal fallback so the body is never empty (imnodes' empty-body assert).
          if (n->DataTypeName[0] && strcmp(n->DataTypeName, n->Draft.Name) != 0)
            ImGui::TextDisabled("data: %s", n->DataTypeName);
          else if (!n->IsLive && !n->IsPromoted)
            ImGui::TextDisabled("builtin");
        }
        else
        {
          ImGui::EditAppNodeDraftFields(&n->Draft);
          if (!n->IsLive)
            EditAppControlCommandChoices(g, n);
        }

        const int in_link = AppGraphIncomingDataLink(g, n->Id);
        if (in_link >= 0)
          EditAppDataEdgeBindings(g, in_link);
      }
      else if (n->Kind == ImGuiAppNodeKind_Layer)
      {
        // A live layer's title is already its type (e.g. "ImGuiAppTaskLayer") and the origin badge is the body
        // item, so it needs no body line. A design layer shows the type *selector* (interactive, not a dup).
        if (!n->IsLive)
        {
          ImGui::SetNextItemWidth(ImGui::GetFontSize() * 11.0f);
          if (ImGui::BeginCombo("##layertype", AppLayerTypeName(n->LayerType)))
          {
            for (int t = 0; t < ImGuiAppLayerType_COUNT; t++)
            {
              const bool is_current = n->LayerType == t;
              const bool taken = !is_current && AppGraphFindLayerOfType(g, (ImGuiAppLayerType)t, n->Id) != nullptr;
              if (taken) ImGui::BeginDisabled();
              if (ImGui::Selectable(AppLayerTypeName((ImGuiAppLayerType)t), is_current) && !taken)
              {
                n->LayerType = (ImGuiAppLayerType)t;
                AppGraphPlaceNode(g, n, nullptr);
              }
              if (taken) ImGui::EndDisabled();
            }
            ImGui::EndCombo();
          }
        }
        if (n->LayerType == ImGuiAppLayerType_Command)
          EditAppNodeCommands(n);
        ImGui::Dummy(ImVec2(kAppGraphLayerNodeWidth, 1.0f));
      }
      else if (n->Kind == ImGuiAppNodeKind_Window || n->Kind == ImGuiAppNodeKind_Sidebar)
      {
        // Design nodes show their kind so Window vs Sidebar is legible on the canvas; live nodes carry that in
        // the origin badge already. Builtin type only when it differs from the title.
        if (!n->IsLive)
          ImGui::TextDisabled("%s", AppNodeKindName(n->Kind));
        if (n->IsBuiltin && n->TypeName[0] && strcmp(n->TypeName, n->Draft.Name) != 0)
          ImGui::TextDisabled("type: %s", n->TypeName);
      }
      else
      {
        if (!n->IsLive)
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
      for (int s = 0; s < origin_styles; s++) ImNodes::PopColorStyle();   // balanced with AppNodePushOriginStyle
    }

    // Draw links. When live nodes are hidden, skip any link incident on a hidden (live) owner: that attribute
    // was never submitted this frame, so imnodes must not reference it. (Inlined: DrawAppNodeLinks can't resolve
    // owners.)
    for (int li = 0; li < g->Links.Size; li++)
    {
      if (!show_live)
      {
        ImGuiAppNode* oa = nullptr; ImGuiAppNode* ob = nullptr;
        AppGraphFindPort(g, g->Links.Data[li].StartAttr, &oa);
        AppGraphFindPort(g, g->Links.Data[li].EndAttr, &ob);
        if ((oa && oa->IsLive) || (ob && ob->IsLive)) continue;
      }
      ImNodes::Link(g->Links.Data[li].Id, g->Links.Data[li].StartAttr, g->Links.Data[li].EndAttr);
    }

    // Overview minimap (drawn as an overlay inside the editor; must precede EndNodeEditor). Lets the user see
    // and jump to nodes that sit past the visible canvas edge.
    ImNodes::MiniMap(0.2f, ImNodesMiniMapLocation_BottomRight);
    ImNodes::EndNodeEditor();

    // Persist canvas layout for save/load. Skip hidden live nodes: they were not submitted, so a read-back
    // would assert; skipping also correctly retains each hidden node's last-shown GridPos.
    int moved_layer_id = 0;
    ImVec2 moved_layer_pos(0.0f, 0.0f);
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      if (!show_live && g->Nodes.Data[i].IsLive) continue;
      ImGuiAppNode* n = &g->Nodes.Data[i];
      const ImVec2 pos = ImNodes::GetNodeGridSpacePos(n->Id);
      if (n->Kind == ImGuiAppNodeKind_Layer && n->HasGridPos
          && (ImAbs(pos.x - n->GridPos.x) > 0.5f || ImAbs(pos.y - n->GridPos.y) > 0.5f))
      {
        moved_layer_id = n->Id;
        moved_layer_pos = pos;
      }
      n->GridPos = pos;
      // Don't finalize layer nodes here: the tight-packer (AppGraphConstrainLayerColumn, below) sets HasGridPos
      // once their real heights are known. Force-finalizing every frame defeated the packer's deferred-finalize
      // and froze the fallback-height (145px) gaps -- the master column stopped re-packing tight.
      if (n->Kind != ImGuiAppNodeKind_Layer)
        n->HasGridPos = true;
    }
    if (dragged_layer_id != 0)
    {
      moved_layer_id = dragged_layer_id;
      moved_layer_pos = dragged_layer_pos;
    }
    AppGraphConstrainLayerColumn(g, show_live, moved_layer_id, moved_layer_id != 0 ? &moved_layer_pos : nullptr);
    AppDrawLayerGroupBox(g, show_live);

    // In-widget node palette: right-click the canvas to add any node kind. The graph is the app, so every
    // pillar -- layers, windows, sidebars, controls -- is created here, inside the editor itself.
    static ImVec2 add_popup_grid = ImVec2(0.0f, 0.0f);
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
      add_popup_grid = ImGui::GetIO().MousePos - ImGui::GetItemRectMin() - ImNodes::EditorContextGetPanning();
      ImGui::OpenPopup("##AppGraphAdd");
    }
    if (ImGui::BeginPopup("##AppGraphAdd"))
    {
      ImGui::TextDisabled("Add node");
      ImGui::Separator();
      ImGuiAppNode* added = nullptr;
      if (ImGui::MenuItem("Control"))   added = AppGraphAddNode(g, ImGuiAppNodeKind_Control, "NewControl");
      if (ImGui::BeginMenu("Layer"))
      {
        if (ImGui::MenuItem("Task", nullptr, false, !AppGraphHasLayerType(g, ImGuiAppLayerType_Task)))       { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "TaskLayer");    added->LayerType = ImGuiAppLayerType_Task;    }
        if (ImGui::MenuItem("Command", nullptr, false, !AppGraphHasLayerType(g, ImGuiAppLayerType_Command))) { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "CommandLayer"); added->LayerType = ImGuiAppLayerType_Command; }
        if (ImGui::MenuItem("Status", nullptr, false, !AppGraphHasLayerType(g, ImGuiAppLayerType_Status)))   { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "StatusLayer");  added->LayerType = ImGuiAppLayerType_Status;  }
        if (ImGui::MenuItem("Window", nullptr, false, !AppGraphHasLayerType(g, ImGuiAppLayerType_Window)))   { added = AppGraphAddNode(g, ImGuiAppNodeKind_Layer, "WindowLayer");  added->LayerType = ImGuiAppLayerType_Window;  }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Window"))    added = AppGraphAddNode(g, ImGuiAppNodeKind_Window,  "Window");
      if (ImGui::MenuItem("Sidebar"))   added = AppGraphAddNode(g, ImGuiAppNodeKind_Sidebar, "Sidebar");
      if (added != nullptr)
        AppGraphPlaceNode(g, added, &add_popup_grid);
      ImGui::EndPopup();
    }

    char err[128];
    ImGui::CaptureAppGraphLinks(g, err, IM_ARRAYSIZE(err));

    // B1: transient link-rejection toast at the canvas bottom-left. The editor child is the last item after
    // EndNodeEditor; fade alpha over ~2.5s, the timer re-armed whenever LastLinkErrSeq changes (single owner).
    {
      static int   shown_seq = -1;
      static float toast_t0 = 0.0f;
      const float kFade = 2.5f;
      if (g->LastLinkErrSeq != shown_seq) { shown_seq = g->LastLinkErrSeq; toast_t0 = (float)ImGui::GetTime(); }
      const float age = (float)ImGui::GetTime() - toast_t0;
      if (g->LastLinkErr[0] && g->LastLinkErrSeq > 0 && age < kFade)
      {
        const float em = ImGui::GetFontSize();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        char msg[IM_LABEL_SIZE + 32];
        ImFormatString(msg, IM_ARRAYSIZE(msg), "link refused: %s", g->LastLinkErr);
        const ImVec2 ts = ImGui::CalcTextSize(msg);
        const ImU32 col = ImGui::GetColorU32(ImVec4(0.90f, 0.45f, 0.45f, 1.0f - age / kFade));
        ImGui::GetWindowDrawList()->AddText(ImVec2(mn.x + em * 0.6f, mx.y - em * 0.6f - ts.y), col, msg);
      }
    }

    // Cross-view selection sync (Theme C). Valid here: NumSelectedNodes asserts CurrentScope==None, satisfied
    // after EndNodeEditor. Order: dangle guard -> canvas read-back -> tree apply + reveal.
    if (selected_node_id != nullptr)
    {
      static int applied_sel = -1;

      // 1) Dangle guard: a deleted / stripped / reloaded id must not highlight a ghost.
      if (*selected_node_id >= 0 && AppGraphFindNode(g, *selected_node_id) == nullptr)
        *selected_node_id = -1;

      // 2) Canvas -> tree read-back: a single canvas selection writes itself to *selected_node_id (closes the
      //    one-way gap). A multi-select leaves *selected_node_id unchanged (single-select model).
      bool canvas_originated = false;
      if (ImNodes::NumSelectedNodes() == 1)
      {
        int picked = -1;
        ImNodes::GetSelectedNodes(&picked);
        if (picked != *selected_node_id) { *selected_node_id = picked; canvas_originated = true; }
      }

      // 3) Tree -> canvas apply + reveal: an external (tree) change outlines AND pans the node into view. Never
      //    pan on a canvas-originated change (don't yank the viewport on a click).
      if (*selected_node_id != applied_sel)
      {
        // Apply only to a node imnodes actually submitted this frame: a hidden live node (tree still lists it)
        // was evicted from the pool, so SelectNode/MoveToNode on it would assert.
        ImGuiAppNode* tn = (*selected_node_id >= 0) ? AppGraphFindNode(g, *selected_node_id) : nullptr;
        const bool submitted = tn != nullptr && !(!show_live && tn->IsLive);
        if (submitted && !canvas_originated)
        {
          ImNodes::ClearNodeSelection();
          ImNodes::SelectNode(*selected_node_id);   // highlight only -- no auto-pan (it yanked the whole canvas)
        }
        applied_sel = *selected_node_id;
      }
    }
  }

  void AppGraphSelectionBreadcrumb(const ImGuiAppGraph* g, int node_id, char* buf, int buf_size)
  {
    IM_ASSERT(g != nullptr && buf != nullptr && buf_size > 0);

    const ImGuiAppNode* n = node_id >= 0 ? AppGraphFindNodeConst(g, node_id) : nullptr;
    if (n == nullptr) { ImStrncpy(buf, "sel: -", (size_t)buf_size); return; }

    const char* tag = n->IsLive ? "live" : n->IsPromoted ? "promoted" : "design";
    const int parent = AppGraphParentOf(g, n->Id);
    const ImGuiAppNode* pn = parent >= 0 ? AppGraphFindNodeConst(g, parent) : nullptr;
    if (pn != nullptr)
      ImFormatString(buf, buf_size, "sel: %s > %s [%s]", pn->Draft.Name, n->Draft.Name, tag);
    else
      ImFormatString(buf, buf_size, "sel: %s [%s]", n->Draft.Name, tag);
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

  static void AppCommandEnumValue(const ImGuiAppCommandDesc* cmd, char* out, size_t out_size)
  {
    char base[IM_LABEL_SIZE];
    AppSanitizeIdentifier(base, IM_ARRAYSIZE(base), cmd->Name);
    ImFormatString(out, (int)out_size, "AppCommand_%s", base);
  }

  // Just the AppCommand enum folded from every CommandLayer's command list (no app shell).
  static void AppEmitCommandEnum(const ImGuiAppGraph* g, ImGuiTextBuffer* out)
  {
    if (AppGraphCommandDefinitionCount(g) == 0)
      return;

    // Align the '=' column: width = the longest enumerator name carrying an initializer.
    int w = (int)strlen("AppCommand_None");
    if ((int)strlen("AppCommand_Shutdown") > w) w = (int)strlen("AppCommand_Shutdown");
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (!AppNodeIsCommandLayer(n)) continue;
      for (int c = 0; c < n->Commands.Size; c++)
      {
        char enum_value[IM_LABEL_SIZE];
        AppCommandEnumValue(&n->Commands.Data[c], enum_value, IM_ARRAYSIZE(enum_value));
        if ((int)strlen(enum_value) > w) w = (int)strlen(enum_value);
      }
    }

    out->appendf("enum AppCommand\n{\n");
    out->appendf("  %-*s = ImGuiAppCommand_None,\n", w, "AppCommand_None");
    out->appendf("  %-*s = ImGuiAppCommand_Shutdown,\n", w, "AppCommand_Shutdown");
    int offset = 0;
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (!AppNodeIsCommandLayer(n)) continue;
      for (int c = 0; c < n->Commands.Size; c++)
      {
        char enum_value[IM_LABEL_SIZE];
        AppCommandEnumValue(&n->Commands.Data[c], enum_value, IM_ARRAYSIZE(enum_value));
        out->appendf("  %-*s = ImGuiAppCommand_COUNT + %d,\n", w, enum_value, offset++);
      }
    }
    out->appendf("  AppCommand_COUNT\n};\n\n");
  }

  static void AppEmitCommandEnumAndApp(const ImGuiAppGraph* g, ImGuiTextBuffer* out)
  {
    if (AppGraphCommandDefinitionCount(g) == 0)
      return;

    AppEmitCommandEnum(g, out);

    out->appendf("struct ClientApp : ImGuiApp\n{\n");
    out->appendf("  virtual void OnExecuteCommand(ImGuiAppCommand cmd) override\n  {\n");
    out->appendf("    switch ((AppCommand)cmd)\n    {\n");
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (!AppNodeIsCommandLayer(n)) continue;
      for (int c = 0; c < n->Commands.Size; c++)
      {
        char enum_value[IM_LABEL_SIZE];
        AppCommandEnumValue(&n->Commands.Data[c], enum_value, IM_ARRAYSIZE(enum_value));
        out->appendf("    case %s:\n      // TODO: handle %s\n      break;\n", enum_value, n->Commands.Data[c].Name);
      }
    }
    out->appendf("    default:\n      ImGuiApp::OnExecuteCommand(cmd);\n      break;\n");
    out->appendf("    }\n  }\n};\n\n");
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

    // Collect AUTHORED control node ids (stable order = node order, for deterministic output). Live-mirror
    // controls are excluded: they are never emitted, so the topo/health domain matches the codegen domain.
    ImVector<int> ctrl;
    for (int i = 0; i < g->Nodes.Size; i++)
      if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_Control && !g->Nodes.Data[i].IsLive)
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

    out->appendf("  virtual void OnGetCommand(const ImGuiApp* app, ImGuiAppCommand* cmd, const %sData* data, const %sTempData* temp_data", base, base);
    emit_dep_params(out);
    out->appendf(") const override final\n  {\n    IM_UNUSED(app); IM_UNUSED(cmd); IM_UNUSED(data); IM_UNUSED(temp_data);\n");
    if (n->Commands.Size > 0)
    {
      out->appendf("    // TODO: emit one of this control's commands from UI/control state, for example:\n");
      for (int c = 0; c < n->Commands.Size; c++)
      {
        if (AppGraphFindCommandDefinition(g, n->Commands.Data[c].Name) == nullptr)
        {
          out->appendf("    // TODO: command '%s' is not defined on CommandLayer\n", n->Commands.Data[c].Name);
          continue;
        }
        char enum_value[IM_LABEL_SIZE];
        AppCommandEnumValue(&n->Commands.Data[c], enum_value, IM_ARRAYSIZE(enum_value));
        out->appendf("    // *cmd = (ImGuiAppCommand)%s;\n", enum_value);
      }
    }
    else
      out->appendf("    // TODO: set *cmd when this control should request an app command\n");
    out->appendf("  }\n\n");

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
          else if (dst_id[0] && src_id[0])
            out->appendf("    // WARNING: dropped binding %s = %s (type mismatch)\n", dst_id, src_id);
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

  ImGuiID AppGraphSignature(const ImGuiAppGraph* g)
  {
    IM_ASSERT(g != nullptr);

    // Fold the authored (!IsLive) population plus command definitions attached to the CommandLayer. Live mirror
    // node churn still stays out of the hash. char[] hashed as NUL-terminated ImHashStr (ctors zero only byte 0,
    // so ImHashData over the fixed buffer would fold trailing garbage). GridPos/ids/BodyAttrId excluded.
    ImGuiID h = 0;
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->IsLive)
      {
        if (AppNodeIsCommandLayer(n))
          for (int c = 0; c < n->Commands.Size; c++)
            h = ImHashStr(n->Commands.Data[c].Name, 0, h);
        continue;
      }
      h = ImHashData(&n->Kind, sizeof(n->Kind), h);
      h = ImHashStr(n->Draft.Name, 0, h);
      h = ImHashStr(n->TypeName, 0, h);
      h = ImHashStr(n->DataTypeName, 0, h);
      h = ImHashData(&n->IsBuiltin, sizeof(n->IsBuiltin), h);
      h = ImHashData(&n->LayerType, sizeof(n->LayerType), h);
      for (int f = 0; f < n->Draft.PersistFields.Size; f++)
      {
        const ImGuiAppFieldDesc* fd = &n->Draft.PersistFields.Data[f];
        h = ImHashStr(fd->Name, 0, h);
        h = ImHashData(&fd->Type, sizeof(fd->Type), h);
        h = ImHashData(&fd->ArraySize, sizeof(fd->ArraySize), h);
      }
      for (int f = 0; f < n->Draft.TempFields.Size; f++)
      {
        const ImGuiAppFieldDesc* fd = &n->Draft.TempFields.Data[f];
        h = ImHashStr(fd->Name, 0, h);
        h = ImHashData(&fd->Type, sizeof(fd->Type), h);
        h = ImHashData(&fd->ArraySize, sizeof(fd->ArraySize), h);
      }
      for (int c = 0; c < n->Commands.Size; c++)
        h = ImHashStr(n->Commands.Data[c].Name, 0, h);
    }
    for (int li = 0; li < g->Links.Size; li++)
    {
      const ImGuiAppNodeLink* l = &g->Links.Data[li];
      const int oa = AppGraphPortOwnerId(g, l->StartAttr);
      const int ob = AppGraphPortOwnerId(g, l->EndAttr);
      const ImGuiAppNode* na = oa >= 0 ? AppGraphFindNodeConst(g, oa) : nullptr;
      const ImGuiAppNode* nb = ob >= 0 ? AppGraphFindNodeConst(g, ob) : nullptr;
      if (na == nullptr || nb == nullptr || na->IsLive || nb->IsLive) continue;   // authored links only
      h = ImHashData(&l->StartAttr, sizeof(l->StartAttr), h);   // stable port ids: capture connectivity changes
      h = ImHashData(&l->EndAttr, sizeof(l->EndAttr), h);
      h = ImHashData(&l->Kind, sizeof(l->Kind), h);
      for (int bi = 0; bi < g->Bindings.Size; bi++)
      {
        if (g->Bindings.Data[bi].LinkId != l->Id) continue;
        h = ImHashStr(g->Bindings.Data[bi].DstField, 0, h);
        h = ImHashStr(g->Bindings.Data[bi].SrcField, 0, h);
      }
    }
    return h;
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

    // 2) Emit client command enum/app shell, then data structs + control structs for DRAFTED controls in topo
    // order (builtin types already exist).
    AppEmitCommandEnumAndApp(g, out);

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
      if (n->Kind == ImGuiAppNodeKind_Layer && !n->IsLive)
        out->appendf("  PushAppLayer<%s>(app);\n", AppLayerTypeName(n->LayerType));
    }
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Kind == ImGuiAppNodeKind_Window && !n->IsLive)
      {
        char base[IM_LABEL_SIZE]; AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
        out->appendf("  PushAppWindow<%s>(app);\n", base);
      }
    }
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      const ImGuiAppNode* n = &g->Nodes.Data[i];
      if (n->Kind == ImGuiAppNodeKind_Sidebar && !n->IsLive)
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
      {
        out->appendf("  // WARNING: control '%s' cannot be hosted in window '%s' yet (PushWindowControl unimplemented)\n", base, pn->Draft.Name);
        out->appendf("  PushAppControl<%s>(app);\n", base);
      }
      else
        out->appendf("  PushAppControl<%s>(app);\n", base);
    }
    out->appendf("}\n");
  }

  void GenerateAppNodeCode(const ImGuiAppGraph* g, const ImGuiAppNode* n, ImGuiTextBuffer* out)
  {
    IM_ASSERT(g != nullptr && n != nullptr && out != nullptr);

    switch (n->Kind)
    {
    case ImGuiAppNodeKind_Control:
      AppEmitControlWithDeps(g, n, out);            // data structs + control struct with derived deps/bindings
      break;
    case ImGuiAppNodeKind_Layer:
      if (AppNodeIsCommandLayer(n))
        AppEmitCommandEnum(g, out);                 // just the AppCommand enum (the user's example)
      else
        out->appendf("// '%s' is a framework %s layer -- no generated type, just bring-up:\n"
                     "PushAppLayer<%s>(app);\n",
                     n->Draft.Name, AppLayerTypeName(n->LayerType), AppLayerTypeName(n->LayerType));
      break;
    case ImGuiAppNodeKind_Window:
    {
      char base[IM_LABEL_SIZE]; AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
      out->appendf("// Window '%s' bring-up:\nPushAppWindow<%s>(app);\n", n->Draft.Name, base);
      break;
    }
    case ImGuiAppNodeKind_Sidebar:
    {
      char base[IM_LABEL_SIZE]; AppNodeBaseName(n, base, IM_ARRAYSIZE(base));
      out->appendf("// Sidebar '%s' bring-up:\nPushAppSidebar<%s>(app, vp, ImGuiDir_Down, 0.0f, ImGuiWindowFlags_None);\n", n->Draft.Name, base);
      break;
    }
    case ImGuiAppNodeKind_App:
    default:
      GenerateAppGraphCode(g, out);                 // App node == the whole composition
      break;
    }

    if (out->size() == 0)
    {
      if (n->Kind == ImGuiAppNodeKind_Layer && AppNodeIsCommandLayer(n))
        out->appendf("// CommandLayer has no commands yet -- add commands to generate AppCommand.\n");
      else
        out->appendf("// no code generated for this node.\n");
    }
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
      for (int c = 0; c < n->Commands.Size; c++)
        buf.appendf("Command=%s\n", n->Commands.Data[c].Name);
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

  static bool AppNodeKeepsPortKind(const ImGuiAppNode* n, ImGuiAppPortKind kind)
  {
    switch (n->Kind)
    {
    case ImGuiAppNodeKind_App:
      return kind == ImGuiAppPortKind_ChildIn;
    case ImGuiAppNodeKind_Layer:
      return false;
    case ImGuiAppNodeKind_Window:
    case ImGuiAppNodeKind_Sidebar:
      return kind == ImGuiAppPortKind_ChildIn || kind == ImGuiAppPortKind_ChildOut;
    case ImGuiAppNodeKind_Control:
    default:
      return kind == ImGuiAppPortKind_DataIn || kind == ImGuiAppPortKind_DataOut || kind == ImGuiAppPortKind_ChildOut;
    }
  }

  static void AppGraphNormalizeLoadedPorts(ImGuiAppGraph* g)
  {
    for (int i = 0; i < g->Nodes.Size; i++)
    {
      ImGuiAppNode* n = &g->Nodes.Data[i];
      for (int p = n->Ports.Size - 1; p >= 0; p--)
        if (!AppNodeKeepsPortKind(n, n->Ports.Data[p].Kind))
          n->Ports.erase(n->Ports.Data + p);
    }
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
      else if (strncmp(p, "Command=", 8) == 0)   { if (cur) AppNodeAddCommand(cur, p + 8); }
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

    AppGraphNormalizeLoadedPorts(g);

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
  static const char* AppLayerNodeName(ImGuiAppLayerType t);   // fwd (defined above)

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
      if (i < ImGuiAppLayerType_COUNT) ImFormatString(w.Name, IM_ARRAYSIZE(w.Name), "%s", AppLayerNodeName((ImGuiAppLayerType)i));
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

        // New live nodes share the same composition-first placement as authored nodes:
        // layers/app | windows/sidebars | controls. Existing live nodes keep user-dragged positions.
        AppGraphPlaceNode(g, node, nullptr);
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
    IM_UNUSED(app);   // the graph already mirrors the live app, so it is the single outliner source (#16)

    // Grouped outliner: Layers / Sidebars / Controls / App sections, every row selectable + origin-tinted.
    // Window nodes are NOT a separate section -- they nest under the Window-layer node (ImGuiAppWindowLayer
    // owns the windows).
    auto Row = [&](ImGuiAppNode* n)
    {
      const char* tag = n->IsLive ? " [live]" : n->IsPromoted ? " [promoted]" : "";
      char label[IM_LABEL_SIZE + 32];
      ImFormatString(label, IM_ARRAYSIZE(label), "%s%s##n%d", n->Draft.Name[0] ? n->Draft.Name : "(unnamed)", tag, n->Id);
      const bool selrow = selected_node_id && *selected_node_id == n->Id;
      const ImU32 tint = AppGraphOriginColor(n);   // same constants as the canvas tint + legend
      if (tint) ImGui::PushStyleColor(ImGuiCol_Text, tint);
      if (ImGui::Selectable(label, selrow) && selected_node_id) *selected_node_id = n->Id;
      if (tint) ImGui::PopStyleColor();
    };
    auto CountKind = [&](ImGuiAppNodeKind k) { int c = 0; for (int i = 0; i < g->Nodes.Size; i++) if (g->Nodes.Data[i].Kind == k) c++; return c; };

    bool window_layer_exists = false;
    for (int i = 0; i < g->Nodes.Size; i++)
      if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_Layer && g->Nodes.Data[i].LayerType == ImGuiAppLayerType_Window)
      { window_layer_exists = true; break; }

    // Layers -- the Window layer hosts the window nodes as nested children.
    if (ImGui::TreeNodeEx("##layers", ImGuiTreeNodeFlags_DefaultOpen, "Layers (%d)", CountKind(ImGuiAppNodeKind_Layer)))
    {
      for (int i = 0; i < g->Nodes.Size; i++)
      {
        ImGuiAppNode* n = &g->Nodes.Data[i];
        if (n->Kind != ImGuiAppNodeKind_Layer) continue;

        if (n->LayerType == ImGuiAppLayerType_Window)
        {
          const char* tag = n->IsLive ? " [live]" : n->IsPromoted ? " [promoted]" : "";
          char label[IM_LABEL_SIZE + 32];
          ImFormatString(label, IM_ARRAYSIZE(label), "%s%s##n%d", n->Draft.Name[0] ? n->Draft.Name : "(unnamed)", tag, n->Id);
          ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
          if (selected_node_id && *selected_node_id == n->Id) f |= ImGuiTreeNodeFlags_Selected;
          const ImU32 tint = AppGraphOriginColor(n);
          if (tint) ImGui::PushStyleColor(ImGuiCol_Text, tint);
          const bool open = ImGui::TreeNodeEx(label, f);
          if (tint) ImGui::PopStyleColor();
          if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && selected_node_id) *selected_node_id = n->Id;
          if (open)
          {
            for (int wj = 0; wj < g->Nodes.Size; wj++)
              if (g->Nodes.Data[wj].Kind == ImGuiAppNodeKind_Window) Row(&g->Nodes.Data[wj]);
            ImGui::TreePop();
          }
        }
        else
          Row(n);
      }
      ImGui::TreePop();
    }

    // Windows fallback: only when there is no Window layer to host them (so they're never lost).
    if (!window_layer_exists && CountKind(ImGuiAppNodeKind_Window) > 0)
      if (ImGui::TreeNodeEx("##windows", ImGuiTreeNodeFlags_DefaultOpen, "Windows (%d)", CountKind(ImGuiAppNodeKind_Window)))
      { for (int i = 0; i < g->Nodes.Size; i++) if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_Window) Row(&g->Nodes.Data[i]); ImGui::TreePop(); }

    if (ImGui::TreeNodeEx("##sidebars", ImGuiTreeNodeFlags_DefaultOpen, "Sidebars (%d)", CountKind(ImGuiAppNodeKind_Sidebar)))
    { for (int i = 0; i < g->Nodes.Size; i++) if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_Sidebar) Row(&g->Nodes.Data[i]); ImGui::TreePop(); }

    if (ImGui::TreeNodeEx("##controls", ImGuiTreeNodeFlags_DefaultOpen, "Controls (%d)", CountKind(ImGuiAppNodeKind_Control)))
    { for (int i = 0; i < g->Nodes.Size; i++) if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_Control) Row(&g->Nodes.Data[i]); ImGui::TreePop(); }

    if (CountKind(ImGuiAppNodeKind_App) > 0)
      if (ImGui::TreeNodeEx("##app", ImGuiTreeNodeFlags_DefaultOpen, "App (%d)", CountKind(ImGuiAppNodeKind_App)))
      { for (int i = 0; i < g->Nodes.Size; i++) if (g->Nodes.Data[i].Kind == ImGuiAppNodeKind_App) Row(&g->Nodes.Data[i]); ImGui::TreePop(); }
  }
}
