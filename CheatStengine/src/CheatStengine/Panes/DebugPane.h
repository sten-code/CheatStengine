#pragma once

#include <CheatStengine/Panes/Pane.h>

class DebugPane final : public Pane {
public:
    explicit DebugPane(State& state);

    void Draw(double deltaTime) override;

};