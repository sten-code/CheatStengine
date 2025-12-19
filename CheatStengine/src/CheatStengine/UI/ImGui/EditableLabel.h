#pragma once

#include <imgui.h>

#include <string>

namespace ImGui {

    bool EditableLabel(const char* str_id, std::string* str, ImGuiInputTextFlags flags = 0);

}