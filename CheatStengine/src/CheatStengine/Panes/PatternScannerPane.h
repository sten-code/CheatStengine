#pragma once

#include "CheatStengine/Tools/PatternScanner.h"

#include <CheatStengine/Core/KeybindManager.h>
#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Panes/Pane.h>

struct PatternScanResult {
    std::string Pattern;
    std::vector<uintptr_t> Results;
    size_t SelectedIndex = 0;
};

class PatternScannerPane final : public Pane {
public:
    explicit PatternScannerPane(State& state, ModalManager& modalManager, KeybindManager& keybindManager);

    void Draw() override;

private:
    void DrawPatternScanResults(PatternScanResult& result);

    void ScanPatternModal(const std::string& name, const std::any& payload);

private:
    ModalManager& m_ModalManager;
    KeybindManager& m_KeybindManager;
    PatternScanner m_PatternScanner;
};