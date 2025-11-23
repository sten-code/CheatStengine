#pragma once

#include <CheatEngine/Core/Process.h>
#include <CheatEngine/Tools/MemoryScanner.h>
#include <CheatEngine/Tools/StructDissect.h>

#include <Windows.h>
#include <tlhelp32.h>

#include <vector>

struct State {
    std::vector<MODULEENTRY32> Modules;
    Process Process;
    std::vector<MemoryAddress> WatchAddresses;
    std::vector<Dissection> Dissections;
};