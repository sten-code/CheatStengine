#pragma once

#include <CheatStengine/Panes/Pane.h>

class ModulesPane final : public Pane {
public:
    explicit ModulesPane(State& state);

    void Draw(double deltaTime) override;

private:
    size_t m_SelectedIndex = -1;
};