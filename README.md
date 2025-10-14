# ImGuiAppLayer

```C++
struct TestLayer : ImGuiAppLayer
{
  virtual void OnAttach(ImGuiApp*) override final
  {
    Counter++;
  }

  virtual void OnRender(const ImGuiApp* app) override final
  {
    ImGui::Begin("App Layer Window");
    ImGui::Text("Hello from App Layer!");
    ImGui::End();
  }

private:
  int Counter;
};

static void ShowImGuiAppLayerDemo()
{
  static ImGuiApp app;

  if (1 == ImGui::GetFrameCount())
    ImGui::PushAppLayer<TestLayer>(&app);

  ImGui::UpdateApp(&app);
  ImGui::RenderApp(&app);
}
```
