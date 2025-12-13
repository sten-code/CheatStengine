#define IMGUI_DEFINE_MATH_OPERATORS
#include "Menu.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui {

    static bool IsRootOfOpenMenuSet()
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        if ((g.OpenPopupStack.Size <= g.BeginPopupStack.Size) || (window->Flags & ImGuiWindowFlags_ChildMenu))
            return false;

        const ImGuiPopupData* upper_popup = &g.OpenPopupStack[g.BeginPopupStack.Size];
        if (window->DC.NavLayerCurrent != upper_popup->ParentNavLayer)
            return false;
        return upper_popup->Window && (upper_popup->Window->Flags & ImGuiWindowFlags_ChildMenu) && IsWindowChildOf(upper_popup->Window, window, true, false);
    }

    bool RoundedMenuItem(const char* label, const char* shortcut, bool selected, bool enabled)
    {
        return RoundedMenuItemEx(label, nullptr, shortcut, selected, enabled);
    }

    bool RoundedMenuItemEx(const char* label, const char* icon, const char* shortcut, bool selected, bool enabled)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        // See BeginMenuEx() for comments about this.
        const bool menuset_is_open = IsRootOfOpenMenuSet();
        if (menuset_is_open)
            PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // We've been using the equivalent of ImGuiSelectableFlags_SetNavIdOnHover on all Selectable() since early Nav system days (commit 43ee5d73),
        // but I am unsure whether this should be kept at all. For now moved it to be an opt-in feature used by menus only.
        bool pressed;
        if (!enabled)
            BeginDisabled();

        // We use ImGuiSelectableFlags_NoSetKeyOwner to allow down on one menu item, move, up on another.
        const ImGuiButtonFlags button_flags = ImGuiButtonFlags_NoSetKeyOwner;
        const ImGuiMenuColumns* offsets = &window->DC.MenuColumns;

        if (window->DC.LayoutType == ImGuiLayoutType_Horizontal) {
            // Mimic the exact layout spacing of BeginMenu() to allow MenuItem() inside a menu bar, which is a little misleading but may be useful
            // Note that in this situation: we don't render the shortcut, we render a highlight instead of the selected tick mark.
            float w = label_size.x;
            window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
            ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
            PushStyleVarX(ImGuiStyleVar_ItemSpacing, style.ItemSpacing.x * 2.0f);

            ImVec2 size(w, label_size.y + style.FramePadding.y * 2.0f);
            ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
            ItemSize(size);
            ItemAdd(bb, id);

            bool hovered, held;
            pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);

            if (pressed && enabled)
                CloseCurrentPopup();

            const ImU32 col = GetColorU32(
                (held && hovered) ? ImGuiCol_ButtonActive
                    : hovered     ? ImGuiCol_ButtonHovered
                                  : ImGuiCol_Button);

            RenderNavHighlight(bb, id);
            RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

            if (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_Visible)
                RenderText(text_pos, label);

            PopStyleVar();
            window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
        } else {
            // Menu item inside a vertical menu
            // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
            //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.)
            float icon_w = (icon && icon[0]) ? CalcTextSize(icon, NULL).x : 0.0f;
            float shortcut_w = (shortcut && shortcut[0]) ? CalcTextSize(shortcut, NULL).x : 0.0f;
            float checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
            float min_w = window->DC.MenuColumns.DeclColumns(icon_w, label_size.x, shortcut_w, checkmark_w); // Feedback for next frame
            float stretch_w = ImMax(0.0f, GetContentRegionAvail().x - min_w);

            // Custom rounded selectable for vertical layout
            ImVec2 size(min_w + stretch_w, label_size.y + style.FramePadding.y * 2.0f);
            ImRect bb(pos, pos + size);
            ItemSize(size);
            ItemAdd(bb, id);

            bool hovered, held;
            pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);

            if (pressed && enabled)
                CloseCurrentPopup();

            if (hovered) {
                const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                        : hovered                               ? ImGuiCol_HeaderHovered
                                                                : ImGuiCol_Header);
                RenderNavHighlight(bb, id);
                RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
            }
            if (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_Visible) {
                RenderText(bb.Min + style.FramePadding + ImVec2(offsets->OffsetLabel, 0), label);
                if (icon_w > 0.0f)
                    RenderText(bb.Min + style.FramePadding + ImVec2(offsets->OffsetIcon, 0), icon);
                if (shortcut_w > 0.0f) {
                    PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
                    LogSetNextTextDecoration("(", ")");
                    RenderText(bb.Min + style.FramePadding + ImVec2(offsets->OffsetShortcut + stretch_w, 0), shortcut, NULL, false);
                    PopStyleColor();
                }
                if (selected)
                    RenderCheckMark(window->DrawList, bb.Min + ImVec2(offsets->OffsetMark + stretch_w, g.FontSize * 0.134f * 0.5f + style.FramePadding.y), GetColorU32(ImGuiCol_Text), g.FontSize * 0.866f);
            }
        }

        IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (selected ? ImGuiItemStatusFlags_Checked : 0));
        if (!enabled)
            EndDisabled();

        if (menuset_is_open)
            PopItemFlag();

        return pressed;
    }

    bool BeginRoundedMenu(const char* label, bool enabled)
    {
        return BeginRoundedMenuEx(label, nullptr, enabled);
    }

    bool BeginRoundedMenuEx(const char* label, const char* icon, bool enabled)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        bool menu_is_open = IsPopupOpen(id, ImGuiPopupFlags_None);

        // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would steal focus and not allow hovering on parent menu)
        // The first menu in a hierarchy isn't so hovering doesn't get across (otherwise e.g. resizing borders with ImGuiButtonFlags_FlattenChildren would react), but top-most BeginMenu() will bypass that limitation.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (window->Flags & ImGuiWindowFlags_ChildMenu)
            window_flags |= ImGuiWindowFlags_ChildWindow;

        // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
        // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the expected small amount of BeginMenu() calls per frame.
        // If somehow this is ever becoming a problem we can switch to use e.g. ImGuiStorage mapping key to last frame used.
        if (g.MenusIdSubmittedThisFrame.contains(id)) {
            if (menu_is_open)
                menu_is_open = BeginPopupMenuEx(id, label, window_flags); // menu_is_open can be 'false' when the popup is completely clipped (e.g. zero size display)
            else
                g.NextWindowData.ClearFlags(); // we behave like Begin() and need to consume those values
            return menu_is_open;
        }

        // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
        g.MenusIdSubmittedThisFrame.push_back(id);

        ImVec2 label_size = CalcTextSize(label, NULL, true);

        // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent without always being a Child window)
        // This is only done for items for the menu set and not the full parent window.
        const bool menuset_is_open = IsRootOfOpenMenuSet();
        if (menuset_is_open)
            PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);

        // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child menu,
        // However the final position is going to be different! It is chosen by FindBestWindowPosForPopup().
        // e.g. Menus tend to overlap each other horizontally to amplify relative Z-ordering.
        ImVec2 popup_pos, pos = window->DC.CursorPos;
        PushID(label);
        if (!enabled)
            BeginDisabled();
        const ImGuiMenuColumns* offsets = &window->DC.MenuColumns;
        bool pressed;

        // We use ImGuiSelectableFlags_NoSetKeyOwner to allow down on one menu item, move, up on another.
        const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_NoSetKeyOwner | ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_NoAutoClosePopups;
        if (window->DC.LayoutType == ImGuiLayoutType_Horizontal) {
            // Menu inside a horizontal menu bar
            // Selectable extend their highlight by half ItemSpacing in each direction.
            // For ChildMenu, the popup position will be overwritten by the call to FindBestWindowPosForPopup() in Begin()
            popup_pos = ImVec2(pos.x - 1.0f - IM_TRUNC(style.ItemSpacing.x * 0.5f), pos.y - style.FramePadding.y + window->MenuBarHeight);
            window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
            PushStyleVarX(ImGuiStyleVar_ItemSpacing, style.ItemSpacing.x * 2.0f);
            float w = label_size.x;
            ImVec2 text_pos(window->DC.CursorPos.x + offsets->OffsetLabel, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);

            ImVec2 size(w, label_size.y + style.FramePadding.y * 2.0f);
            ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
            ItemSize(size);
            ItemAdd(bb, id);

            bool hovered, held;
            pressed = ButtonBehavior(bb, id, &hovered, &held, selectable_flags);

            if (hovered) {
                const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                        : hovered                               ? ImGuiCol_HeaderHovered
                                                                : ImGuiCol_Header);
                RenderNavHighlight(bb, id);
                RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
            }

            LogSetNextTextDecoration("[", "]");
            RenderText(text_pos, label);
            PopStyleVar();

            window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
        } else {
            // Menu inside a regular/vertical menu
            // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
            //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.)
            popup_pos = ImVec2(pos.x, pos.y - style.WindowPadding.y);
            float icon_w = (icon && icon[0]) ? CalcTextSize(icon, NULL).x : 0.0f;
            float checkmark_w = IM_TRUNC(g.FontSize * 1.20f);
            float min_w = window->DC.MenuColumns.DeclColumns(icon_w, label_size.x, 0.0f, checkmark_w); // Feedback to next frame

            ImVec2 size(min_w, label_size.y + style.FramePadding.y * 2.0f);
            ImRect bb(pos, pos + size);
            ItemSize(size);
            ItemAdd(bb, id);

            bool hovered, held;
            pressed = ButtonBehavior(bb, id, &hovered, &held, selectable_flags | ImGuiSelectableFlags_SpanAvailWidth);

            if (hovered) {
                const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive
                        : hovered                               ? ImGuiCol_HeaderHovered
                                                                : ImGuiCol_Header);
                RenderNavHighlight(bb, id);
                RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
            }

            LogSetNextTextDecoration("", ">");
            RenderText(bb.Min + style.FramePadding + ImVec2(offsets->OffsetLabel, 0), label);
            if (icon_w > 0.0f)
                RenderText(bb.Min + style.FramePadding + ImVec2(offsets->OffsetIcon, 0), icon);
            RenderArrow(window->DrawList, bb.Min + style.FramePadding + ImVec2(offsets->OffsetMark, 0), GetColorU32(ImGuiCol_Text), ImGuiDir_Right);
        }
        if (!enabled)
            EndDisabled();

        const bool hovered = (g.HoveredId == id) && enabled && !g.NavHighlightItemUnderNav;
        if (menuset_is_open)
            PopItemFlag();

        bool want_open = false;
        bool want_open_nav_init = false;
        bool want_close = false;
        if (window->DC.LayoutType == ImGuiLayoutType_Vertical) // (window->Flags & (ImGuiWindowFlags_Popup|ImGuiWindowFlags_ChildMenu))
        {
            // Close menu when not hovering it anymore unless we are moving roughly in the direction of the menu
            // Implement http://bjk5.com/post/44698559168/breaking-down-amazons-mega-dropdown to avoid using timers, so menus feels more reactive.
            bool moving_toward_child_menu = false;
            ImGuiPopupData* child_popup = (g.BeginPopupStack.Size < g.OpenPopupStack.Size) ? &g.OpenPopupStack[g.BeginPopupStack.Size] : NULL; // Popup candidate (testing below)
            ImGuiWindow* child_menu_window = (child_popup && child_popup->Window && child_popup->Window->ParentWindow == window) ? child_popup->Window : NULL;
            if (g.HoveredWindow == window && child_menu_window != NULL) {
                const float ref_unit = g.FontSize; // FIXME-DPI
                const float child_dir = (window->Pos.x < child_menu_window->Pos.x) ? 1.0f : -1.0f;
                const ImRect next_window_rect = child_menu_window->Rect();
                ImVec2 ta = (g.IO.MousePos - g.IO.MouseDelta);
                ImVec2 tb = (child_dir > 0.0f) ? next_window_rect.GetTL() : next_window_rect.GetTR();
                ImVec2 tc = (child_dir > 0.0f) ? next_window_rect.GetBL() : next_window_rect.GetBR();
                const float pad_farmost_h = ImClamp(ImFabs(ta.x - tb.x) * 0.30f, ref_unit * 0.5f, ref_unit * 2.5f); // Add a bit of extra slack.
                ta.x += child_dir * -0.5f;
                tb.x += child_dir * ref_unit;
                tc.x += child_dir * ref_unit;
                tb.y = ta.y + ImMax((tb.y - pad_farmost_h) - ta.y, -ref_unit * 8.0f); // Triangle has maximum height to limit the slope and the bias toward large sub-menus
                tc.y = ta.y + ImMin((tc.y + pad_farmost_h) - ta.y, +ref_unit * 8.0f);
                moving_toward_child_menu = ImTriangleContainsPoint(ta, tb, tc, g.IO.MousePos);
                // GetForegroundDrawList()->AddTriangleFilled(ta, tb, tc, moving_toward_child_menu ? IM_COL32(0,128,0,128) : IM_COL32(128,0,0,128)); // [DEBUG]
            }

            // The 'HovereWindow == window' check creates an inconsistency (e.g. moving away from menu slowly tends to hit same window, whereas moving away fast does not)
            // But we also need to not close the top-menu menu when moving over void. Perhaps we should extend the triangle check to a larger polygon.
            // (Remember to test this on BeginPopup("A")->BeginMenu("B") sequence which behaves slightly differently as B isn't a Child of A and hovering isn't shared.)
            if (menu_is_open && !hovered && g.HoveredWindow == window && !moving_toward_child_menu && !g.NavHighlightItemUnderNav && g.ActiveId == 0)
                want_close = true;

            // Open
            // (note: at this point 'hovered' actually includes the NavDisableMouseHover == false test)
            if (!menu_is_open && pressed) // Click/activate to open
                want_open = true;
            else if (!menu_is_open && hovered && !moving_toward_child_menu) // Hover to open
                want_open = true;
            else if (!menu_is_open && hovered && g.HoveredIdTimer >= 0.30f && g.MouseStationaryTimer >= 0.30f) // Hover to open (timer fallback)
                want_open = true;
            if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right) // Nav-Right to open
            {
                want_open = want_open_nav_init = true;
                NavMoveRequestCancel();
                SetNavCursorVisibleAfterMove();
            }
        } else {
            // Menu bar
            if (menu_is_open && pressed && menuset_is_open) // Click an open menu again to close it
            {
                want_close = true;
                want_open = menu_is_open = false;
            } else if (pressed || (hovered && menuset_is_open && !menu_is_open)) // First click to open, then hover to open others
            {
                want_open = true;
            } else if (g.NavId == id && g.NavMoveDir == ImGuiDir_Down) // Nav-Down to open
            {
                want_open = true;
                NavMoveRequestCancel();
            }
        }

        if (!enabled) // explicitly close if an open menu becomes disabled, facilitate users code a lot in pattern such as 'if (BeginMenu("options", has_object)) { ..use object.. }'
            want_close = true;
        if (want_close && IsPopupOpen(id, ImGuiPopupFlags_None))
            ClosePopupToLevel(g.BeginPopupStack.Size, true);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Openable | (menu_is_open ? ImGuiItemStatusFlags_Opened : 0));
        PopID();

        if (want_open && !menu_is_open && g.OpenPopupStack.Size > g.BeginPopupStack.Size) {
            // Don't reopen/recycle same menu level in the same frame if it is a different menu ID, first close the other menu and yield for a frame.
            OpenPopup(label);
        } else if (want_open) {
            menu_is_open = true;
            OpenPopup(label, ImGuiPopupFlags_NoReopen); // | (want_open_nav_init ? ImGuiPopupFlags_NoReopenAlwaysNavInit : 0));
        }

        if (menu_is_open) {
            ImGuiLastItemData last_item_in_parent = g.LastItemData;
            SetNextWindowPos(popup_pos, ImGuiCond_Always); // Note: misleading: the value will serve as reference for FindBestWindowPosForPopup(), not actual pos.
            PushStyleVar(ImGuiStyleVar_ChildRounding, style.PopupRounding); // First level will use _PopupRounding, subsequent will use _ChildRounding
            menu_is_open = BeginPopupMenuEx(id, label, window_flags); // menu_is_open may be 'false' when the popup is completely clipped (e.g. zero size display)
            PopStyleVar();
            if (menu_is_open) {
                // Implement what ImGuiPopupFlags_NoReopenAlwaysNavInit would do:
                // Perform an init request in the case the popup was already open (via a previous mouse hover)
                if (want_open && want_open_nav_init && !g.NavInitRequest) {
                    FocusWindow(g.CurrentWindow, ImGuiFocusRequestFlags_UnlessBelowModal);
                    NavInitWindow(g.CurrentWindow, false);
                }

                // Restore LastItemData so IsItemXXXX functions can work after BeginMenu()/EndMenu()
                // (This fixes using IsItemClicked() and IsItemHovered(), but IsItemHovered() also relies on its support for ImGuiItemFlags_NoWindowHoverableCheck)
                g.LastItemData = last_item_in_parent;
                if (g.HoveredWindow == window)
                    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
            }
        } else {
            g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
        }

        return menu_is_open;
    }
}