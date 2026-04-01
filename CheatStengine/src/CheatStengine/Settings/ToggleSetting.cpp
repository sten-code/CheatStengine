#include "ToggleSetting.h"

#include <imgui.h>

void ToggleSetting::Draw()
{
    ImGui::PushID(m_Name.c_str());
    ImGui::BeginGroup();

    ImGui::Checkbox(m_Name.c_str(), &m_TempValue);

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