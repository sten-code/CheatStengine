#include "DebugPane.h"
#include <CheatStengine/MainLayer.h>
#include <CheatStengine/UI/ImGui/Menu.h>

#include <IconsMaterialDesignIcons.h>
#include <imgui.h>

DebugPane::DebugPane(State& state)
    : Pane(ICON_MDI_BUG " Debug", state)
{
}

void DebugPane::Draw(double deltaTime)
{
    double fps = 1.0 / deltaTime;

    ImGui::Begin(m_Name.c_str(), &m_Open);

    ImGui::Text("FPS: %f", fps);
    ImGui::Text("Delta Time: %f ms", deltaTime * 1000.0);


    ImGui::End();
}