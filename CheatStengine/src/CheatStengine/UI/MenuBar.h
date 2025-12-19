#pragma once

#include <CheatStengine/Core/ModalManager.h>

#include <imgui_internal.h>

class MainLayer;

class MenuBar {
public:
    explicit MenuBar(MainLayer& mainLayer, ModalManager& modalManager);

    void Draw();

private:
    bool BeginMenubar(const ImRect& barRectangle);
    void EndMenubar();

private:
    MainLayer& m_MainLayer;
    ModalManager& m_ModalManager;
};