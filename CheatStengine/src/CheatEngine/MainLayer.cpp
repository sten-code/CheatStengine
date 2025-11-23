#include "MainLayer.h"

#include <CheatEngine/Panes/DisassemblyPane.h>
#include <CheatEngine/Panes/MemoryScannerPane.h>
#include <CheatEngine/Panes/ModulesPane.h>
#include <CheatEngine/Panes/StructDissectPane.h>
#include <CheatEngine/Panes/WatchPane.h>
#include <Engine/Core/Application.h>

#include "AddressEvaluator/Error.h"
#include "AddressEvaluator/Evaluator.h"
#include "Icons/MaterialDesignIcons.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

MainLayer::MainLayer(Window& window)
    : Layer("MainLayer")
    , m_Window(window)
    , m_MenuBar(*this, m_ModalManager)
    , m_TitleBar(window)
{
    m_State.Process = Process("RobloxPlayerBeta.exe");
    m_ModalManager.RegisterModal("Open Process", BIND_FN(MainLayer::OpenProcessModal));

    AddPane<ModulesPane>(m_State);
    AddPane<WatchPane>(m_State, m_ModalManager);
    AddPane<MemoryScannerPane>(m_State);
    AddPane<DisassemblyPane>(m_State, m_ModalManager);
    StructDissectPane& structDissectPane = AddPane<StructDissectPane>(m_State, m_ModalManager);

    std::unordered_map<std::string, uintptr_t> identifiers;
    for (const MODULEENTRY32& entry : m_State.Process.GetModuleEntries()) {
        std::string moduleName = entry.szModule;
        std::ranges::transform(moduleName, moduleName.begin(), ::tolower);
        identifiers[moduleName] = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
    }

    AddressEvaluator::Result result = AddressEvaluator::Evaluate("robloxplayerbeta.exe+0x77F3158", identifiers);
    if (!result.IsError()) {
        structDissectPane.AddDissection("RawScheduler", result.Value);
    }
}

void MainLayer::OnAttach()
{
    // GetPane<DisassemblyPane>()->Analyze(0);
}

void MainLayer::OnUpdate(float deltaTime)
{
    if (m_State.Process.IsValid()) {
        m_State.Modules = m_State.Process.GetModuleEntries();
    }
}

void MainLayer::OnImGuiRender()
{
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
    static double lastTime = 0.0;
    double currentTime = ImGui::GetTime();
    double deltaTime = currentTime - lastTime;
    double fps = 1.0 / deltaTime;
    lastTime = currentTime;

    for (const std::unique_ptr<Pane>& pane : m_Panes) {
        if (pane->IsOpen()) {
            pane->Draw();
        }
    }

    ImGui::Begin(ICON_MDI_BUG " Debug");

    ImGui::Text("FPS: %f", fps);
    ImGui::Text("Delta Time: %f ms", deltaTime * 1000.0);

    ImGui::End();

    m_ModalManager.RenderModals();

    // m_ModulesPane.Draw();
    // m_DisassemblyPane.Draw();
    // m_MemoryScannerPane.Draw();
    // m_MemoryViewerPane.Draw();
    // m_WatchPane.Draw();

    // ImGui::Begin("Debug");
    // ImGui::Text("FPS: %.1f", fps);
    // ImGui::Text("Delta Time: %.3f ms", deltaTime * 1000.0);
    // ImGui::End();
}

void MainLayer::OnEvent(Event& event)
{
}

// BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
// {
//     char title[256];
//     GetWindowTextA(hwnd, title, sizeof(title));
//     return TRUE;
// }

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

            ImGui::PushID(i);

            if (ImGui::Selectable(proc.szExeFile)) {
                m_State.Process = Process(proc.th32ProcessID);
                // m_DisassemblyPane.Analyze(m_Process, 0);
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

            ImGui::PushID(i);

            if (ImGui::Selectable(window.Title.c_str())) {
                m_State.Process = Process(window.Pid);
                // m_DisassemblyPane.Analyze(m_Process, 0);
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::PopStyleColor();
}
