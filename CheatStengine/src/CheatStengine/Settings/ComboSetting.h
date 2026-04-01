#pragma once

#include "Setting.h"

class ComboSetting final : public Setting {
public:
    ComboSetting(std::string name, std::string description, std::vector<std::string> items, int defaultValue = 0)
        : m_Name(std::move(name))
        , m_Description(std::move(description))
        , m_Items(std::move(items))
        , m_CurrentItem(defaultValue)
        , m_TempItem(defaultValue)
    {
    }

    void Draw() override
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

    void Restore() override { m_TempItem = m_CurrentItem; }
    void Apply() override { m_CurrentItem = m_TempItem; }

    [[nodiscard]] const std::string& GetCurrentItem() const { return m_Items[m_CurrentItem]; }
    [[nodiscard]] int GetValue() const { return m_CurrentItem; }
    void SetValue(int index)
    {
        if (index >= 0 && index < m_Items.size()) {
            m_CurrentItem = index;
        }
    }

    [[nodiscard]] std::string GetName() const override { return m_Name; }
    [[nodiscard]] std::string GetDescription() const override { return m_Description; }
    [[nodiscard]] bool HasValueChanged() const override { return m_CurrentItem != m_TempItem; }

private:
    std::string m_Name;
    std::string m_Description;
    std::vector<std::string> m_Items;
    int m_CurrentItem;
    int m_TempItem;
};
