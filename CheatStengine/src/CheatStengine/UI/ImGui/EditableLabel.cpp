#include "EditableLabel.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace ImGui {

    bool EditableLabel(const char* str_id, std::string* str, ImGuiInputTextFlags flags)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems) {
            return false;
        }

        const ImGuiID id = window->GetID(str_id);
        const ImGuiID storage_id = id + 1;

        ImGuiStorage* storage = &window->StateStorage;
        const bool is_editing = storage->GetBool(storage_id, false);
        bool value_changed = false;

        if (is_editing) {
            if (storage->GetInt(storage_id + 2, 0) == g.FrameCount - 1) {
                SetKeyboardFocusHere();
            }

            PushItemWidth(-FLT_MIN);
            PushID(id);
            if (InputText("##edit", str, flags | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                storage->SetBool(storage_id, false);
                value_changed = true;
            }
            PopID();
            PopItemWidth();

            if (IsKeyPressed(ImGuiKey_Escape) || (IsItemDeactivated() && !value_changed)) {
                storage->SetBool(storage_id, false);
            }
        } else {
            PushID(id);
            Selectable("##selectable", false, ImGuiSelectableFlags_AllowOverlap);

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                storage->SetBool(storage_id, true);
                storage->SetInt(storage_id + 2, g.FrameCount);
            }

            SameLine(0, 0);
            TextUnformatted(str->c_str());
            PopID();
        }

        return value_changed;
    }

}