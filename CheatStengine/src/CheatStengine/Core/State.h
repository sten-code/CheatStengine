#pragma once

#include <CheatStengine/Process/Process.h>
#include <CheatStengine/Tools/MemoryScanner.h>
#include <CheatStengine/Tools/StructDissect.h>

#include <Tlhelp32.h>
#include <Windows.h>

#include <vector>

struct PatternScan;

struct State {
    std::vector<MODULEENTRY32> Modules;
    std::unique_ptr<Process> Process;
    std::vector<MemoryAddress> WatchAddresses;
    std::vector<Dissection> Dissections;
    std::vector<PatternScan> PatternScanResults;
};