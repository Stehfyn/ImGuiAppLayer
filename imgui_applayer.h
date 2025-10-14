#pragma once

#include "imgui.h"

#include <type_traits>

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

struct ImGuiApp
{
  ImVector<ImGuiAppLayerBase*> Layers;
};

namespace ImGui
{
  template <typename T>
  inline void PushAppLayer(ImGuiApp* app);
  inline void PopAppLayer(ImGuiApp* app);
  inline void UpdateApp(ImGuiApp* app);
  inline void RenderApp(const ImGuiApp* app);
}

namespace ImGui
{
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
}

