#pragma once

#include <CheatStengine/Core/ModalManager.h>

#include <CheatStengine/Core/KeybindManager.h>
#include <imgui_internal.h>

class MainLayer;

class MenuBar {
public:
    explicit MenuBar(MainLayer& mainLayer);

    void Draw();

private:
    bool BeginMenubar(const ImRect& barRectangle);
    void EndMenubar();

    void AllocateMemoryModal(const std::string& name, const std::any& payload) const;

private:
    MainLayer& m_MainLayer;
    ModalManager& m_ModalManager;
    KeybindManager& m_KeybindManager;
};