#include "MainLayer.h"

#include <CheatStengine/AddressEvaluator/Error.h>
#include <CheatStengine/AddressEvaluator/Evaluator.h>
#include <CheatStengine/Panes/DebugPane.h>
#include <CheatStengine/Panes/DisassemblyPane.h>
#include <CheatStengine/Panes/MemoryScannerPane.h>
#include <CheatStengine/Panes/ModulesPane.h>
#include <CheatStengine/Panes/PEViewer.h>
#include <CheatStengine/Panes/PatternScannerPane.h>
#include <CheatStengine/Panes/StructDissectPane.h>
#include <CheatStengine/Panes/WatchPane.h>
#include <CheatStengine/Settings/ComboSetting.h>
#include <CheatStengine/Settings/KernelSettings.h>
#include <CheatStengine/Settings/McpControls.h>
#include <CheatStengine/Settings/KeybindSetting.h>
#include <CheatStengine/Settings/SliderSetting.h>
#include <CheatStengine/Settings/ToggleSetting.h>
#include <Engine/Core/Application.h>
#include <Platform/DX11/DX11Context.h>

#include <IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <Windows.h>

#include <shellapi.h>

#include <algorithm>

DEFINE_ENUM_SETTING(ProcessMode,
    { ProcessMode::WinAPI, "WinAPI" },
    { ProcessMode::Kernel, "Kernel" });

MainLayer::MainLayer(Window& window)
    : Layer("MainLayer")
    , m_Window(window)
    , m_MenuBar(*this)
    , m_TitleBar(window, m_State)
{
    SettingsCategory& general = m_SettingsManager.AddCategory("General");
    m_ProcessModeSetting = general.AddSetting<EnumSetting<ProcessMode>>("Process Mode", "What should be used to interact with processes?", ProcessMode::WinAPI);
    general.AddSetting<KernelSettings>("Kernel", m_Monitor, [this] {
        return m_ProcessModeSetting->GetPendingValue() == ProcessMode::Kernel;
    });

    // MCP control server settings. Auth is off by default so an agent connects
    // with just the URL; flip that on to require the bearer token. The toggle
    // values are synced to the live server each frame in OnUpdate. The rest of
    // the tab (enable/disable, Open Web, Install, live URL/token) is drawn by a
    // McpControls so it can act immediately instead of on Apply.
    SettingsCategory& server = m_SettingsManager.AddCategory("MCP");
    m_ServerEnabledSetting = server.AddSetting<ToggleSetting>("Enable MCP Server",
        "Master switch. When off, the control server is stopped and no agent can connect.", true);
    m_RequireAuthSetting = server.AddSetting<ToggleSetting>("Require Auth Token",
        "When on, MCP requests must carry the bearer token. When off, the loopback bind is the only gate.", false);
    server.AddSetting<McpControls>("MCP Server", [this]() { DrawMcpControls(); });

    // Register modals
    m_ModalManager.RegisterModal("Settings", BIND_FN(MainLayer::SettingsModal));
    m_ModalManager.RegisterModal("Open Process", BIND_FN(MainLayer::OpenProcessModal));

    // Keybinds
    m_KeybindManager.RegisterKeybind("Settings", "Opens the settings menu", "Global", ImGuiMod_Ctrl | ImGuiKey_Comma, [this]() {
        m_ModalManager.OpenModal("Settings");
    });

    // Create panes
    AddPane<ModulesPane>(m_State);
    AddPane<WatchPane>(m_State, m_ModalManager);
    AddPane<MemoryScannerPane>(m_State);
    AddPane<DisassemblyPane>(m_State, m_ModalManager, m_KeybindManager);
    AddPane<PatternScannerPane>(m_State, *this);
    AddPane<DebugPane>(m_State);
    AddPane<PEViewer>(m_State);
    StructDissectPane& structDissectPane = AddPane<StructDissectPane>(m_State, m_ModalManager, m_KeybindManager);

    // Generate settings for keybinds
    SettingsCategory& keybindsSettings = m_SettingsManager.AddCategory("Keybinds");
    std::vector<std::string> keybindCategories = m_KeybindManager.GetCategories();
    for (const std::string& category : keybindCategories) {
        SettingsCategory& subCategory = keybindsSettings.AddSubCategory(category);
        std::vector<Keybind*> keybindsInCategory = m_KeybindManager.GetKeybindsInCategory(category);
        for (Keybind* keybind : keybindsInCategory) {
            subCategory.AddSetting<KeybindSetting>(keybind->Name, keybind->Description, static_cast<KeyChord>(keybind->KeyChord), [keybind](KeyChord keyChord) {
                keybind->KeyChord = static_cast<ImGuiKeyChord>(keyChord);
            });
        }
    }

    // Testing shit
    m_State.Process = Process::Create("RobloxPlayerBeta.exe", m_ProcessModeSetting->GetValue());

    // Add some default dissections for Roblox
    if (AddressEvaluator::Result result = AddressEvaluator::Evaluate("robloxplayerbeta.exe+0x834A988", *m_State.Process);
        !result.IsError()) {
        structDissectPane.AddDissection("FakeDataModel", result.Value);
    }

    if (AddressEvaluator::Result result = AddressEvaluator::Evaluate("robloxplayerbeta.exe+0x8428188", *m_State.Process);
        !result.IsError()) {
        structDissectPane.AddDissection("RawScheduler", result.Value);
    }
}

void MainLayer::OnAttach()
{
    // Bind the icon cache to the live D3D11 device so the process selector can
    // show executable icons. GetGraphicsContext() is the concrete DX11 backend.
    auto& context = static_cast<DX11Context&>(m_Window.GetGraphicsContext());
    m_IconCache.Initialize(context.GetDevice());

    // Bring up the MCP control server so an external agent can drive the engine.
    // Binds to loopback with a per-run bearer token (both logged on startup).
    m_McpServer.Start(m_State);
}

void MainLayer::OnDetach()
{
    m_Monitor.Stop();
    m_McpServer.Stop();

    // Release icon textures while the device is still alive.
    m_IconCache.Shutdown();
}

void MainLayer::OnUpdate(float deltaTime)
{
    if (m_ProcessModeSetting->GetValue() == ProcessMode::Kernel) {
        m_Monitor.Start();
    } else {
        m_Monitor.Stop();
    }

    // Run any tool work the server queued from its listener thread. Must happen
    // on this (main) thread before we touch State below.
    m_McpServer.DrainCommands();

    // Keep the server's auth requirement in step with the settings toggle.
    if (m_RequireAuthSetting) {
        m_McpServer.SetAuthRequired(m_RequireAuthSetting->GetValue());
    }

    // Start or stop the control server to match the master enable toggle.
    if (m_ServerEnabledSetting) {
        bool wantRunning = m_ServerEnabledSetting->GetValue();
        if (wantRunning && !m_McpServer.IsRunning()) {
            m_McpServer.Start(m_State);
        } else if (!wantRunning && m_McpServer.IsRunning()) {
            m_McpServer.Stop();
        }
    }

    if (m_State.Process->IsValid()) {
        m_State.Modules = m_State.Process->GetModuleEntries();
    }
}

void MainLayer::OnImGuiRender()
{
    m_KeybindManager.Update();
    bool isMaximized = m_Window.IsMaximized();
    ImGui::SetCursorPos(ImVec2 { 40.0f, isMaximized ? 9.0f : 3.0f });

    m_MenuBar.Draw();
    bool menuBarHovered = ImGui::IsItemHovered();
    bool titleBarHovered = m_TitleBar.Draw();
    if (menuBarHovered) {
        m_Window.SetIsTitleBarHovered(false);
    } else {
        m_Window.SetIsTitleBarHovered(titleBarHovered);
    }
}

void MainLayer::OnImGuiRenderDock()
{
    ImGuiIO& io = ImGui::GetIO();
    double deltaTime = io.DeltaTime;

    for (const std::unique_ptr<Pane>& pane : m_Panes) {
        if (pane->IsOpen()) {
            if (pane->IsForcedFocus()) {
                ImGui::SetNextWindowFocus();
                pane->UnforceFocus();
            }
            pane->Draw(deltaTime);
        }
    }

    m_ModalManager.RenderModals();
}

void MainLayer::OnEvent(Event& event)
{
}

void MainLayer::OpenProcess(uint32_t pid)
{
    m_State.Process = Process::Create(pid, m_ProcessModeSetting->GetValue());
    if (m_State.Process->IsValid()) {
        m_State.Modules = m_State.Process->GetModuleEntries();
    }
}

void MainLayer::DrawRowIcon(uint64_t texture)
{
    // Keep the glyph aligned with the row text and vertically centred against a
    // one-line Selectable. A 0 texture still consumes the same width so labels
    // line up whether or not an icon was available.
    constexpr float iconSize = 16.0f;
    float offset = (ImGui::GetTextLineHeight() - iconSize) * 0.5f;
    if (offset > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
    }

    if (texture) {
        ImGui::Image(static_cast<ImTextureID>(texture), ImVec2 { iconSize, iconSize });
    } else {
        ImGui::Dummy(ImVec2 { iconSize, iconSize });
    }

    if (offset > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - offset);
    }
    ImGui::SameLine(0.0f, 6.0f);
}

void MainLayer::OpenProcessModal(const std::string& name, const std::any& payload)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2 { 0.5f, 0.5f });
    ImGui::SetNextWindowSize(ImVec2 { 400, 300 }, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(name.c_str(), nullptr)) {
        if (ImGui::IsWindowAppearing()) {
            m_ProcessEntries = Process::EnumerateProcesses();
            m_WindowEntries = Process::EnumerateApplications();
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::BeginTabBar("##ProcessTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Processes")) {
                DrawOpenProcessList();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Windows")) {
                DrawOpenWindowList();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::EndPopup();
    }
}

void MainLayer::DrawOpenProcessList()
{
    static std::string searchQuery;
    ImGui::PushItemWidth(-1);
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::InputTextWithHint("##Search", "Search processes...", &searchQuery);
    ImGui::PopItemWidth();

    std::string queryLower = searchQuery;
    std::ranges::transform(queryLower, queryLower.begin(), ::tolower);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    if (ImGui::BeginListBox("##ProcessList", ImVec2 { -1, -1 })) {
        for (size_t i = 0; i < m_ProcessEntries.size(); i++) {
            const PROCESSENTRY32& proc = m_ProcessEntries[i];

            std::string processNameLower = proc.szExeFile;
            std::ranges::transform(processNameLower, processNameLower.begin(), ::tolower);

            if (!queryLower.empty() && processNameLower.find(queryLower) == std::string::npos) {
                continue;
            }

            ImGui::PushID(static_cast<int>(i));

            // Row icon: the process's own executable icon, falling back to the
            // generic app glyph. Drawn inline so the Selectable still owns the row.
            uint64_t icon = m_IconCache.GetProcessIcon(proc.th32ProcessID);
            if (!icon) {
                icon = m_IconCache.GetFallbackIcon();
            }
            DrawRowIcon(icon);

            if (ImGui::Selectable(proc.szExeFile)) {
                OpenProcess(proc.th32ProcessID);
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::PopStyleColor();
}

void MainLayer::DrawOpenWindowList()
{
    static std::string searchQuery;
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search windows...", &searchQuery);
    ImGui::PopItemWidth();

    std::string queryLower = searchQuery;
    std::ranges::transform(queryLower, queryLower.begin(), ::tolower);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    if (ImGui::BeginListBox("##WindowList", ImVec2 { -1, -1 })) {
        for (size_t i = 0; i < m_WindowEntries.size(); i++) {
            const Process::Window& window = m_WindowEntries[i];

            std::string windowTitleLower = window.Title;
            std::ranges::transform(windowTitleLower, windowTitleLower.begin(), ::tolower);

            if (!queryLower.empty() && windowTitleLower.find(queryLower) == std::string::npos) {
                continue;
            }

            ImGui::PushID(static_cast<int>(i));

            // Row icon: the owning process's icon, resolved via its window handle.
            uint64_t icon = m_IconCache.GetWindowIcon(window.Handle);
            if (!icon) {
                icon = m_IconCache.GetFallbackIcon();
            }
            DrawRowIcon(icon);

            if (ImGui::Selectable(window.Title.c_str())) {
                OpenProcess(window.Pid);
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::PopStyleColor();
}

void MainLayer::SettingsModal(const std::string& name, const std::any& payload)
{
    m_SettingsManager.DrawSettingsPopup(name);
}

void MainLayer::DrawMcpControls()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_McpServer.IsRunning()) {
        ImGui::TextDisabled("Server stopped. Enable it above to accept connections.");
        return;
    }

    const std::string& url = m_McpServer.GetUrl();
    const std::string& token = m_McpServer.GetToken();

    ImGui::TextColored(ImVec4 { 0.5f, 0.73f, 1.0f, 1.0f }, "%s", url.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##url")) {
        ImGui::SetClipboardText(url.c_str());
    }

    ImGui::TextDisabled("Endpoint");
    ImGui::SameLine();
    std::string endpoint = url + "/mcp";
    ImGui::TextUnformatted(endpoint.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##endpoint")) {
        ImGui::SetClipboardText(endpoint.c_str());
    }

    ImGui::TextDisabled("Token");
    ImGui::SameLine();
    ImGui::TextUnformatted(token.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##token")) {
        ImGui::SetClipboardText(token.c_str());
    }

    ImGui::Spacing();

    if (ImGui::Button("Open Web UI")) {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Configuration")) {
        std::string configUrl = url + "/config.html";
        ShellExecuteA(nullptr, "open", configUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reinstall Client Configs")) {
        m_McpServer.ReinstallClients();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rewrite the MCP entries for Claude Code, Cursor, Claude Desktop, Codex and the skill.");
    }
}
