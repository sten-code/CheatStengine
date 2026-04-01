#pragma once

#include "Setting.h"

class SettingsCategory {
public:
    explicit SettingsCategory(std::string name)
        : m_Name(std::move(name)) {}

    template <typename T, typename... Args>
    T* AddSetting(Args&&... args)
    {
        auto setting = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = setting.get();
        m_Settings.push_back(std::move(setting));
        return ptr;
    }

    void Draw() const
    {
        if (ImGui::BeginChild(("##" + m_Name).c_str(), ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar)) {
            for (auto& setting : m_Settings) {
                setting->Draw();
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();
    }

    void Restore() const
    {
        for (const std::unique_ptr<Setting>& setting : m_Settings) {
            setting->Restore();
        }
    }

    void Apply() const
    {
        for (const std::unique_ptr<Setting>& setting : m_Settings) {
            setting->Apply();
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
};
