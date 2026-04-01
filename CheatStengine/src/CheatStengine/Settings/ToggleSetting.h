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

    void Draw() override;

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
