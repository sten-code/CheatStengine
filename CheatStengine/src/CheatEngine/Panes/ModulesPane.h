#pragma once

#include <CheatEngine/Panes/Pane.h>

class ModulesPane final : public Pane {
public:
    explicit ModulesPane(State& state);

    void Draw() override;

private:
    size_t m_SelectedIndex = -1;
};