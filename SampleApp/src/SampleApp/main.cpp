#include <Engine.h>
#include <Engine/Core/EntryPoint.h>

#include <imgui.h>

class ExampleLayer final : public Layer {
public:
    void OnImGuiRenderDock() override
    {
        ImGui::Begin("Hello, ImGui!");
        ImGui::Text("This is a sample application using the Engine framework.");
        ImGui::End();
    }
};

class SampleApp final : public Application {
public:
    SampleApp()
        : Application(WindowProps { "Sample App", 1280, 720, false })
    {
        m_LayerStack.PushLayer<ExampleLayer>();
    }

    void Exit() override
    {
        Application::Exit();
    }
};

Application* CreateApplication()
{
    return new SampleApp();
}