#pragma once

#include "imgui.h"

#include <tuple>
#include <type_traits>
#include <string_view>

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

template <typename T>
inline constexpr std::string_view MakeTypeNameMinimal()
{
    return RemoveNamespaces(ParseType(FunctionSignature<T>()));
}

inline constexpr ImGuiID ConstantHash(std::string_view sv)
{
    return *sv.data() ?
        static_cast<ImGuiID>(*sv.data()) + 33 * ConstantHash(sv.data() + 1) :
        5381;
}

template <typename T> 
struct ImGuiType
{
    static constexpr ImGuiID ID{ConstantHash(MakeTypeName<T>())};
    static constexpr std::string_view Name{MakeTypeName<T>()};
    static constexpr std::string_view ShortName{MakeTypeNameMinimal<T>()};
};

struct ImGuiApp;

struct ImGuiAppLayerBase
{
  virtual ~ImGuiAppLayerBase() = default;
  virtual void OnAttach(ImGuiApp*) = 0;
  virtual void OnDetach(ImGuiApp*) = 0;
  virtual void OnUpdate(ImGuiApp*, float) = 0;
  virtual void OnRender(const ImGuiApp*) = 0;

protected:
  ImGuiAppLayerBase() = default;
};

struct ImGuiAppLayer : ImGuiAppLayerBase
{
  virtual void OnAttach(ImGuiApp*) override {}
  virtual void OnDetach(ImGuiApp*) override {}
  virtual void OnUpdate(ImGuiApp*, float) override {}
  virtual void OnRender(const ImGuiApp*) override {}
};

struct ImGuiControlBase
{
  virtual ~ImGuiControlBase() = default;

  virtual void OnInitialize(ImGuiApp*) const = 0;
  virtual void OnShutdown(ImGuiApp*) const = 0;
  virtual void OnGetCommand(const ImGuiApp*, int*) const = 0;
  virtual void OnUpdate(ImGuiApp*, float) const = 0;
  virtual void OnRender(const ImGuiApp*) const = 0;
protected:
  ImGuiControlBase() = default;
};

struct ImGuiAppBase
{
  virtual void OnExecuteCommand(int command) = 0;

  virtual ~ImGuiAppBase() = default;
protected:
  ImGuiAppBase() = default;
};

struct ImGuiApp : ImGuiAppBase
{
  ImGuiStorage Data;
  ImVector<ImGuiControlBase*> Controls;
  ImVector<ImGuiAppLayerBase*> Layers;

  virtual void OnExecuteCommand(int command) override {}
};

template <typename ControlData, typename... DataDependencies>
struct ImGuiControlAdapterBase
{
  virtual ~ImGuiControlAdapterBase() = default;

  virtual void OnInitialize(ImGuiApp* app, ControlData* data, const DataDependencies*...) const = 0;
  virtual void OnShutdown(ImGuiApp* app, ControlData* data, const DataDependencies*...) const = 0;
  virtual void OnGetCommand(const ImGuiApp* app, int* command, ControlData* data, const DataDependencies*...) const = 0;
  virtual void OnUpdate(ImGuiApp* app, float dt, ControlData* data) const = 0;
  virtual void OnRender(const ImGuiApp* app, ControlData* data, const DataDependencies*...) const = 0;
protected:
  ImGuiControlAdapterBase() = default;
};

template <typename ControlData, typename... DataDependencies>
struct ImGuiControlAdapter : ImGuiControlBase, ImGuiControlAdapterBase<ControlData, DataDependencies...>
{
    template <typename T> 
    inline T* GetData(const ImGuiApp* app) const
    {
      T* data;

      //static_assert((std::disjunction_v<std::is_same<T, DataDependencies>...>), "Type T is not a valid data dependency for this control.");

      data = static_cast<T*>(app->Data.GetVoidPtr(ImGuiType<T>::ID));
      IM_ASSERT(data);

      return static_cast<T*>(data);
    }

    inline std::tuple<DataDependencies*...> GetAllDependencyData(const ImGuiApp* app) const
    {
      return std::tuple<DataDependencies*...>
      {
        GetData<DataDependencies>(app)...
      };
    }

    virtual void OnInitialize(ImGuiApp* app) const override final
    {
      std::apply([this, app](DataDependencies*... dependencies)
        {
          this->OnInitialize(app, GetData<ControlData>(const_cast<const ImGuiApp*>(app)), dependencies...);
        },
        GetAllDependencyData(const_cast<const ImGuiApp*>(app)));
    }

    virtual void OnShutdown(ImGuiApp* app) const override final
    {
      std::apply([this, app](DataDependencies*... dependencies)
        {
          this->OnShutdown(app, GetData<ControlData>(const_cast<const ImGuiApp*>(app)), dependencies...);
        },
        GetAllDependencyData(const_cast<const ImGuiApp*>(app)));
    }

    virtual void OnGetCommand(const ImGuiApp* app, int* command) const override final
    {
      std::apply([this, app, command](DataDependencies*... dependencies)
        {
          this->OnGetCommand(app, command, GetData<ControlData>(app), dependencies...);
        },
        GetAllDependencyData(app));
    }

    virtual void OnUpdate(ImGuiApp* app, float dt) const override final
    {
      std::apply([this, app, dt](DataDependencies*... dependencies)
        {
          this->OnUpdate(app, dt, GetData<ControlData>(const_cast<const ImGuiApp*>(app)), dependencies...);
        },
        GetAllDependencyData(const_cast<const ImGuiApp*>(app)));
    }

    virtual void OnRender(const ImGuiApp* app) const override final
    {
      std::apply([this, app](DataDependencies*... dependencies)
        {
          this->OnRender(app, GetData<ControlData>(app), dependencies...);
        },
        GetAllDependencyData(app));
    }

    virtual void OnInitialize(ImGuiApp* app, ControlData* data, const DataDependencies*...) const override {}
    virtual void OnShutdown(ImGuiApp* app, ControlData* data, const DataDependencies*...) const override {}
    virtual void OnGetCommand(const ImGuiApp* app, int* command, ControlData* data, const DataDependencies*...) const override {}
    virtual void OnUpdate(ImGuiApp* app, float dt, ControlData* data) const override {}
    virtual void OnRender(const ImGuiApp* app, ControlData* data, const DataDependencies*...) const override {}
};

template <typename ControlData, typename... DataDependencies>
struct ImGuiControl : ImGuiControlAdapter<ControlData, DataDependencies...>
{
  using ControlDataType = ControlData;
};

struct ImGuiAppCommandLayer : ImGuiAppLayer
{
  virtual void OnUpdate(ImGuiApp* app, float) override final
  {
    int command = 0;

    for (auto& control : app->Controls)
      control->OnGetCommand(app, &command);

    app->OnExecuteCommand(command);
  }
};

struct ImGuiAppStatusLayer : ImGuiAppLayer
{
  virtual void OnUpdate(ImGuiApp* app, float dt) override final
  {
    for (auto& control : app->Controls)
      control->OnUpdate(app, dt);
  }

  virtual void OnRender(const ImGuiApp* app) override final
  {
    for (auto& control : app->Controls)
      control->OnRender(app);
  }
};

namespace ImGui
{
  inline void InitializeApp(ImGuiApp* app);
  template <typename T>
  inline void PushAppLayer(ImGuiApp* app);
  inline void PopAppLayer(ImGuiApp* app);
  inline void UpdateApp(ImGuiApp* app);
  inline void RenderApp(const ImGuiApp* app);
  template <typename T>
  inline void AddAppControl(ImGuiApp* app);
}

namespace ImGui
{
  inline void InitializeApp(ImGuiApp* app)
  {
    IM_ASSERT(app);

    PushAppLayer<ImGuiAppCommandLayer>(app);
    PushAppLayer<ImGuiAppStatusLayer>(app);
  }

  template <typename T>
  inline void PushAppLayer(ImGuiApp* app)
  {
    IM_STATIC_ASSERT((std::is_base_of_v<ImGuiAppLayerBase, T>));
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
  inline void AddAppControl(ImGuiApp* app)
  {
    T* control;
    typename T::ControlDataType* data;

    IM_ASSERT(app);

    control = IM_NEW(T)();
    IM_ASSERT(control);

    data = IM_NEW(typename T::ControlDataType)();
    IM_ASSERT(data);

    app->Data.SetVoidPtr(ImGuiType<typename T::ControlDataType>::ID, data);
    app->Controls.push_back(control);
    app->Controls.back()->OnInitialize(app);
  }
}
