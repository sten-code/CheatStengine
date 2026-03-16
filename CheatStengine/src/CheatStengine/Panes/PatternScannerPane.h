#pragma once

#include <CheatStengine/Panes/Pane.h>

class PatternScannerPane final : public Pane {
public:
    explicit PatternScannerPane(State& state);

    void Draw() override;

private:
};