#pragma once

#include <imgui.h>

#include <functional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

class Keybind {
public:
    std::string Name;
    std::string Description;
    std::string Category;
    ImGuiKeyChord KeyChord;
    std::function<void()> Callback;
};

class KeybindManager {
public:
    KeybindManager() = default;
    ~KeybindManager() = default;

    void RegisterKeybind(const std::string& name,
        const std::string& description,
        const std::string& category,
        ImGuiKeyChord keyChord,
        std::function<void()> callback = nullptr)
    {
        Keybind& keybind = m_Keybinds.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple(name, description, category, keyChord, callback)).first->second;
        m_KeybindsByCategory[category].push_back(&keybind);
    }

    void Update() const
    {
        for (const Keybind& keybind : m_Keybinds | std::views::values) {
            if (ImGui::IsKeyChordPressed(keybind.KeyChord)) {
                if (keybind.Callback) {
                    keybind.Callback();
                }
            }
        }
    }

    [[nodiscard]] std::vector<std::string> GetCategories() const
    {
        std::vector<std::string> categories;
        for (const std::string& category : m_KeybindsByCategory | std::views::keys) {
            categories.push_back(category);
        }
        return categories;
    }

    [[nodiscard]] const std::vector<Keybind*>& GetShortcutsInCategory(const std::string& category) const
    {
        static std::vector<Keybind*> empty;
        auto it = m_KeybindsByCategory.find(category);
        if (it != m_KeybindsByCategory.end()) {
            return it->second;
        }
        return empty;
    }

    [[nodiscard]] bool IsShortcutPressed(const std::string& name) const
    {
        auto it = m_Keybinds.find(name);
        if (it != m_Keybinds.end()) {
            return ImGui::IsKeyChordPressed(it->second.KeyChord);
        }
        return false;
    }

private:
    std::unordered_map<std::string, Keybind> m_Keybinds;
    std::unordered_map<std::string, std::vector<Keybind*>> m_KeybindsByCategory;
    bool m_ShortcutsEnabled = true;
};