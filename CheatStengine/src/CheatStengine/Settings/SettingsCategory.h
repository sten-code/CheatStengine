#pragma once

#include "Setting.h"

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

    void Draw(bool drawHeader = false) const
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

    void Restore() const
    {
        for (const std::unique_ptr<Setting>& setting : m_Settings) {
            setting->Restore();
        }
        for (const SettingsCategory& subCategory : m_SubCategories) {
            subCategory.Restore();
        }
    }

    void Apply() const
    {
        for (const std::unique_ptr<Setting>& setting : m_Settings) {
            setting->Apply();
        }
        for (const SettingsCategory& subCategory : m_SubCategories) {
            subCategory.Apply();
        }
    }

    [[nodiscard]] const std::string& GetName() const { return m_Name; }
    [[nodiscard]] bool HasValueChanged() const
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

    Setting* GetSetting(const std::string& name) const
    {
        auto it = std::ranges::find_if(m_Settings, [&name](const auto& setting) {
            return setting->GetName() == name;
        });
        return (it != m_Settings.end()) ? it->get() : nullptr;
    }

    template <typename T>
    T* GetSettingAs(const std::string& name) const { return dynamic_cast<T*>(GetSetting(name)); }

private:
    std::string m_Name;
    std::vector<std::unique_ptr<Setting>> m_Settings;
    std::vector<SettingsCategory> m_SubCategories;
};
