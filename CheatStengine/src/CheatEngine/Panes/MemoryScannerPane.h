#pragma once

#include <CheatEngine/Tools/MemoryScanner.h>
#include <CheatEngine/Panes/Pane.h>

#include <CheatEngine/MainLayer.h>
#include <cstdint>
#include <future>
#include <string>

class MainLayer;

enum class ModuleFilter {
    Any,
    Specific
};

class MemoryScannerPane final : public Pane {
public:
    explicit MemoryScannerPane(State& state);

    void Draw() override;

    void StartFirstScan();

private:
    // Scan Options
    bool m_PendingScanReset = false;
    std::string m_ScanInputLower;
    std::string m_ScanInputUpper;
    bool m_IsInputHex = false;
    ScanType m_ScanType = ScanType::ExactValue;
    ValueType m_ValueType = ValueType::Int32;
    uintptr_t m_MinAddress = 0;
    uintptr_t m_MaxAddress = 0x7FFFFFFFFFFF;
    ModuleFilter m_ModuleFilter = ModuleFilter::Any;
    int m_SelectedModuleIndex = 0; // Index in the modules vector

    // Scan Results
    std::unique_ptr<MemoryScanner> m_Scanner;
};
