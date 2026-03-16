#include "PatternScannerPane.h"

#include "CheatStengine/Utils.h"

#include <CheatStengine/Icons/MaterialDesignIcons.h>

#include <imgui.h>

PatternScannerPane::PatternScannerPane(State& state)
    : Pane(ICON_MDI_REGEX " Pattern Scanner", state)
{
    m_State.PatternScanResults.push_back({"FF FF FF", {0x0}});
}

void PatternScannerPane::Draw()
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    if (ImGui::BeginTabBar("DissectionsTabBar")) {
        for (size_t i = 0; i < m_State.PatternScanResults.size(); i++) {
            PatternScanResult& result = m_State.PatternScanResults[i];
            bool open = true;
            if (ImGui::BeginTabItem(result.Pattern.c_str(), &open)) {
                DrawPatternScanResults(result);
                ImGui::EndTabItem();
            }
            if (!open) {
                m_State.PatternScanResults.erase(m_State.PatternScanResults.begin() + i);
                i--;
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void PatternScannerPane::DrawPatternScanResults(PatternScanResult& result)
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2 { 3, 2 });
    if (ImGui::BeginTable("PatternScannerResults", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Instruction");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(result.Results.size());
        while (clipper.Step()) {
            for (size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                uintptr_t address = result.Results[i];
                bool isSelected = (result.SelectedIndex == i);
                ImGui::TableNextRow();

                // Address
                ImGui::TableSetColumnIndex(0);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeightWithSpacing() / 2 - ImGui::GetTextLineHeight() / 2));
                std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(address, m_State.Modules);
                if (modEntry) {
                    ImGui::Text("%s+0x%llX", modEntry->szModule, address - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                } else {
                    ImGui::Text("0x%llX", address);
                }

                ImGui::SameLine();

                ImGui::TableSetColumnIndex(0);
                std::string selectableLabel = std::format("##pattern_scan_span_{}_{:X}", i, address);
                if (ImGui::Selectable(selectableLabel.c_str(), isSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    result.SelectedIndex = i;
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}