#include "ModulesPane.h"
#include <CheatStengine/MainLayer.h>

#include <IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <CheatStengine/UI/ImGui/Menu.h>
#include <algorithm>
#include <format>

ModulesPane::ModulesPane(State& state)
    : Pane(ICON_MDI_VIEW_GRID " Modules", state)
{
}

void ModulesPane::Draw(double deltaTime)
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    // Handle Ctrl+F to focus on the search bar
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::GetIO().KeyCtrl) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search modules...", &m_SearchQuery);
    ImGui::PopItemWidth();

    std::string queryLower = m_SearchQuery;
    std::ranges::transform(queryLower, queryLower.begin(), ::tolower);

    for (size_t i = 0; i < m_State.Modules.size(); ++i) {
        const MODULEENTRY32& entry = m_State.Modules[i];
        std::string moduleNameLower = entry.szModule;
        std::ranges::transform(moduleNameLower, moduleNameLower.begin(), ::tolower);

        // Skip modules that don't match the search query
        if (!queryLower.empty() && moduleNameLower.find(queryLower) == std::string::npos) {
            continue;
        }

        bool isSelected = (m_SelectedIndex == i);

        if (ImGui::Selectable(std::format("{} (Base: 0x{:X} Size: 0x{:X})", entry.szModule, reinterpret_cast<uintptr_t>(entry.modBaseAddr), entry.modBaseSize).c_str(), isSelected)) {
            m_SelectedIndex = i;
        } else if (isSelected) {
            m_SelectedIndex = -1;
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            m_SelectedIndex = i; // Ensure it stays selected on right-click
            ImGui::OpenPopup(entry.szModule);
        }

        if (ImGui::BeginPopup(entry.szModule)) {
            m_SelectedIndex = i;
            if (ImGui::RoundedMenuItem("Copy Name")) {
                ImGui::SetClipboardText(entry.szModule);
            }
            if (ImGui::RoundedMenuItem("Copy Base Address")) {
                ImGui::SetClipboardText(std::format("0x{:X}", reinterpret_cast<uintptr_t>(entry.modBaseAddr)).c_str());
            }

            if (ImGui::BeginRoundedMenu("Copy Size")) {
                if (ImGui::RoundedMenuItem("As Hex")) {
                    ImGui::SetClipboardText(std::format("0x{:X}", entry.modBaseSize).c_str());
                }
                if (ImGui::RoundedMenuItem("As Decimal")) {
                    ImGui::SetClipboardText(std::to_string(entry.modBaseSize).c_str());
                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }

    ImGui::End();
}