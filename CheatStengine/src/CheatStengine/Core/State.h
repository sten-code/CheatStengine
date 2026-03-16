#pragma once

#include <CheatStengine/Core/Process.h>
#include <CheatStengine/Tools/MemoryScanner.h>
#include <CheatStengine/Tools/StructDissect.h>

#include <Windows.h>
#include <Tlhelp32.h>

#include <vector>

struct PatternScan;

struct State {
    std::vector<MODULEENTRY32> Modules;
    Process Process;
    std::vector<MemoryAddress> WatchAddresses;
    std::vector<Dissection> Dissections;
    std::vector<PatternScan> PatternScanResults;
};