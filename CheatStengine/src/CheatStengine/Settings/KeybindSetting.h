#pragma once

#include "Setting.h"

#include <imgui.h>

#include <functional>
#include <string>

struct KeyChord {
    ImGuiKey Key = ImGuiKey_None;
    bool Ctrl = false;
    bool Shift = false;
    bool Alt = false;
    bool Super = false;

    KeyChord() = default;

    explicit KeyChord(ImGuiKey key, bool ctrl = false, bool shift = false, bool alt = false, bool super = false)
        : Key(key), Ctrl(ctrl), Shift(shift), Alt(alt), Super(super) {}

    explicit KeyChord(ImGuiKeyChord chord);

    explicit operator ImGuiKeyChord() const;

    std::string ToString() const;

    bool operator==(const KeyChord& other) const
    {
        return Key == other.Key && Ctrl == other.Ctrl && Shift == other.Shift && Alt == other.Alt && Super == other.Super;
    }

    bool operator!=(const KeyChord& other) const { return !(*this == other); }
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

    void Draw() override;

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

    bool IsPressed() const;

private:
    void HandleKeyInput();

private:
    std::string m_Name;
    std::string m_Description;
    KeyChord m_Value;
    KeyChord m_TempValue;
    std::function<void(KeyChord)> m_Callback;
    bool m_WaitingForInput;
};