#pragma once

#include "Setting.h"

#include <string>
#include <vector>

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

    void Draw() override;

    void Restore() override { m_TempItem = m_CurrentItem; }
    void Apply() override { m_CurrentItem = m_TempItem; }

    [[nodiscard]] const std::string& GetCurrentItem() const { return m_Items[m_CurrentItem]; }
    [[nodiscard]] int GetValue() const { return m_CurrentItem; }
    void SetValue(int index);

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
