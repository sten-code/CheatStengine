#pragma once

#include "SettingsCategory.h"

#include <imgui.h>

class SettingsManager {
public:
    SettingsManager() = default;
    ~SettingsManager() = default;

    SettingsCategory& AddCategory(std::string name)
    {
        m_Categories.emplace_back(std::move(name));
        return m_Categories.back();
    }

    void DrawSettingsPopup(const std::string& name);

    void Restore() const;
    void Apply() const;
    [[nodiscard]] bool HasValueChanged() const;

    [[nodiscard]] SettingsCategory* GetCategory(const std::string& name);

private:
    void DrawCategoryList() const;

private:
    std::vector<SettingsCategory> m_Categories;
};
