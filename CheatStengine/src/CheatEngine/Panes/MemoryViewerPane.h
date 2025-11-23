#pragma once

#include <CheatEngine/Panes/Pane.h>
#include <CheatEngine/UI/ImGui/ImGuiHex.h>

class MemoryViewerPane final : public Pane {
public:
    explicit MemoryViewerPane(State& state);

    void Draw() override;

private:
    ImGuiHexEditorState m_HexState;
};
