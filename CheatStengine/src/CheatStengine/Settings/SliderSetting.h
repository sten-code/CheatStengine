#pragma once

#include "Setting.h"

class SliderSetting final : public Setting {
public:
    SliderSetting(std::string name, std::string description, float defaultValue = 0.5f, float min = 0.0f, float max = 1.0f)
        : m_Name(std::move(name))
        , m_Description(std::move(description))
        , m_Value(defaultValue)
        , m_TempValue(defaultValue)
        , m_Min(min)
        , m_Max(max) {}

    void Draw() override
    {
        ImGui::PushID(m_Name.c_str());
        ImGui::BeginGroup();

        ImGui::SliderFloat(m_Name.c_str(), &m_TempValue, m_Min, m_Max, "%.2f");

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

    [[nodiscard]] float GetValue() const { return m_Value; }
    void SetValue(float value) { m_Value = value; }

    [[nodiscard]] std::string GetName() const override { return m_Name; }
    [[nodiscard]] std::string GetDescription() const override { return m_Description; }
    [[nodiscard]] bool HasValueChanged() const override { return m_Value != m_TempValue; }

private:
    std::string m_Name;
    std::string m_Description;
    float m_Value;
    float m_TempValue;
    float m_Min;
    float m_Max;
};
