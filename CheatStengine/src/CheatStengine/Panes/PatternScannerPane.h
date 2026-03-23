#pragma once

#include "CheatStengine/Tools/PatternScanner.h"

#include <CheatStengine/Core/KeybindManager.h>
#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/MainLayer.h>
#include <CheatStengine/Panes/Pane.h>

struct PatternScan {
    struct Result {
        uintptr_t Address;
        std::string Instruction;
    };
    std::string Pattern;
    std::vector<Result> Results;
    size_t SelectedIndex = 0;
};

class PatternScannerPane final : public Pane {
public:
    explicit PatternScannerPane(State& state, MainLayer& mainLayer);

    void Draw(double deltaTime) override;

    void PerformPatternScan(std::string_view pattern, const MODULEENTRY32& moduleEntry) const;

private:
    void DrawPatternScanResults(PatternScan& patternScan) const;

    void ScanPatternModal(const std::string& name, const std::any& payload);

private:
    MainLayer& m_MainLayer;
    ModalManager& m_ModalManager;
    KeybindManager& m_KeybindManager;
    PatternScanner m_PatternScanner;
};