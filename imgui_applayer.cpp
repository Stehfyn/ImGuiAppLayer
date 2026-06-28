#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_applayer.h"
#include "imgui_internal.h"
#include "imguix.h"

#include <ctime>
#include <cstdio>

ImGuiApp::~ImGuiApp()
{
    Shutdown();
}

bool ImGuiApp::OnInitializePlatform(ImGuiAppConfig& config)
{
    IM_ASSERT(config.WindowTitle  != nullptr && "WindowTitle must be set in config passed to Initialize");
    IM_ASSERT(config.WindowWidth  >  0       && "WindowWidth must be set in config passed to Initialize");
    IM_ASSERT(config.WindowHeight >  0       && "WindowHeight must be set in config passed to Initialize");

    if (!ImGuiApp_GetPlatformBackend()->InitPlatform(this, config))
    {
        OnShutdownPlatform();
        return false;
    }
    return true;
}

void ImGuiApp::OnShutdownPlatform()
{
    // ImGuiX shutdown must happen before the backend tears down its platform/renderer
    // resources, since the registered backend Shutdown hook still needs them alive.
    if (ImGuiX::GetCurrentContext() != nullptr)
    {
        ImGuiX::Shutdown();
        ImGuiX::DestroyContext();
    }

    ImGuiApp_GetPlatformBackend()->ShutdownPlatform(this);
    PlatformData = nullptr;
}

int ImGuiApp::Run(int argc, char** argv)
{
    if (!OnInitialize(argc, argv))
    {
        Shutdown();
        return 1;
    }

    return ImGuiApp_GetPlatformBackend()->RunLoop(this);
}

bool ImGuiApp::Initialize(const ImGuiAppConfig* config)
{
    IM_ASSERT(config != nullptr);

    if (Initialized)
        Shutdown();

    ShutdownPending = false;

    ImGuiAppConfig cfg = *config;

    if (!OnInitializePlatform(cfg))
        return false;

    ImGui::InitializeApp(this, &cfg);
    Initialized = true;
    return true;
}

void ImGuiApp::Shutdown()
{
    if (!Initialized && PlatformData == nullptr && Layers.empty() && Windows.empty() && Sidebars.empty() && StorageEntries.empty())
        return;

    ImGui::ShutdownApp(this);
    OnShutdownPlatform();

    Platform        = ImGuiAppPlatform();
    Initialized     = false;
    ShutdownPending = false;
}

void ImGuiApp::OnDrawFrame()
{
    ImGuiAppFrameConfig frame_config;
    frame_config.ClearColor = ClearColor;
    ImGuiX::BeginFrame();
    DrawFrame(this);
    ImGuiX::EndFrame(&frame_config);
}

void ImGuiApp::OnExecuteCommand(ImGuiAppCommand cmd)
{
    if (cmd == ImGuiAppCommand_Shutdown)
        ShutdownPending = true;
}

void ImGuiApp::DrawFrame(ImGuiApp* app)
{
    IM_ASSERT(app != nullptr);
    if (app == nullptr)
        return;

    ImGui::UpdateApp(app);
    ImGui::RenderApp(app);
}

void ImGuiAppTaskLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppTaskLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppTaskLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    IM_UNUSED(app);
    IM_UNUSED(dt);
}

void ImGuiAppTaskLayer::OnRender(const ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppCommandLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppCommandLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppCommandLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    IM_UNUSED(dt);

    ImGuiAppCommand cmd;
    ImBitArray<ImGuiAppCommand_COUNT> arr;
    arr.ClearAllBits();

    //for (auto& control : app->Controls)
    //{
    //    cmd = ImGuiAppCommand_None;
    //    control->OnGetCommand(app, &cmd);
    //    arr.SetBit(static_cast<int>(cmd));
    //}

    for (cmd = ImGuiAppCommand_None; cmd < ImGuiAppCommand_COUNT; cmd = static_cast<ImGuiAppCommand>(1 + cmd))
    {
        if (arr.TestBit(cmd))
            app->OnExecuteCommand(cmd);
    }
}

void ImGuiAppCommandLayer::OnRender(const ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppStatusLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppStatusLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppStatusLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    IM_UNUSED(app);
    IM_UNUSED(dt);
}

void ImGuiAppStatusLayer::OnRender(const ImGuiApp* app) const
{
    IM_ASSERT(app != nullptr);
    if (app == nullptr)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    const char* app_platform = app->Platform.Name != nullptr ? app->Platform.Name : "unknown";
    const char* imgui_platform = io.BackendPlatformName != nullptr ? io.BackendPlatformName : "unknown";
    const char* renderer = io.BackendRendererName != nullptr ? io.BackendRendererName : "unknown";

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 padding = ImVec2(8.0f, 8.0f);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(viewport->WorkPos + ImVec2(padding.x, viewport->WorkSize.y - padding.y), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("AppLayerStatus", nullptr, flags))
    {
        ImGui::Text("App: %s", app_platform);
        ImGui::Text("Platform: %s", imgui_platform);
        ImGui::Text("Renderer: %s", renderer);
    }
    ImGui::End();
}

namespace
{
  static void AppWindowLayerSettingsHandler_ClearAll(ImGuiContext*, ImGuiSettingsHandler*)
  {
  }

  static void AppWindowLayerSettingsHandler_ReadInit(ImGuiContext*, ImGuiSettingsHandler*)
  {
  }

  static void* AppWindowLayerSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
  {
    return nullptr;
  }

  static void AppWindowLayerSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
  {
  }

  static void AppWindowLayerSettingsHandler_ApplyAll(ImGuiContext*, ImGuiSettingsHandler*)
  {
  }

  static void AppWindowLayerSettingsHandler_WriteAll(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer*)
  {
  }
}

void ImGuiAppWindowLayer::OnAttach(ImGuiApp* app) const
{
    IM_UNUSED(app);

    if (ImGui::FindSettingsHandler("AppWindowLayer") != nullptr)
        return;

    // Add .ini handle for persistent AppWindow layer data
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "AppWindowLayer";
    ini_handler.TypeHash = ImHashStr("AppWindowLayer");
    ini_handler.ClearAllFn = AppWindowLayerSettingsHandler_ClearAll;
    ini_handler.ReadInitFn = AppWindowLayerSettingsHandler_ReadInit;
    ini_handler.ReadOpenFn = AppWindowLayerSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = AppWindowLayerSettingsHandler_ReadLine;
    ini_handler.ApplyAllFn = AppWindowLayerSettingsHandler_ApplyAll;
    ini_handler.WriteAllFn = AppWindowLayerSettingsHandler_WriteAll;
    ImGui::AddSettingsHandler(&ini_handler);
}

void ImGuiAppWindowLayer::OnDetach(ImGuiApp* app) const
{
    IM_UNUSED(app);
}

void ImGuiAppWindowLayer::OnUpdate(ImGuiApp* app, float dt) const
{
    for (auto& sidebar : app->Sidebars)
    {
      for (auto& control : sidebar->Controls)
        control->OnUpdate(app, dt);

      sidebar->OnUpdate(app, dt);
    }

    for (auto& window : app->Windows)
    {
      for (auto& control : window->Controls)
        control->OnUpdate(app, dt);

      window->OnUpdate(app, dt);
    }
}

void ImGuiAppWindowLayer::OnRender(const ImGuiApp* app) const
{
    for (auto& sidebar : app->Sidebars)
    {
      sidebar->OnStylePush(app);

      if (sidebar->Window && (sidebar->Flags & ImGuiWindowFlags_AlwaysAutoResize))
      {
        int idx = (ImGuiDir_Up == sidebar->DockDir) || (ImGuiDir_Down == sidebar->DockDir);
        sidebar->Size = sidebar->Window->ContentSizeIdeal[idx] + (2.0f * ImGui::GetStyle().WindowPadding[idx]);
      }

      if (ImGui::BeginViewportSideBar(sidebar->Label, sidebar->Viewport, sidebar->DockDir, sidebar->Size, sidebar->Flags))
      {
        sidebar->Open = true;
        sidebar->Window = ImGui::GetCurrentWindow();
        sidebar->OnRender(app);
      }
      else
      {
        sidebar->Open = false;
			}
      ImGui::End();

      sidebar->OnStylePop(app);
    }

    for (auto& window : app->Windows)
    {
      window->OnStylePush(app);

      if (ImGui::Begin(window->Label, &window->Open, window->Flags))
      {
        window->Window = ImGui::GetCurrentWindow();
        window->OnRender(app);
      }
      ImGui::End();

      window->OnStylePop(app);
    }
}

namespace
{
  // Encourage "pure" design, such that a control is "agnostic to the data passing through it."
  static void RenderTextT(const char* text, ImVec2 text_size, ImVec2 pos, ImVec2 avail, float t_value)
  {
    float offset = avail.x * 0.5f + (t_value * 0.5f * (avail.x - text_size.x));
    ImGui::SetCursorScreenPos(pos + ImVec2(offset, 0.0f) + (text_size * -0.5f));
    ImGui::Text("%s", text);
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

      ImStrncpy(data->type, sv.data(), sv.length());
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
    float timer_secs;
    float t_value;
    float t_direction;
    ImVec4 col;
  };

  struct BreathingControlTempData
  {
    bool hovered;
  };

  struct BreathingControlDemo : ImGuiAppControl<BreathingControlData, BreathingControlTempData, RandomTimeData>
  {
    virtual void OnInitialize(ImGuiApp*, BreathingControlData* data, const RandomTimeData* random_time_data) const override final
    {
      std::string_view sv;

      IM_UNUSED(random_time_data); // Explicitly ignore dependency on initialization (as we explicitly use it on OnUpdate())

      sv = ImGuiType<decltype(this)>::Name;

      ImStrncpy(data->type, sv.data(), sv.length());
      ImFormatString(data->label, sizeof(data->label), "%s", data->type);
    }

    virtual void OnUpdate(float dt, BreathingControlData* data, const BreathingControlTempData* temp_data, const BreathingControlTempData* last_temp_data, const RandomTimeData* random_time_data) const override final
    {
      data->timer_secs = ImMax(0.0f, data->timer_secs - dt);

      if (temp_data->hovered ^ last_temp_data->hovered)
      {
        // If hovered, use RandomTimeData::max_timer_secs to set the timer (otherwise, data->timer_secs will be set to 0.0f)
        data->timer_secs = temp_data->hovered * random_time_data->max_timer_secs;
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

    virtual void OnRender(const BreathingControlData* data, BreathingControlTempData* temp_data, const RandomTimeData* random_time_data) const override final
    {
      IM_UNUSED(random_time_data); // Explicitly ignore dependency on rendering (as we explicitly use it on OnUpdate())

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
}

namespace ImGui
{
  IMGUI_API void RegisterAppStorage(ImGuiApp* app, ImGuiID id, void* ptr, void (*destroy)(void*))
  {
      IM_ASSERT(app);
      IM_ASSERT(id != 0);
      IM_ASSERT(ptr != nullptr);
      IM_ASSERT(destroy != nullptr);

      if (app == nullptr || id == 0 || ptr == nullptr)
        return;

      for (const ImGuiAppStorageEntry& entry : app->StorageEntries)
      {
        IM_ASSERT(entry.ID != id && "ImGuiApp storage entry already registered.");
        if (entry.ID == id)
          return;
      }

      ImGuiAppStorageEntry entry;
      entry.ID = id;
      entry.Ptr = ptr;
      entry.Destroy = destroy;
      app->StorageEntries.push_back(entry);
  }

  IMGUI_API void ClearAppStorage(ImGuiApp* app)
  {
      IM_ASSERT(app);
      if (app == nullptr)
        return;

      for (int i = app->StorageEntries.Size - 1; i >= 0; --i)
      {
        ImGuiAppStorageEntry& entry = app->StorageEntries[i];
        if (entry.Destroy != nullptr && entry.Ptr != nullptr)
          entry.Destroy(entry.Ptr);
      }

      app->StorageEntries.clear();
      app->Data.Clear();
  }

  IMGUI_API void InitializeApp(ImGuiApp* app, const ImGuiAppConfig* config)
  {
      IM_ASSERT(app);
      IM_ASSERT(app->Layers.empty() && "ImGui app already has layers. ShutdownApp() before re-initializing.");
      if (app == nullptr || !app->Layers.empty())
        return;

      if (config != nullptr)
      {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= config->ConfigFlags;
        if (!config->PersistSettings)
          io.IniFilename = nullptr;

        switch (config->Style)
        {
        case ImGuiAppStyle_Light:   ImGui::StyleColorsLight();   break;
        case ImGuiAppStyle_Classic: ImGui::StyleColorsClassic(); break;
        default:                    ImGui::StyleColorsDark();    break;
        }

        app->ClearColor = config->ClearColor;
      }

      app->ShutdownPending = false;
      PushAppLayer<ImGuiAppTaskLayer>(app);
      PushAppLayer<ImGuiAppCommandLayer>(app);
      PushAppLayer<ImGuiAppStatusLayer>(app);
      PushAppLayer<ImGuiAppWindowLayer>(app);
  }

  IMGUI_API void ShutdownApp(ImGuiApp* app)
  {
      IM_ASSERT(app);
      if (app == nullptr)
        return;

      while (!app->Sidebars.empty())
        PopAppSidebar(app);
      while (!app->Windows.empty())
        PopAppWindow(app);
      while (!app->Layers.empty())
        PopAppLayer(app);

      ClearAppStorage(app);
      app->ShutdownPending = false;
  }

  IMGUI_API void UpdateApp(ImGuiApp* app)
  {
      IM_ASSERT(app);

      for (auto& layer : app->Layers)
        layer->OnUpdate(app, GetIO().DeltaTime);
  }

  IMGUI_API void RenderApp(const ImGuiApp* app)
  {
      IM_ASSERT(app);

      for (auto& layer : app->Layers)
        layer->OnRender(app);
  }

  IMGUI_API void ShowAppLayerDemo()
  {
      static ImGuiApp app;

      if (1 == ImGui::GetFrameCount())
      {
        InitializeApp(&app);
        {
          PushAppWindow<BaseWindow>(&app);
          PushAppWindow<BaseWindow>(&app);
          PushAppWindow<BaseWindow>(&app);
          PushAppWindow<BaseWindow>(&app);
          PushAppSidebar<StatusBar>(&app, ImGui::GetMainViewport(), ImGuiDir_Down, 0.0f, ImGuiWindowFlags_AlwaysAutoResize);
        }
        // This demo showcases a button whose color appears to "breathe" while hovered,
        // (i.e. BreathingControlDemo) with a timer that is randomly set each time the
        // button is hovered, which is managed by another control (RandomTimeControlDemo).
        {
          PushAppControl<RandomTimeControlDemo>(&app);
          PushAppControl<BreathingControlDemo>(&app);
        }
      }

      UpdateApp(&app);
      RenderApp(&app);

      if (app.ShutdownPending)
        ShutdownApp(&app);
  }
}
