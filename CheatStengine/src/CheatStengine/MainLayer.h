#pragma once

#include <CheatStengine/Core/KeybindManager.h>
#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Panes/Pane.h>
#include <CheatStengine/UI/MenuBar.h>
#include <CheatStengine/UI/TitleBar.h>
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
    [[nodiscard]] T* GetPane() const;
    template <std::derived_from<Pane> T, typename... Args>
    T& AddPane(Args&&... args);

    [[nodiscard]] std::vector<std::unique_ptr<Pane>>& GetPanes() { return m_Panes; }
    [[nodiscard]] const std::vector<std::unique_ptr<Pane>>& GetPanes() const { return m_Panes; }

    [[nodiscard]] ModalManager& GetModalManager() { return m_ModalManager; }
    [[nodiscard]] const ModalManager& GetModalManager() const { return m_ModalManager; }

    [[nodiscard]] KeybindManager& GetKeybindManager() { return m_KeybindManager; }
    [[nodiscard]] const KeybindManager& GetKeybindManager() const { return m_KeybindManager; }

private:
    Window& m_Window;

    State m_State;
    ModalManager m_ModalManager;
    KeybindManager m_KeybindManager;
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
T* MainLayer::GetPane() const
{
    for (const std::unique_ptr<Pane>& pane : m_Panes) {
        if (T* casted = dynamic_cast<T*>(pane.get())) {
            return casted;
        }
    }
    return nullptr;
}

template <std::derived_from<Pane> T, typename... Args>
T& MainLayer::AddPane(Args&&... args)
{
    return static_cast<T&>(*m_Panes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...)));
}