#include "ModulesPane.h"
#include <CheatEngine/MainLayer.h>

#include <imgui.h>

#include <CheatEngine/Icons/MaterialDesignIcons.h>
#include <algorithm>
#include <format>

ModulesPane::ModulesPane(State& state)
    : Pane(ICON_MDI_VIEW_GRID " Modules", state)
{
}

// void ModulesPane::SetModules(const std::vector<MODULEENTRY32>& modules)
// {
//     m_Modules = modules;
//     std::ranges::sort(m_Modules, [](const MODULEENTRY32& a, const MODULEENTRY32& b) {
//         std::string aName = a.szModule;
//         std::string bName = b.szModule;
//         std::ranges::transform(aName, aName.begin(), ::tolower);
//         std::ranges::transform(bName, bName.begin(), ::tolower);
//         return aName < bName;
//     });
// }

void ModulesPane::Draw()
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    // Handle Ctrl+F to focus on the search bar
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::GetIO().KeyCtrl) {
        ImGui::SetKeyboardFocusHere();
    }

    static char searchQuery[256] = "";
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search modules...", searchQuery, sizeof(searchQuery));
    ImGui::PopItemWidth();

    std::string queryLower = searchQuery;
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

        if (ImGui::BeginPopupContextItem(entry.szModule)) {
            m_SelectedIndex = i; // Ensure it stays selected on right-click

            if (ImGui::MenuItem("Copy Name")) {
                ImGui::SetClipboardText(entry.szModule);
            }
            if (ImGui::BeginMenu("Copy Base Address")) {
                if (ImGui::MenuItem("As Hex")) {
                    ImGui::SetClipboardText(std::format("0x{:X}", reinterpret_cast<uintptr_t>(entry.modBaseAddr)).c_str());
                }
                if (ImGui::MenuItem("As Decimal")) {
                    ImGui::SetClipboardText(std::to_string(reinterpret_cast<uintptr_t>(entry.modBaseAddr)).c_str());
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Copy Size")) {
                if (ImGui::MenuItem("As Hex")) {
                    ImGui::SetClipboardText(std::format("0x{:X}", entry.modBaseSize).c_str());
                }
                if (ImGui::MenuItem("As Decimal")) {
                    ImGui::SetClipboardText(std::to_string(entry.modBaseSize).c_str());
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}