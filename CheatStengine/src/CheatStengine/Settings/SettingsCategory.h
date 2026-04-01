#pragma once

#include "Setting.h"

#include <memory>
#include <vector>

class SettingsCategory {
public:
    explicit SettingsCategory(std::string name)
        : m_Name(std::move(name)) {}

    SettingsCategory& AddSubCategory(std::string name)
    {
        m_SubCategories.emplace_back(std::move(name));
        return m_SubCategories.back();
    }

    template <typename T, typename... Args>
    T* AddSetting(Args&&... args)
    {
        auto setting = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = setting.get();
        m_Settings.push_back(std::move(setting));
        return ptr;
    }

    void Draw(bool drawHeader = false) const;

    void Restore() const;
    void Apply() const;

    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] bool HasValueChanged() const;

    Setting* GetSetting(const std::string& name) const;

    template <typename T>
    T* GetSettingAs(const std::string& name) const { return dynamic_cast<T*>(GetSetting(name)); }

private:
    std::string m_Name;
    std::vector<std::unique_ptr<Setting>> m_Settings;
    std::vector<SettingsCategory> m_SubCategories;
};
