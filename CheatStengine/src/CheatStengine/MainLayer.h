#pragma once

#include <CheatStengine/Core/KeybindManager.h>
#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Panes/Pane.h>
#include <CheatStengine/Server/McpServer.h>
#include <CheatStengine/Settings/EnumSetting.h>
#include <CheatStengine/Settings/SettingsManager.h>
#include <CheatStengine/Settings/ToggleSetting.h>
#include <CheatStengine/UI/ImGui/IconCache.h>
#include <CheatStengine/UI/MenuBar.h>
#include <CheatStengine/UI/TitleBar.h>
#include <Engine/Core/Layers/Layer.h>

class MainLayer final : public Layer {
public:
    explicit MainLayer(Window& window);
    ~MainLayer() override = default;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float deltaTime) override;
    void OnImGuiRender() override;
    void OnImGuiRenderDock() override;
    void OnEvent(Event& event) override;

    void OpenProcess(uint32_t pid);

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
    void OpenProcessModal(const std::string& name, const std::any& payload);
    void DrawOpenProcessList();
    void DrawOpenWindowList();

    // Draws a 16px row icon (or a blank same-size spacer when texture is 0) and
    // advances the cursor so the following Selectable sits beside it.
    static void DrawRowIcon(uint64_t texture);

    void SettingsModal(const std::string& name, const std::any& payload);

    // Draws the extra MCP tab controls (live URL/token, Open Web, Install).
    void DrawMcpControls();

private:
    Window& m_Window;

    State m_State;
    std::vector<std::unique_ptr<Pane>> m_Panes;

    // Managers
    ModalManager m_ModalManager;
    KeybindManager m_KeybindManager;
    SettingsManager m_SettingsManager;

    MenuBar m_MenuBar;
    TitleBar m_TitleBar;

    // Agent-facing MCP control server. Owns its own listener thread; we only
    // pump its command queue from OnUpdate so engine access stays on this thread.
    Server::McpServer m_McpServer;

    // Small cache of process/window shell icons for the selector. Bound to the
    // D3D11 device in OnAttach and released in OnDetach.
    IconCache m_IconCache;

    std::vector<PROCESSENTRY32> m_ProcessEntries;
    std::vector<Process::Window> m_WindowEntries;

    // Settings
    EnumSetting<ProcessMode>* m_ProcessModeSetting;
    ToggleSetting* m_ServerEnabledSetting = nullptr;
    ToggleSetting* m_RequireAuthSetting = nullptr;

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