#include "SettingsCategory.h"

#include <imgui.h>

void SettingsCategory::Draw(bool drawHeader) const
{
    if (drawHeader) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
        ImGui::TextUnformatted(m_Name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
    }

    for (const std::unique_ptr<Setting>& setting : m_Settings) {
        setting->Draw();
        ImGui::Spacing();
    }

    if (!m_SubCategories.empty()) {
        ImGui::Indent(20.0f);
        for (const SettingsCategory& subCategory : m_SubCategories) {
            subCategory.Draw(true);
            ImGui::Spacing();
        }
        ImGui::Unindent(20.0f);
    }
}

void SettingsCategory::Restore() const
{
    for (const std::unique_ptr<Setting>& setting : m_Settings) {
        setting->Restore();
    }
    for (const SettingsCategory& subCategory : m_SubCategories) {
        subCategory.Restore();
    }
}

void SettingsCategory::Apply() const
{
    for (const std::unique_ptr<Setting>& setting : m_Settings) {
        setting->Apply();
    }
    for (const SettingsCategory& subCategory : m_SubCategories) {
        subCategory.Apply();
    }
}

bool SettingsCategory::HasValueChanged() const
{
    for (const std::unique_ptr<Setting>& setting : m_Settings) {
        if (setting->HasValueChanged()) {
            return true;
        }
    }
    for (const SettingsCategory& subCategory : m_SubCategories) {
        if (subCategory.HasValueChanged()) {
            return true;
        }
    }
    return false;
}

Setting* SettingsCategory::GetSetting(const std::string& name) const
{
    auto it = std::ranges::find_if(m_Settings, [&name](const auto& setting) {
        return setting->GetName() == name;
    });
    return (it != m_Settings.end()) ? it->get() : nullptr;
}