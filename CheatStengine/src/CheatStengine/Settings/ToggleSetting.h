#pragma once

#include "Setting.h"

class ToggleSetting final : public Setting {
public:
    ToggleSetting(std::string name, std::string description, bool defaultValue = false)
        : m_Name(std::move(name))
        , m_Description(std::move(description))
        , m_Value(defaultValue)
        , m_TempValue(defaultValue)
    {
    }

    void Draw() override
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

    void Restore() override { m_TempValue = m_Value; }
    void Apply() override { m_Value = m_TempValue; }

    [[nodiscard]] bool GetValue() const { return m_Value; }
    void SetValue(bool value) { m_Value = value; }

    [[nodiscard]] std::string GetName() const override { return m_Name; }
    [[nodiscard]] std::string GetDescription() const override { return m_Description; }
    [[nodiscard]] bool HasValueChanged() const override { return m_Value != m_TempValue; }

private:
    std::string m_Name;
    std::string m_Description;
    bool m_Value;
    bool m_TempValue;
};
