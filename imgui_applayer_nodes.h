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
// [SECTION] Graph topology and persistence (ImGuiAppNodeLink, link capture, save/load)
//-----------------------------------------------------------------------------

// A user-created dataflow edge between two node attributes (output -> input).
struct ImGuiAppNodeLink
{
  int Id;
  int StartAttr;
  int EndAttr;
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
