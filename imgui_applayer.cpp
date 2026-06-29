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
    // App-level controls (not attached to a window/sidebar) render their own windows.
    for (auto& control : app->Controls)
      control->OnUpdate(app, dt);

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
        const bool horizontal = (ImGuiDir_Left == sidebar->DockDir) || (ImGuiDir_Right == sidebar->DockDir);
        const int  idx = horizontal ? 0 : 1;
        float ideal = sidebar->Window->ContentSizeIdeal[idx] + (2.0f * ImGui::GetStyle().WindowPadding[idx]);

        // Left/Right sidebars auto-size their width, but wrapped or auto-width content reports a
        // collapsed ideal width (it wraps to the current bar width), making the bar far too thin.
        // Clamp to a sensible minimum, in text units so it scales with the font.
        if (horizontal)
          ideal = ImMax(ideal, ImGui::GetFontSize() * 8.0f);

        sidebar->Size = ideal;
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

      // Controls render their own windows; submit them outside the sidebar's Begin/End.
      for (auto& control : sidebar->Controls)
        control->OnRender(app);
    }

    for (auto& window : app->Windows)
    {
      window->OnStylePush(app);

      if (window->HasInitialPlacement)
      {
        if (window->InitialSize.x > 0.0f && window->InitialSize.y > 0.0f)
          ImGui::SetNextWindowSize(window->InitialSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(window->InitialPos, ImGuiCond_FirstUseEver);
      }

      if (ImGui::Begin(window->Label, &window->Open, window->Flags))
      {
        window->Window = ImGui::GetCurrentWindow();
        window->OnRender(app);

        // Window-hosted controls render INSIDE the host window (they submit child regions, not their own
        // Begin/End). This is what distinguishes a hosted control from an app-level control (which owns a window).
        for (auto& control : window->Controls)
          control->OnRender(app);
      }
      ImGui::End();

      window->OnStylePop(app);
    }

    for (auto& control : app->Controls)
      control->OnRender(app);
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
      ShutdownAppControls(app, app->Controls);
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
}
