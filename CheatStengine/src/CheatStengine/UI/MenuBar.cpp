#define IMGUI_DEFINE_MATH_OPERATORS
#include "MenuBar.h"

#include <CheatStengine/Icons/FontAwesome6.h>
#include <CheatStengine/Icons/MaterialDesignIcons.h>
#include <CheatStengine/MainLayer.h>
#include <CheatStengine/Panes/DisassemblyPane.h>
#include <CheatStengine/UI/ImGui/Fonts.h>
#include <CheatStengine/UI/ImGui/Menu.h>
#include <CheatStengine/UI/TitleBar.h>
#include <Engine/Core/Application.h>

#include <imgui.h>
#include <imgui_stdlib.h>

static ImRect RectOffset(const ImRect& rect, float x, float y)
{
    ImRect result = rect;
    result.Min.x += x;
    result.Min.y += y;
    result.Max.x += x;
    result.Max.y += y;
    return result;
}

MenuBar::MenuBar(MainLayer& mainLayer)
    : m_MainLayer(mainLayer)
    , m_ModalManager(mainLayer.m_ModalManager)
    , m_KeybindManager(mainLayer.m_KeybindManager)
{
    m_ModalManager.RegisterModal("Allocate Memory", BIND_FN(MenuBar::AllocateMemoryModal));
    m_KeybindManager.RegisterKeybind(
        "Open Process",
        "Opens the list of processes to attach to",
        "Global", ImGuiMod_Ctrl | ImGuiKey_O,
        [this]() {
            m_ModalManager.OpenModal("Open Process");
        });
    m_KeybindManager.RegisterKeybind("Allocate Memory",
        "Opens the allocate memory modal",
        "Global", ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_M,
        [this]() {
            m_ModalManager.OpenModal("Allocate Memory");
        });
}

void MenuBar::Draw()
{
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
    ImRect menuBarRect = { ImGui::GetCursorPos(), { ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x, TitleBar::TITLEBAR_HEIGHT } };
    ImGui::BeginGroup();
    if (BeginMenubar(menuBarRect)) {
        if (ImGui::BeginMenu("File")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            Fonts::Push(Fonts::Type::FontAwesome6);
            if (ImGui::RoundedMenuItemEx("Open Process", ICON_FA_WINDOW_MAXIMIZE, m_KeybindManager.GetKeybindString("Open Process").c_str())) {
                m_ModalManager.OpenModal("Open Process");
            }
            Fonts::Pop();

            ImGui::PopStyleVar();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            for (const std::unique_ptr<Pane>& pane : m_MainLayer.GetPanes()) {
                if (ImGui::RoundedMenuItem(pane->GetName().c_str(), nullptr, pane->IsOpen())) {
                    pane->SetOpen(!pane->IsOpen());
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::RoundedMenuItemEx("Allocate Memory", ICON_MDI_PLUS_BOX, m_KeybindManager.GetKeybindString("Allocate Memory").c_str())) {
                m_ModalManager.OpenModal("Allocate Memory");
            }
            ImGui::EndMenu();
        }
    }
    EndMenubar();
    ImGui::EndGroup();
}

bool MenuBar::BeginMenubar(const ImRect& barRectangle)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }
    /*if (!(window->Flags & ImGuiWindowFlags_MenuBar))
        return false;*/

    IM_ASSERT(!window->DC.MenuBarAppending);
    ImGui::BeginGroup(); // Backup position on layer 0 // FIXME: Misleading to use a group for that backup/restore
    ImGui::PushID("##menubar");

    const ImVec2 padding = window->WindowPadding;

    // We don't clip with current window clipping rectangle as it is already set to the area below. However we clip with window full rect.
    // We remove 1 worth of rounding to Max.x to that text in long menus and small windows don't tend to display over the lower-right rounded area, which looks particularly glitchy.
    ImRect barRect = RectOffset(barRectangle, 0.0f, padding.y); // window->MenuBarRect();
    ImRect clipRect(IM_ROUND(ImMax(window->Pos.x, barRect.Min.x + window->WindowBorderSize + window->Pos.x - 10.0f)), IM_ROUND(barRect.Min.y + window->WindowBorderSize + window->Pos.y),
        IM_ROUND(ImMax(barRect.Min.x + window->Pos.x, barRect.Max.x - ImMax(window->WindowRounding, window->WindowBorderSize))), IM_ROUND(barRect.Max.y + window->Pos.y));

    clipRect.ClipWith(window->OuterRectClipped);
    ImGui::PushClipRect(clipRect.Min, clipRect.Max, false);

    // We overwrite CursorMaxPos because BeginGroup sets it to CursorPos (essentially the .EmitItem hack in EndMenuBar() would need something analogous here, maybe a BeginGroupEx() with flags).
    window->DC.CursorPos = window->DC.CursorMaxPos = ImVec2(barRect.Min.x + window->Pos.x, barRect.Min.y + window->Pos.y);
    window->DC.LayoutType = ImGuiLayoutType_Horizontal;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;
    window->DC.MenuBarAppending = true;
    ImGui::AlignTextToFramePadding();
    return true;
}

void MenuBar::EndMenubar()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;
    ImGuiContext& g = *GImGui;

    // Nav: When a move request within one of our child menu failed, capture the request to navigate among our siblings.
    if (ImGui::NavMoveRequestButNoResultYet() && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) && (g.NavWindow->Flags & ImGuiWindowFlags_ChildMenu)) {
        // Try to find out if the request is for one of our child menu
        ImGuiWindow* nav_earliest_child = g.NavWindow;
        while (nav_earliest_child->ParentWindow && (nav_earliest_child->ParentWindow->Flags & ImGuiWindowFlags_ChildMenu))
            nav_earliest_child = nav_earliest_child->ParentWindow;
        if (nav_earliest_child->ParentWindow == window && nav_earliest_child->DC.ParentLayoutType == ImGuiLayoutType_Horizontal && (g.NavMoveFlags & ImGuiNavMoveFlags_Forwarded) == 0) {
            // To do so we claim focus back, restore NavId and then process the movement request for yet another frame.
            // This involve a one-frame delay which isn't very problematic in this situation. We could remove it by scoring in advance for multiple window (probably not worth bothering)
            const ImGuiNavLayer layer = ImGuiNavLayer_Menu;
            IM_ASSERT(window->DC.NavLayersActiveMaskNext & (1 << layer)); // Sanity check
            ImGui::FocusWindow(window);
            ImGui::SetNavID(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);
            // g.NavDisableHighlight = true; // Hide highlight for the current frame so we don't see the intermediary selection.
            // g.NavDisableMouseHover = g.NavMousePosDirty = true;
            ImGui::NavMoveRequestForward(g.NavMoveDir, g.NavMoveClipDir, g.NavMoveFlags, g.NavMoveScrollFlags); // Repeat
        }
    }

    IM_MSVC_WARNING_SUPPRESS(6011); // Static Analysis false positive "warning C6011: Dereferencing NULL pointer 'window'"
    // IM_ASSERT(window->Flags & ImGuiWindowFlags_MenuBar); // NOTE(Yan): Needs to be commented out because Jay
    IM_ASSERT(window->DC.MenuBarAppending);
    ImGui::PopClipRect();
    ImGui::PopID();
    window->DC.MenuBarOffset.x = window->DC.CursorPos.x - window->Pos.x; // Save horizontal position so next append can reuse it. This is kinda equivalent to a per-layer CursorPos.
    g.GroupStack.back().EmitItem = false;
    ImGui::EndGroup(); // Restore position on layer 0
    window->DC.LayoutType = ImGuiLayoutType_Vertical;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
    window->DC.MenuBarAppending = false;
}

void MenuBar::AllocateMemoryModal(const std::string& name, const std::any& payload) const
{
    static std::string sizeInput;
    bool allocate = false;

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2 { 0.5f, 0.5f });
    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Text("How much memory do you want to allocate?");
        if (ImGui::IsWindowAppearing()) {
            sizeInput = "4096";
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Size", &sizeInput, ImGuiInputTextFlags_EnterReturnsTrue)) {
            allocate = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
            allocate = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (allocate) {
        try {
            size_t size = std::stoull(sizeInput);
            uintptr_t address = m_MainLayer.m_State.Process.Allocate(size, PAGE_READWRITE);
            if (address == 0) {
                ERR("Failed to allocate memory");
                return;
            }
            DisassemblyPane* pane = m_MainLayer.GetPane<DisassemblyPane>();
            pane->FocusAddress(address);
            pane->ForceFocus();
        } catch (const std::exception&) {
        }
    }
}
