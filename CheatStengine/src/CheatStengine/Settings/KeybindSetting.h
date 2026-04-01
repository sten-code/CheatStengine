#pragma once

#include "Setting.h"

#include <imgui.h>

struct KeyChord {
    ImGuiKey Key = ImGuiKey_None;
    bool Ctrl = false;
    bool Shift = false;
    bool Alt = false;
    bool Super = false;

    KeyChord() = default;

    KeyChord(ImGuiKey key, bool ctrl = false, bool shift = false, bool alt = false, bool super = false)
        : Key(key), Ctrl(ctrl), Shift(shift), Alt(alt), Super(super) {}

    KeyChord(ImGuiKeyChord chord)
    {
        Ctrl = chord & ImGuiMod_Ctrl;
        Shift = chord & ImGuiMod_Shift;
        Alt = chord & ImGuiMod_Alt;
        Super = chord & ImGuiMod_Super;
        Key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    }

    operator ImGuiKeyChord() const
    {
        ImGuiKeyChord chord = static_cast<ImGuiKeyChord>(Key);
        if (Ctrl) chord |= ImGuiMod_Ctrl;
        if (Shift) chord |= ImGuiMod_Shift;
        if (Alt) chord |= ImGuiMod_Alt;
        if (Super) chord |= ImGuiMod_Super;
        return chord;
    }

    std::string ToString() const
    {
        if (Key == ImGuiKey_None) return "None";

        std::string result;
        if (Ctrl) result += "Ctrl+";
        if (Shift) result += "Shift+";
        if (Alt) result += "Alt+";
        if (Super) result += "Super+";

        const char* keyName = ImGui::GetKeyName(Key);
        if (keyName) {
            result += keyName;
        } else {
            result += "Unknown";
        }

        return result;
    }

    bool operator==(const KeyChord& other) const
    {
        return Key == other.Key && Ctrl == other.Ctrl && Shift == other.Shift && Alt == other.Alt && Super == other.Super;
    }

    bool operator!=(const KeyChord& other) const
    {
        return !(*this == other);
    }
};

class KeybindSetting final : public Setting {
public:
    KeybindSetting(std::string name, std::string description, KeyChord defaultKeyChord = KeyChord {})
        : m_Name(std::move(name))
        , m_Description(std::move(description))
        , m_Value(defaultKeyChord)
        , m_TempValue(defaultKeyChord)
        , m_WaitingForInput(false)
    {
    }

    // Constructor with callback
    KeybindSetting(std::string name, std::string description, KeyChord defaultKeyChord, std::function<void(KeyChord)> callback)
        : m_Name(std::move(name))
        , m_Description(std::move(description))
        , m_Value(defaultKeyChord)
        , m_TempValue(defaultKeyChord)
        , m_Callback(std::move(callback))
        , m_WaitingForInput(false)
    {
    }

    void Draw() override
    {
        ImGui::PushID(m_Name.c_str());
        ImGui::BeginGroup();

        ImGui::TextUnformatted(m_Name.c_str());

        if (ImGui::IsItemHovered() && !m_Description.empty()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(m_Description.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() * 0.6f);

        std::string buttonText = m_WaitingForInput ? "Press any key..." : m_TempValue.ToString();
        if (ImGui::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 30.0f, 0))) {
            m_WaitingForInput = true;
        }

        if (m_WaitingForInput) {
            HandleKeyInput();
        }

        ImGui::SameLine(0, 5);

        if (ImGui::Button("X", ImVec2(25, 0))) {
            m_TempValue = KeyChord {};
            m_WaitingForInput = false;
        }

        ImGui::EndGroup();
        ImGui::PopID();
    }

    void Restore() override
    {
        m_TempValue = m_Value;
        m_WaitingForInput = false;
    }

    void Apply() override
    {
        m_Value = m_TempValue;
        if (m_Callback) {
            m_Callback(m_Value);
        }
    }

    [[nodiscard]] std::string GetName() const override { return m_Name; }
    [[nodiscard]] std::string GetDescription() const override { return m_Description; }
    [[nodiscard]] bool HasValueChanged() const override { return m_Value != m_TempValue; }
    [[nodiscard]] KeyChord GetValue() const { return m_Value; }
    [[nodiscard]] KeyChord GetTempValue() const { return m_TempValue; }

    void SetValue(const KeyChord& value)
    {
        m_Value = value;
        m_TempValue = value;
    }

    void SetCallback(std::function<void(KeyChord)> callback)
    {
        m_Callback = std::move(callback);
    }

    bool IsPressed() const
    {
        if (m_Value.Key == ImGuiKey_None) {
            return false;
        }

        bool ctrlDown = (m_Value.Ctrl && ImGui::GetIO().KeyCtrl) || (!m_Value.Ctrl && !ImGui::GetIO().KeyCtrl);
        bool shiftDown = (m_Value.Shift && ImGui::GetIO().KeyShift) || (!m_Value.Shift && !ImGui::GetIO().KeyShift);
        bool altDown = (m_Value.Alt && ImGui::GetIO().KeyAlt) || (!m_Value.Alt && !ImGui::GetIO().KeyAlt);
        bool superDown = (m_Value.Super && ImGui::GetIO().KeySuper) || (!m_Value.Super && !ImGui::GetIO().KeySuper);

        return ctrlDown && shiftDown && altDown && superDown && ImGui::IsKeyPressed(m_Value.Key);
    }

private:
    void HandleKeyInput()
    {
        ImGuiIO& io = ImGui::GetIO();

        bool ctrl = io.KeyCtrl;
        bool shift = io.KeyShift;
        bool alt = io.KeyAlt;
        bool super = io.KeySuper;

        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
            ImGuiKey key = static_cast<ImGuiKey>(k);
            if (ImGui::IsKeyPressed(key)) {
                // Don't capture modifier keys alone
                if (k == ImGuiKey_LeftCtrl || k == ImGuiKey_LeftShift || k == ImGuiKey_LeftAlt || k == ImGuiKey_LeftSuper
                    || k == ImGuiKey_RightCtrl || k == ImGuiKey_RightShift || k == ImGuiKey_RightAlt || k == ImGuiKey_RightSuper
                    || k == ImGuiKey_ReservedForModCtrl || k == ImGuiKey_ReservedForModShift || k == ImGuiKey_ReservedForModAlt || k == ImGuiKey_ReservedForModSuper) {
                    continue;
                }

                m_TempValue = KeyChord(key, ctrl, shift, alt, super);
                m_WaitingForInput = false;
                return;
            }
        }

        // Escape cancels
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_WaitingForInput = false;
        }
    }

private:
    std::string m_Name;
    std::string m_Description;
    KeyChord m_Value;
    KeyChord m_TempValue;
    std::function<void(KeyChord)> m_Callback;
    bool m_WaitingForInput;
};