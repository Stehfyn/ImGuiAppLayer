#pragma once

#include "imgui.h"

#include <tuple>
#include <type_traits>
#include <string_view>

#ifndef IM_UNREFERENCED_PARAMETER
  #define IM_UNREFERENCED_PARAMETER(x) (void)(x)
#endif

template <typename T>
inline constexpr const char* FunctionSignature()
{
#ifdef _MSC_VER
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

inline constexpr std::string_view ParseType(std::string_view sv)
{
    size_t start = sv.find_last_of('<');
    size_t end = sv.find_first_of('>');

    if ((sv.length() > end) && (end >= start))
    {
      return sv.substr(start, end - start + 1);
    }

    return sv;
}

inline constexpr std::string_view RemoveNamespaces(std::string_view sv)
{
    size_t last = sv.find_last_of(':');

    if (last == std::string_view::npos)
    {
        return sv;
    }

    return sv.substr(last + 1, sv.size() - (last + 1) - 1);
}

template <typename T>
inline constexpr std::string_view MakeTypeName()
{
    return ParseType(FunctionSignature<T>());
}

inline constexpr ImGuiID ConstantHash(std::string_view sv)
{
    return *sv.data() ? static_cast<ImGuiID>(*sv.data()) + 33 * ConstantHash(sv.data() + 1) : 5381;
}

template <typename T> 
struct ImGuiType
{
    static constexpr ImGuiID ID{ConstantHash(MakeTypeName<T>())};
    static constexpr std::string_view Name{MakeTypeName<T>()};
};

struct ImGuiApp;

enum ImGuiAppCommand;
enum ImGuiAppCommand
{
  ImGuiAppCommand_None = 0,
  ImGuiAppCommand_Shutdown = 1,
};

struct ImGuiAppLayerBase
{
  virtual ~ImGuiAppLayerBase() = default;
  virtual void OnAttach(ImGuiApp*) const = 0;
  virtual void OnDetach(ImGuiApp*) const = 0;
  virtual void OnUpdate(ImGuiApp*, float) const = 0;
  virtual void OnRender(const ImGuiApp*) const = 0;

protected:
  ImGuiAppLayerBase() = default;
};

struct ImGuiAppLayer : ImGuiAppLayerBase
{
  virtual void OnAttach(ImGuiApp*) const override {}
  virtual void OnDetach(ImGuiApp*) const override {}
  virtual void OnUpdate(ImGuiApp*, float) const override {}
  virtual void OnRender(const ImGuiApp*) const override {}
};

struct ImGuiAppTaskLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*) const override final;
  virtual void OnDetach(ImGuiApp*) const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*) const override final;
};

struct ImGuiAppCommandLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*) const override final;
  virtual void OnDetach(ImGuiApp*) const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*) const override final;
};

struct ImGuiAppStatusLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*) const override final;
  virtual void OnDetach(ImGuiApp*) const override final;
  virtual void OnUpdate(ImGuiApp*, float) const override final;
  virtual void OnRender(const ImGuiApp*) const override final;
};

struct ImGuiControlBase
{
  virtual ~ImGuiControlBase() = default;

  virtual void OnInitialize(ImGuiApp*) const = 0;
  virtual void OnShutdown(ImGuiApp*) const = 0;
  virtual void OnGetCommand(const ImGuiApp*, ImGuiAppCommand*) const = 0;
  virtual void OnUpdate(const ImGuiApp*, float) const = 0;
  virtual void OnRender(const ImGuiApp*) const = 0;
protected:
  ImGuiControlBase() = default;
};

struct ImGuiAppBase
{
  virtual void OnExecuteCommand(ImGuiAppCommand cmd) = 0;

  virtual ~ImGuiAppBase() = default;
protected:
  ImGuiAppBase() = default;
};

struct ImGuiApp : ImGuiAppBase
{
  ImGuiStorage Data;
  ImVector<ImGuiControlBase*> Controls;
  ImVector<ImGuiAppLayerBase*> Layers;
  virtual void OnExecuteCommand(ImGuiAppCommand cmd) override;
  bool ShutdownCommanded;
};

template <typename ControlData, typename TempData, typename... DataDependencies>
struct ImGuiControlAdapterBase
{
  virtual ~ImGuiControlAdapterBase() = default;

  virtual void OnInitialize(ImGuiApp* app, ControlData* data, const DataDependencies*...) const = 0;
  virtual void OnShutdown(ImGuiApp* app, ControlData* data, const DataDependencies*...) const = 0;
  virtual void OnGetCommand(const ImGuiApp* app, ImGuiAppCommand* cmd, const ControlData* data, const TempData* temp_data, const DataDependencies*...) const = 0;
  virtual void OnUpdate(const ImGuiApp* app, float dt, ControlData* data, const TempData* temp_data, const TempData* last_temp_data, const DataDependencies*...) const = 0;
  virtual void OnRender(const ImGuiApp* app, const ControlData* data, TempData* temp_data, const DataDependencies*...) const = 0;
protected:
  ImGuiControlAdapterBase() = default;
};

template <typename ControlData, typename TempData, typename... DataDependencies>
struct ImGuiControlAdapter : ImGuiControlBase, ImGuiControlAdapterBase<ControlData, TempData, DataDependencies...>
{
    struct InstanceData
    {
      ControlData Control;
      TempData Temp;
      TempData LastTemp;
    };

    template <typename T> 
    inline T* GetData(const ImGuiApp* app) const
    {
      T* data;

      static_assert((std::is_same_v<T, InstanceData>) || (std::disjunction_v<std::is_same<T, DataDependencies>...>),
        "Type T is not a valid data dependency for this control.");

      data = static_cast<T*>(app->Data.GetVoidPtr(ImGuiType<T>::ID));
      IM_ASSERT(data);

      return static_cast<T*>(data);
    }

    inline std::tuple<DataDependencies*...> GetAllDependencyData(const ImGuiApp* app) const
    {
      return { GetData<DataDependencies>(app)... };
    }

    virtual void OnInitialize(ImGuiApp* app) const override final
    {
      std::apply([=](DataDependencies*... dependencies) { OnInitialize(app, &GetData<InstanceData>(app)->Control, dependencies...); }, GetAllDependencyData(app));
    }

    virtual void OnShutdown(ImGuiApp* app) const override final
    {
      std::apply([=](DataDependencies*... dependencies) { OnShutdown(app, &GetData<InstanceData>(app)->Control, dependencies...); }, GetAllDependencyData(app));
    }

    virtual void OnGetCommand(const ImGuiApp* app, ImGuiAppCommand* cmd) const override final
    {
      std::apply([=](DataDependencies*... dependencies) { OnGetCommand(app, cmd, &GetData<InstanceData>(app)->Control, &GetData<InstanceData>(app)->Temp, dependencies...); }, GetAllDependencyData(app));
    }

    virtual void OnUpdate(const ImGuiApp* app, float dt) const override final
    {
      const TempData* temp_data = &GetData<InstanceData>(app)->Temp;
      std::apply([=](DataDependencies*... dependencies) { OnUpdate(app, dt, &GetData<InstanceData>(app)->Control, temp_data, &GetData<InstanceData>(app)->LastTemp, dependencies...); }, GetAllDependencyData(app));
      GetData<InstanceData>(app)->LastTemp = *temp_data;
    }

    virtual void OnRender(const ImGuiApp* app) const override final
    {
      TempData* temp_data = &GetData<InstanceData>(app)->Temp;
      (*temp_data) = {};
      std::apply([=](DataDependencies*... dependencies) { OnRender(app, &GetData<InstanceData>(app)->Control, temp_data, dependencies...); }, GetAllDependencyData(app));
    }

    virtual void OnInitialize(ImGuiApp* app, ControlData* data, const DataDependencies*...) const override {}
    virtual void OnShutdown(ImGuiApp* app, ControlData* data, const DataDependencies*...) const override {}
    virtual void OnGetCommand(const ImGuiApp* app, ImGuiAppCommand* cmd, const ControlData* data, const TempData* temp_data, const DataDependencies*...) const override {}
    virtual void OnUpdate(const ImGuiApp* app, float dt, ControlData* data, const TempData* temp_data, const TempData* last_temp_data, const DataDependencies*...) const override {}
    virtual void OnRender(const ImGuiApp* app,const ControlData* data, TempData* temp_data, const DataDependencies*...) const override {}
};

template <typename ControlData, typename TempData, typename... DataDependencies>
struct ImGuiControl : ImGuiControlAdapter<ControlData, TempData, DataDependencies...>
{
  using ControlInstanceDataType = ImGuiControlAdapter<ControlData, TempData, DataDependencies...>::InstanceData;
};

namespace ImGui
{
  inline void InitializeApp(ImGuiApp* app);
  inline void ShutdownApp(ImGuiApp* app);
  inline void UpdateApp(ImGuiApp* app);
  inline void RenderApp(const ImGuiApp* app);

  template <typename T>
  inline void PushAppLayer(ImGuiApp* app);
  inline void PopAppLayer(ImGuiApp* app);

  template <typename T>
  inline void PushAppControl(ImGuiApp* app);
  inline void PopAppControl(ImGuiApp* app);

  void ShowAppLayerDemo();
}

namespace ImGui
{
  inline void InitializeApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      //PushAppLayer<ImGuiTask>
      PushAppLayer<ImGuiAppCommandLayer>(app);
      PushAppLayer<ImGuiAppStatusLayer>(app);
  }

  inline void ShutdownApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      while (!app->Controls.empty())
        PopAppControl(app);

      while (!app->Layers.empty())
        PopAppLayer(app);
  }

  inline void UpdateApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      for (auto& layer : app->Layers)
        layer->OnUpdate(app, GetIO().DeltaTime);
  }

  inline void RenderApp(const ImGuiApp* app)
  {
      IM_ASSERT(app);

      for (auto& layer : app->Layers)
        layer->OnRender(app);
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

      app->Layers.back()->OnDetach(app);
      app->Layers.pop_back();
  }

  template <typename T>
  inline void PushAppControl(ImGuiApp* app)
  {
      T* control;
      typename T::ControlInstanceDataType* data;

      IM_ASSERT(app);

      control = IM_NEW(T)();
      IM_ASSERT(control);

      data = IM_NEW(typename T::ControlInstanceDataType)();
      IM_ASSERT(data);

      app->Data.SetVoidPtr(ImGuiType<typename T::ControlInstanceDataType>::ID, data);
      app->Controls.push_back(control);
      app->Controls.back()->OnInitialize(app);
  }

  inline void PopAppControl(ImGuiApp* app)
  {
      IM_ASSERT(app);
      
      if (app->Controls.empty())
        return;

      app->Controls.back()->OnShutdown(app);
      app->Controls.pop_back();
  }
}

