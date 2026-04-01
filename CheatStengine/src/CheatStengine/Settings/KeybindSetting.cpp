#include "KeybindSetting.h"

#include <CheatStengine/UI/ImGui/Fonts.h>

#include <IconsMaterialDesignIcons.h>

KeyChord::KeyChord(ImGuiKeyChord chord)
{
    Ctrl = chord & ImGuiMod_Ctrl;
    Shift = chord & ImGuiMod_Shift;
    Alt = chord & ImGuiMod_Alt;
    Super = chord & ImGuiMod_Super;
    Key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
}

KeyChord::operator int() const
{
    ImGuiKeyChord chord = Key;
    if (Ctrl) chord |= ImGuiMod_Ctrl;
    if (Shift) chord |= ImGuiMod_Shift;
    if (Alt) chord |= ImGuiMod_Alt;
    if (Super) chord |= ImGuiMod_Super;
    return chord;
}

std::string KeyChord::ToString() const
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

void KeybindSetting::Draw()
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
    if (ImGui::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 10, 0))) {
        m_WaitingForInput = true;
    }

    if (m_WaitingForInput) {
        HandleKeyInput();
    }

    ImGui::SameLine(0, 5);

    Fonts::Push(Fonts::Type::MaterialDesignIcons);
    if (ImGui::Button(ICON_MDI_CLOSE, ImVec2(ImGui::GetFrameHeight() + 5, ImGui::GetFrameHeight()))) {
        m_TempValue = KeyChord {};
        m_WaitingForInput = false;
    }
    Fonts::Pop();

    ImGui::EndGroup();
    ImGui::PopID();
}

bool KeybindSetting::IsPressed() const
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

void KeybindSetting::HandleKeyInput()
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