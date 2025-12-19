#pragma once

#include <CheatStengine/Panes/Pane.h>
#include <CheatStengine/UI/ImGui/ImGuiHex.h>

class MemoryViewerPane final : public Pane {
public:
    explicit MemoryViewerPane(State& state);

    void Draw() override;

private:
    ImGuiHexEditorState m_HexState;
};
