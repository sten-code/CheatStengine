#include "PatternScannerPane.h"

#include <CheatStengine/Icons/MaterialDesignIcons.h>

#include <imgui.h>

PatternScannerPane::PatternScannerPane(State& state)
    : Pane(ICON_MDI_REGEX " Pattern Scanner", state)
{
}

void PatternScannerPane::Draw()
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    ImGui::End();
}