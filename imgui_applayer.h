#pragma once

/*

Index of this file:
// [SECTION] Header mess
// [SECTION] Forward declarations and basic types
// [SECTION] Compile-time helpers (ImGuiType<>)
// [SECTION] Dear ImGui end-user API functions

// [SECTION] Helpers (ImGuiOnceUponAFrame, ImGuiTextFilter, ImGuiTextBuffer, ImGuiStorage, ImGuiListClipper, Math Operators, ImColor)
// [SECTION] Multi-Select API flags and structures (ImGuiMultiSelectFlags, ImGuiMultiSelectIO, ImGuiSelectionRequest, ImGuiSelectionBasicStorage, ImGuiSelectionExternalStorage)
// [SECTION] Font API (ImFontConfig, ImFontGlyph, ImFontGlyphRangesBuilder, ImFontAtlasFlags, ImFontAtlas, ImFontBaked, ImFont)
// [SECTION] Obsolete functions and types

*/

//-----------------------------------------------------------------------------
// [SECTION] Header mess
//-----------------------------------------------------------------------------

#include "imgui.h" 										    // IMGUI_API, ImGuiID, ImGuiStorage, ImBitArray, ImGuiTextIndex, ImChunkStream
#include "imgui_internal.h"               // ImStrncpy
#include "imapp_config.h"

#include <mutex>                          // std::call_once
#include <tuple>                          // 
#include <type_traits>                    // 
#include <string_view>                    // 

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 26495)          // [Static Analyzer] Variable 'XXX' is uninitialized. Always initialize a member variable (type.6).
#endif

//-----------------------------------------------------------------------------
// [SECTION] Forward declarations and basic types
//-----------------------------------------------------------------------------

// Forward declarations: ImGuiStatic layer
template <typename T> struct ImGuiStatic; //

// Forward declarations: ImGuiApp layer
struct ImGuiApp;                          //
struct ImGuiAppBase;                      //
struct ImGuiAppLayerBase;                 //
struct ImGuiAppLayer;                     //
struct ImGuiAppTaskLayer;                 //
struct ImGuiAppCommandLayer;              //

// Forward declarations: ImGuiAppControl layer
struct ImGuiAppControlBase;                  //
template <typename PersistDataT, typename TempDataT, typename... DataDependencies>                struct ImGuiInterfaceAdapterBase; //
template <typename Base, typename PersistDataT, typename TempDataT, typename... DataDependencies> struct ImGuiInterfaceAdapter;     //
template <typename PersistDataT, typename TempDataT, typename... DataDependencies> struct ImGuiAppControl;            //

// Forward declarations: ImGuiAppWindow layer
struct ImGuiAppWindowBase;                   //

// Forward declarations: ImGuiAppSidebar layer
struct ImGuiAppSidebarBase;                  //

enum ImGuiAppCommand
{
  ImGuiAppCommand_None,
  ImGuiAppCommand_Shutdown,
  ImGuiAppCommand_COUNT,
};

enum ImGuiAppCommandPrivate
{
  ImGuiAppCommandPrivate_ = ImGuiAppCommand_COUNT,
};

//-----------------------------------------------------------------------------
// [SECTION] Compile-time helpers (ImGuiStatic<>, ImGuiType<>)
//-----------------------------------------------------------------------------

#ifndef ImFuncSig
#ifdef _MSC_VER
#define ImFuncSig __FUNCSIG__
#else
#define ImFuncSig __PRETTY_FUNCTION__
#endif 
#endif 

#ifndef IM_LABEL_SIZE
#define IM_LABEL_SIZE 256
#endif 

#ifndef ImParseTypeStart
#ifdef _MSC_VER
#define ImParseTypeStart "::"
#define ImParseTypeStart2 " "
#else
#define ImParseTypeStart '='
#endif 
#endif 

#ifndef ImParseTypeStart2
#define ImParseTypeStart2 ImParseTypeStart
#endif

#ifndef ImParseTypeEnd
#ifdef _MSC_VER
#define ImParseTypeEnd ">"
#else
#define ImParseTypeEnd ']'
#endif 
#endif 


struct ImGuiInterface { char Label[IM_LABEL_SIZE] = {}; ImGuiInterface() = default; virtual ~ImGuiInterface() = default; };

template <typename T>
struct ImGuiStatic
{
  inline static constexpr const char*      _FunctionSignature()                { return ImFuncSig; }
  inline static constexpr bool             _StartsWith(std::string_view sv, std::string_view prefix) { return sv.size() >= prefix.size() && sv.substr(0, prefix.size()) == prefix; }
  inline static constexpr std::string_view _StripTypeKeyword(std::string_view sv)
  {
    return _StartsWith(sv, "struct ") ? sv.substr(7) :
           _StartsWith(sv, "class ")  ? sv.substr(6) :
           _StartsWith(sv, "enum ")   ? sv.substr(5) :
           _StartsWith(sv, "union ")  ? sv.substr(6) : sv;
  }
  inline static constexpr std::string_view _StripDisplayScope(std::string_view sv)
  {
    sv = _StripTypeKeyword(sv);
    size_t scope = sv.rfind("::");
    return _StripTypeKeyword(scope == std::string_view::npos ? sv : sv.substr(scope + 2));
  }
  inline static constexpr std::string_view _ParseType(std::string_view sv)
  {
    constexpr std::string_view clang_marker = "T = ";
    size_t start = sv.find(clang_marker);
    if (start != std::string_view::npos)
    {
      start += clang_marker.size();
      size_t end = sv.find(';', start);
      size_t bracket_end = sv.find(']', start);
      if (end == std::string_view::npos || (bracket_end != std::string_view::npos && bracket_end < end))
        end = bracket_end;
      if (end == std::string_view::npos)
        end = sv.size();
      return _StripDisplayScope(sv.substr(start, end - start));
    }

    constexpr std::string_view msvc_marker = "ImGuiStatic<";
    start = sv.find(msvc_marker);
    if (start != std::string_view::npos)
    {
      start += msvc_marker.size();
      size_t depth = 1;
      for (size_t i = start; i < sv.size(); ++i)
      {
        if (sv[i] == '<')
          ++depth;
        else if (sv[i] == '>' && --depth == 0)
          return _StripDisplayScope(sv.substr(start, i - start));
      }
    }

    size_t end = sv.rfind(ImParseTypeEnd);
    auto sv2 = sv.substr(0, end);
    start = (sv2.rfind(ImParseTypeStart) > sv2.rfind(ImParseTypeStart2)) ? sv2.rfind(ImParseTypeStart) : sv2.rfind(ImParseTypeStart2);
    start = start >= end ? 0 : start;
    return _StripDisplayScope((sv.size() > end) && (end >= (start + 2)) ? sv.substr(start + 2, end - (start + 1)) : sv);
  }
  inline static constexpr ImGuiID          _ConstantHash(std::string_view sv)  { return *sv.data() ? static_cast<ImGuiID>(*sv.data()) + 33 * _ConstantHash(sv.data() + 1) : 5381; }
  inline static           ImGuiID          GetRelativeID()                     { std::call_once(_Initialized, []() { Count = 1; }); return Count++; }
  static constexpr        std::string_view Name                                { _ParseType(_FunctionSignature()) };
  static constexpr        ImGuiID          ID                                  { _ConstantHash(Name) };
  inline static           int              Count;
  inline static           std::once_flag   _Initialized;
};

template <typename T>
using ImGuiType = ImGuiStatic<std::remove_cvref_t<std::remove_pointer_t<T>>>;

template <typename T>
inline static void GenerateLabel(char* label, size_t size) { std::string_view sv = ImGuiType<T>::Name; ImFormatString(label, size, "%.*s", (int)sv.size(), sv.data()); }
template <typename T>
inline static void GenerateUniqueLabel(char* label, size_t size) { std::string_view sv = ImGuiType<T>::Name; ImFormatString(label, size, "%.*s##%d", (int)sv.size(), sv.data(), ImGuiType<T>::GetRelativeID()); }

struct ImGuiColorModEx
{
  ImGuiColorMod ColorMod;
  bool          Active;
};

struct ImGuiStyleModEx
{
  ImGuiStyleMod StyleMod;
  bool          Active;
};

//-----------------------------------------------------------------------------
// [SECTION] Dear ImGui end-user API functions
// (Note that ImGui:: being a namespace, you can add extra ImGui:: functions in your own separate file. Please don't modify imgui source files!)
//-----------------------------------------------------------------------------

namespace ImGui
{
  IMGUI_API void InitializeApp(ImGuiApp* app);
  IMGUI_API void ShutdownApp(ImGuiApp* app);
  IMGUI_API void UpdateApp(ImGuiApp* app);
  IMGUI_API void RenderApp(const ImGuiApp* app);
  IMGUI_API void RegisterAppStorage(ImGuiApp* app, ImGuiID id, void* ptr, void (*destroy)(void*));
  IMGUI_API void ClearAppStorage(ImGuiApp* app);

  template <typename T>
  inline void PushAppSidebar(ImGuiApp* app, ImGuiViewport* vp, ImGuiDir dir, float size = 0.0f, ImGuiWindowFlags flags = 0);
  inline void PopAppSidebar(ImGuiApp* app);

  template <typename T>
  IMGUI_API inline void PushAppLayer(ImGuiApp* app);
  IMGUI_API inline void PopAppLayer(ImGuiApp* app);

  template <typename T>
  IMGUI_API inline void PushAppControl(ImGuiApp* app);
  IMGUI_API inline void PopAppControl(ImGuiApp* app);

  //template <typename T>
  //IMGUI_API inline void PushWindowControl(ImGuiApp* app, ImGuiAppWindowBase* window);

  template <typename T>
  IMGUI_API inline void PushSidebarControl(ImGuiApp* app, ImGuiAppSidebarBase* sidebar);

  IMGUI_API void ShowAppLayerDemo();
}

struct ImGuiAppLayerBase : ImGuiInterface
{
  virtual void OnAttach(ImGuiApp*)        const = 0;
  virtual void OnDetach(ImGuiApp*)        const = 0;
  virtual void OnUpdate(ImGuiApp*, float) const = 0;
  virtual void OnRender(const ImGuiApp*)  const = 0;
};

struct ImGuiAppItemBase : ImGuiInterface
{
  virtual void OnInitialize(ImGuiApp*)                         const = 0;
  virtual void OnShutdown(ImGuiApp*)                           const = 0;
  virtual void OnGetCommand(const ImGuiApp*, ImGuiAppCommand*) const = 0;
  virtual void OnUpdate(const ImGuiApp* app, float dt)         const = 0;
  virtual void OnRender(const ImGuiApp*)                       const = 0;
  virtual void OnStylePush(const ImGuiApp*)                    const = 0;
  virtual void OnStylePop(const ImGuiApp*)                     const = 0;
};

struct ImGuiAppWindowBase : ImGuiAppItemBase
{
	bool Open = true;
  ImGuiWindow* Window = nullptr;
  ImGuiViewport* Viewport = nullptr;
  ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
  ImVector<ImGuiAppControlBase*> Controls;
  ImVector<ImGuiStyleModEx> StyleMods;
  ImVector<ImGuiColorModEx> ColorMods;
};

struct ImGuiAppSidebarBase : ImGuiAppWindowBase
{
  ImGuiDir DockDir = ImGuiDir_None;
  float    Size = 0.0f;
};

struct ImGuiAppControlBase : ImGuiAppItemBase
{
};

template <typename PersistDataT, typename TempDataT, typename... DataDependencies>
struct ImGuiInterfaceAdapterBase : ImGuiInterface
{
  virtual void OnInitialize(ImGuiApp*, PersistDataT*, const DataDependencies*...)                                                const = 0;
  virtual void OnShutdown(ImGuiApp*, PersistDataT*, const DataDependencies*...)                                                  const = 0;
  virtual void OnGetCommand(const ImGuiApp*, ImGuiAppCommand*, const PersistDataT*, const TempDataT*, const DataDependencies*...) const = 0;
  virtual void OnUpdate(float, PersistDataT*, const TempDataT*, const TempDataT*, const DataDependencies*...)                    const = 0;
  virtual void OnRender(const PersistDataT*, TempDataT*, const DataDependencies*...)                                             const = 0;
};

struct ImGuiAppLayer : ImGuiAppLayerBase
{
  virtual void OnAttach(ImGuiApp*)        const override {}
  virtual void OnDetach(ImGuiApp*)        const override {}
  virtual void OnUpdate(ImGuiApp*, float) const override {}
  virtual void OnRender(const ImGuiApp*)  const override {}
};

struct ImGuiAppTaskLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*)        const override final;
  virtual void OnDetach(ImGuiApp*)        const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*)  const override final;
};

struct ImGuiAppCommandLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*)        const override final;
  virtual void OnDetach(ImGuiApp*)        const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*)  const override final;
};

struct ImGuiAppStatusLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*)        const override final;
  virtual void OnDetach(ImGuiApp*)        const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*)  const override final;
};

struct ImGuiAppWindowLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*)        const override final;
  virtual void OnDetach(ImGuiApp*)        const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*)  const override final;
};

struct ImGuiAppBase : ImGuiInterface
{
  virtual void OnExecuteCommand(ImGuiAppCommand cmd) = 0;
  bool ShutdownPending = false;
};

struct ImGuiAppStorageEntry
{
  ImGuiID ID = 0;
  void* Ptr = nullptr;
  void (*Destroy)(void*) = nullptr;
};

struct ImGuiApp : ImGuiAppBase
{
  ImGuiStorage                   Data;
  ImVector<ImGuiAppStorageEntry> StorageEntries;
  ImVector<ImGuiAppLayerBase*>   Layers;
  ImVector<ImGuiAppWindowBase*>  Windows;
  ImVector<ImGuiAppSidebarBase*> Sidebars;
  ImGuiAppPlatform               Platform;
  bool                           Initialized = false;

  bool Initialize(const ImGuiAppConfig* config = nullptr);
  void Shutdown();
  static void DrawFrame(ImGuiApp* app);
  bool IsInitialized() const { return Initialized; }
  virtual void OnExecuteCommand(ImGuiAppCommand cmd) override;
};

template <typename Base, typename PersistDataT, typename TempDataT, typename... DataDependencies>
struct ImGuiInterfaceAdapter : Base, ImGuiInterfaceAdapterBase<PersistDataT, TempDataT, DataDependencies...>
{
    // Instance data for this control, created and stored in ImGuiApp::Data by PushAppControl<>(), and accessible from _InstanceData
    mutable struct InstanceData
    {
      PersistDataT PersistData;
      TempDataT    LastTempData;
      TempDataT    TempData;
    } *_InstanceData;

    // If you assert here, it means that either this control's _InstanceData was not allocated and inserted into ImGuiApp::Data (performed by PushAppControl<>()) or
    // a defined DataDependency was not properly pushed before this control (also performed by PushAppControl<>() for each dependency).
    template <typename T> inline T* GetData(const ImGuiApp* app) const { T* data = static_cast<T*>(app->Data.GetVoidPtr(ImGuiType<T>::ID)); IM_ASSERT(data); return static_cast<T*>(data); }
    inline std::tuple<DataDependencies*...> GetAllDependencyData(const ImGuiApp* app) const { return { GetData<DataDependencies>(app)... }; }

    //
    //
    //
    //
    virtual void OnInitialize(ImGuiApp*, PersistDataT*, const DataDependencies*...) const override {}
    virtual void OnInitialize(ImGuiApp* app) const override final
    {
      _InstanceData = reinterpret_cast<InstanceData*>(GetData<PersistDataT>(app)); // Cache pointer to instance data
      std::apply([=, this](DataDependencies*... dependencies) { OnInitialize(app, &_InstanceData->PersistData, dependencies...); }, GetAllDependencyData(app));
    }

    //
    //
    //
    //
    virtual void OnShutdown(ImGuiApp*, PersistDataT*, const DataDependencies*...) const override {}
    virtual void OnShutdown(ImGuiApp* app) const override final
    {
      std::apply([=, this](DataDependencies*... dependencies) { OnShutdown(app, &_InstanceData->PersistData, dependencies...); }, GetAllDependencyData(app));
    }

    //
    //
    //
    //
    virtual void OnGetCommand(const ImGuiApp*, ImGuiAppCommand*, const PersistDataT*, const TempDataT*, const DataDependencies*...) const override {}
    virtual void OnGetCommand(const ImGuiApp* app, ImGuiAppCommand* cmd) const override final
    {
      std::apply([=, this](DataDependencies*... dependencies) { OnGetCommand(app, cmd, &_InstanceData->PersistData, &_InstanceData->TempData, dependencies...); }, GetAllDependencyData(app));
    }

    //
    //
    //
    //
    virtual void OnUpdate(float, PersistDataT*, const TempDataT*, const TempDataT*, const DataDependencies*...) const override {}
    virtual void OnUpdate(const ImGuiApp* app, float dt) const override final
    {
      std::apply([=, this](DataDependencies*... dependencies) { OnUpdate(dt, &_InstanceData->PersistData, &_InstanceData->TempData, &_InstanceData->LastTempData, dependencies...); }, GetAllDependencyData(app));
      _InstanceData->LastTempData = _InstanceData->TempData;
    }

    //
    //
    //
    //
    virtual void OnRender(const PersistDataT*, TempDataT*, const DataDependencies*...) const override {}
    virtual void OnRender(const ImGuiApp* app) const override final
    {
      _InstanceData->TempData = {};
      std::apply([=, this](DataDependencies*... dependencies) { OnRender(&_InstanceData->PersistData, &_InstanceData->TempData, dependencies...); }, GetAllDependencyData(app));
    }
};

template <typename PersistDataT, typename TempDataT, typename... DataDependencies>
struct ImGuiAppControl : ImGuiInterfaceAdapter<ImGuiAppControlBase, PersistDataT, TempDataT, DataDependencies...>
{
  using ControlDataType = PersistDataT;
  using ControlInstanceDataType = ImGuiInterfaceAdapter<ImGuiAppControlBase, PersistDataT, TempDataT, DataDependencies...>::InstanceData;
};

template <typename T>
struct ImGuiAppWindow : ImGuiAppWindowBase
{
  ImGuiAppWindow() { GenerateUniqueLabel<T>(this->Label, sizeof(this->Label)); }

  virtual void OnInitialize(ImGuiApp*)                         const override {};
  virtual void OnShutdown(ImGuiApp*)                           const override {};
  virtual void OnGetCommand(const ImGuiApp*, ImGuiAppCommand*) const override {};
  virtual void OnUpdate(const ImGuiApp* app, float dt)         const override {};
  virtual void OnRender(const ImGuiApp*)                       const override {};
  virtual void OnStylePush(const ImGuiApp*)                    const override {};
  virtual void OnStylePop(const ImGuiApp*)                     const override {};
};

template <typename T>
struct ImGuiAppSidebar : ImGuiAppSidebarBase
{
  ImGuiAppSidebar() { GenerateUniqueLabel<T>(this->Label, sizeof(this->Label)); }

  virtual void OnInitialize(ImGuiApp*)                         const override {};
  virtual void OnShutdown(ImGuiApp*)                           const override {};
  virtual void OnGetCommand(const ImGuiApp*, ImGuiAppCommand*) const override {};
  virtual void OnUpdate(const ImGuiApp* app, float dt)         const override {};
  virtual void OnRender(const ImGuiApp*)                       const override {};
  virtual void OnStylePush(const ImGuiApp*)                    const override {};
  virtual void OnStylePop(const ImGuiApp*)                     const override {};
};

namespace ImGui
{
  template <typename T>
  inline void DestroyAppStorageValue(void* ptr)
  {
      IM_DELETE(static_cast<T*>(ptr));
  }

  inline void ShutdownAppControls(ImGuiApp* app, ImVector<ImGuiAppControlBase*>& controls)
  {
      IM_ASSERT(app);

      while (!controls.empty())
      {
        ImGuiAppControlBase* control = controls.back();
        controls.pop_back();
        control->OnShutdown(app);
        IM_DELETE(control);
      }
  }

  template <typename T>
  inline void PushAppLayer(ImGuiApp* app)
  {
      //IM_STATIC_ASSERT((std::is_base_of_v<ImGuiAppLayerBase, T>));
      IM_ASSERT(app);

      app->Layers.push_back(IM_NEW(T)());
      app->Layers.back()->OnAttach(app);
  }

  inline void PopAppLayer(ImGuiApp* app)
  {
      IM_ASSERT(app);

      if (app->Layers.empty())
        return;

      ImGuiAppLayerBase* layer = app->Layers.back();
      app->Layers.pop_back();
      layer->OnDetach(app);
      IM_DELETE(layer);
  }

  template <typename T>
  inline void PushAppWindow(ImGuiApp* app)
  {
    IM_ASSERT(app);
    T* window = IM_NEW(T)();
    IM_ASSERT(window);
    app->Windows.push_back(window);
    app->Windows.back()->OnInitialize(app);
  }

  inline void PopAppWindow(ImGuiApp* app)
  {
    IM_ASSERT(app);

    if (app->Windows.empty())
      return;

    ImGuiAppWindowBase* window = app->Windows.back();
    app->Windows.pop_back();
    ShutdownAppControls(app, window->Controls);
    window->OnShutdown(app);
    IM_DELETE(window);
  }

  template <typename T>
  inline void PushAppSidebar(ImGuiApp* app, ImGuiViewport* vp, ImGuiDir dir, float size, ImGuiWindowFlags flags)
  {
    IM_ASSERT(app);
    T* sidebar = IM_NEW(T)();
    IM_ASSERT(sidebar);
    sidebar->Viewport = vp;
    sidebar->DockDir = dir;
    sidebar->Size = size;
    sidebar->Flags = flags;
    app->Sidebars.push_back(sidebar);
    app->Sidebars.back()->OnInitialize(app);
  }

  inline void PopAppSidebar(ImGuiApp* app)
  {
    IM_ASSERT(app);

    if (app->Sidebars.empty())
      return;
    ImGuiAppSidebarBase* sidebar = app->Sidebars.back();
    app->Sidebars.pop_back();
    ShutdownAppControls(app, sidebar->Controls);
    sidebar->OnShutdown(app);
    IM_DELETE(sidebar);
  }

  template <typename T>
  inline void PushAppControl(ImGuiApp* app)
  {
  //    ImGuiID id;
  //    T* control;
  //    typename T::ControlInstanceDataType* instance_data;
  //
  //    IM_ASSERT(app);
  //
  //    // Use the control's data type hash for instance data storage/retrieval (so other controls which depend on instance_data->ControlData may access it)
  //    id = ImGuiType<typename T::ControlDataType>::ID;
  //
  //    // Ensure we are not pushing a duplicate instance of this control data type
  //    instance_data = static_cast<decltype(instance_data)>(app->Data.GetVoidPtr(id));
  //    IM_ASSERT(nullptr == instance_data);
  //
  //    control = IM_NEW(T)();
  //    IM_ASSERT(control);
  //
  //    instance_data = IM_NEW(typename T::ControlInstanceDataType)();
  //    IM_ASSERT(instance_data);
  //
  //    app->Data.SetVoidPtr(id, instance_data);
  //    app->Controls.push_back(control);
  //    app->Controls.back()->OnInitialize(app);
  }

  inline void PopAppControl(ImGuiApp* app)
  {
      IM_ASSERT(app);

      //if (app->Controls.empty())
      //  return;
      //
      //app->Controls.back()->OnShutdown(app);
      //app->Controls.pop_back();
  }

  //template <typename T>
  //IMGUI_API void PushWindowControl(ImGuiApp* app, ImGuiAppWindowBase* window)
  //{
  //    ImGuiID id;
  //    T* control;
  //    typename T::ControlInstanceDataType* instance_data;
  //
  //    IM_ASSERT(app);
  //
  //    // Use the control's data type hash for instance data storage/retrieval (so other controls which depend on instance_data->ControlData may access it)
  //    id = ImGuiType<typename T::ControlDataType>::ID;
  //
  //    // Ensure we are not pushing a duplicate instance of this control data type
  //    instance_data = static_cast<decltype(instance_data)>(app->Data.GetVoidPtr(id));
  //    IM_ASSERT(nullptr == instance_data);
  //
  //    control = IM_NEW(T)();
  //    IM_ASSERT(control);
  //
  //    instance_data = IM_NEW(typename T::ControlInstanceDataType)();
  //    IM_ASSERT(instance_data);
  //
  //    app->Data.SetVoidPtr(id, instance_data);
  //    window->Controls.push_back(control);
  //    window->Controls.back()->OnInitialize(app);
  //}

  template <typename T>
  IMGUI_API inline void PushSidebarControl(ImGuiApp* app, ImGuiAppSidebarBase* sidebar)
  {
      ImGuiID id;
      T* control;
      typename T::ControlInstanceDataType* instance_data;

      IM_ASSERT(app);

      // Use the control's data type hash for instance data storage/retrieval (so other controls which depend on instance_data->ControlData may access it)
      id = ImGuiType<typename T::ControlDataType>::ID;

      // Ensure we are not pushing a duplicate instance of this control data type
      instance_data = static_cast<decltype(instance_data)>(app->Data.GetVoidPtr(id));
      IM_ASSERT(nullptr == instance_data);

      control = IM_NEW(T)();
      IM_ASSERT(control);

      instance_data = IM_NEW(typename T::ControlInstanceDataType)();
      IM_ASSERT(instance_data);

      app->Data.SetVoidPtr(id, instance_data);
      RegisterAppStorage(app, id, instance_data, DestroyAppStorageValue<typename T::ControlInstanceDataType>);
      sidebar->Controls.push_back(control);
      sidebar->Controls.back()->OnInitialize(app);
  }
}

#ifdef _MSC_VER
#pragma warning (pop)
#endif
