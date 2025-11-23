#pragma once

#include <CheatEngine/Core/ModalManager.h>
#include <CheatEngine/Panes/Pane.h>
#include <CheatEngine/UI/MenuBar.h>
#include <CheatEngine/UI/TitleBar.h>

#include <Engine/Core/Layers/Layer.h>

class MainLayer final : public Layer {
public:
    explicit MainLayer(Window& window);
    ~MainLayer() override = default;

    void OnAttach() override;
    void OnUpdate(float deltaTime) override;
    void OnImGuiRender() override;
    void OnImGuiRenderDock() override;
    void OnEvent(Event& event) override;

    void OpenProcessModal(const std::string& name, const std::any& payload);
    void DrawOpenProcessList();
    void DrawOpenWindowList();

    template <typename T>
    [[nodiscard]] T* GetPane();
    template <typename T, typename... Args>
    T& AddPane(Args&&... args);

    std::vector<std::unique_ptr<Pane>>& GetPanes() { return m_Panes; }
    const std::vector<std::unique_ptr<Pane>>& GetPanes() const { return m_Panes; }

private:
    Window& m_Window;

    State m_State;
    ModalManager m_ModalManager;
    std::vector<std::unique_ptr<Pane>> m_Panes;

    MenuBar m_MenuBar;
    TitleBar m_TitleBar;

    std::vector<PROCESSENTRY32> m_ProcessEntries;
    std::vector<Process::Window> m_WindowEntries;

    friend class MenuBar;
    friend class DisassemblyPane;
    friend class MemoryScannerPane;
};

template <typename T>
T* MainLayer::GetPane()
{
    for (const auto& pane : m_Panes) {
        if (T* casted = dynamic_cast<T*>(pane.get())) {
            return casted;
        }
    }
    return nullptr;
}

template <typename T, typename... Args>
T& MainLayer::AddPane(Args&&... args)
{
    static_assert(std::is_base_of_v<Pane, T>, "T must be derived from Pane");
    m_Panes.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    return *static_cast<T*>(m_Panes.back().get());
}