#include "ComboSetting.h"

#include <imgui.h>

void ComboSetting::Draw()
{
    ImGui::PushID(m_Name.c_str());
    ImGui::BeginGroup();

    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.4f);
    if (ImGui::BeginCombo(m_Name.c_str(), m_Items[m_TempItem].c_str())) {
        for (int i = 0; i < m_Items.size(); ++i) {
            bool isSelected = (m_TempItem == i);
            if (ImGui::Selectable(m_Items[i].c_str(), isSelected)) {
                m_TempItem = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::IsItemHovered() && !m_Description.empty()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(m_Description.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::EndGroup();
    ImGui::PopID();
}

void ComboSetting::SetValue(int index)
{
    if (index >= 0 && index < m_Items.size()) {
        m_CurrentItem = index;
    }
}