#pragma once

#include <Windows.h>
#include <tlhelp32.h>

#include <optional>
#include <vector>

namespace Utils
{

    [[nodiscard]] std::optional<MODULEENTRY32> GetModuleForAddress(uintptr_t address, const std::vector<MODULEENTRY32>& modules);

}