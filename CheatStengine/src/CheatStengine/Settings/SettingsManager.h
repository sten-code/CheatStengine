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

    void DrawSettingsPopup(const std::string& name)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2 { 0.5f, 0.5f });
        ImGui::SetNextWindowSize(ImVec2 { 600, 500 }, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal(name.c_str(), nullptr)) {
            if (ImGui::BeginChild("SettingsContent", ImVec2(0, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()), false)) {
                DrawCategoryList();
            }
            ImGui::EndChild();

            if (ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
                Apply();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
                Restore();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();

            bool hasValueChanged = HasValueChanged();
            if (!hasValueChanged) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Apply", ImVec2 { 70.0f, 0 })) {
                Apply();
            }

            if (!hasValueChanged) {
                ImGui::EndDisabled();
            }

            ImGui::EndPopup();
        }
    }

    void Restore() const
    {
        for (const SettingsCategory& category : m_Categories) {
            category.Restore();
        }
    }

    void Apply() const
    {
        for (const SettingsCategory& category : m_Categories) {
            category.Apply();
        }
    }

    [[nodiscard]] bool HasValueChanged() const
    {
        for (const SettingsCategory& category : m_Categories) {
            if (category.HasValueChanged()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] SettingsCategory* GetCategory(const std::string& name)
    {
        auto it = std::ranges::find_if(m_Categories, [&name](const SettingsCategory& category) {
            return category.GetName() == name;
        });
        if (it == m_Categories.end()) {
            ERR("SettingsCategory::GetCategory: Settings category '{}' not found", name);
            return nullptr;
        }
        return &*it;
    }

private:
    void DrawCategoryList()
    {
        if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
            for (const SettingsCategory& category : m_Categories) {
                if (ImGui::BeginTabItem(category.GetName().c_str())) {
                    ImGui::Spacing();
                    category.Draw();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

private:
    std::vector<SettingsCategory> m_Categories;
};
