#include "Utils.h"

namespace Utils
{

    std::optional<MODULEENTRY32> GetModuleForAddress(uintptr_t address, const std::vector<MODULEENTRY32>& modules)
    {
        for (const MODULEENTRY32& entry : modules) {
            uintptr_t modBase = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
            uintptr_t modEnd = modBase + entry.modBaseSize;
            if (address >= modBase && address < modEnd) {
                return entry;
            }
        }
        return std::nullopt;
    }

}