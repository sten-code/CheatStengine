#pragma once

namespace ImGui {
    bool RoundedMenuItem(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
    bool RoundedMenuItemEx(const char* label, const char* icon, const char* shortcut = nullptr, bool selected = false, bool enabled = true);

    bool BeginRoundedMenu(const char* label, bool enabled = true);
    bool BeginRoundedMenuEx(const char* label, const char* icon, bool enabled = true);
}