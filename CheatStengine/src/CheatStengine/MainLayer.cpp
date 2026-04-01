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
#include <CheatStengine/Settings/KeybindSetting.h>
#include <CheatStengine/Settings/SliderSetting.h>
#include <CheatStengine/Settings/ToggleSetting.h>
#include <Engine/Core/Application.h>

#include <IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_stdlib.h>

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

    SettingsCategory& keybindsSettings = m_SettingsManager.AddCategory("Keybinds");
    std::vector<std::string> keybindCategories = m_KeybindManager.GetCategories();
    for (const std::string& category : keybindCategories) {
        SettingsCategory& subCatagory = keybindsSettings.AddSubCategory(category);
        std::vector<Keybind*> keybindsInCategory = m_KeybindManager.GetKeybindsInCategory(category);
        for (Keybind* keybind : keybindsInCategory) {
            subCatagory.AddSetting<KeybindSetting>(keybind->Name, keybind->Description, keybind->KeyChord, [keybind](KeyChord keyChord) {
                keybind->KeyChord = static_cast<ImGuiKeyChord>(keyChord);
            });
        }
    }

    // Testing shit
    m_State.Process = Process::Create("RobloxPlayerBeta.exe", m_ProcessModeSetting->GetValue());

    // Add some default dissections for Roblox
    if (AddressEvaluator::Result result = AddressEvaluator::Evaluate("robloxplayerbeta.exe+0x795A0D8", *m_State.Process);
        !result.IsError()) {
        structDissectPane.AddDissection("FakeDataModel", result.Value);
    }

    if (AddressEvaluator::Result result = AddressEvaluator::Evaluate("robloxplayerbeta.exe+0x7A08710", *m_State.Process);
        !result.IsError()) {
        structDissectPane.AddDissection("RawScheduler", result.Value);
    }
}

void MainLayer::OnAttach()
{
}

void MainLayer::OnUpdate(float deltaTime)
{
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
