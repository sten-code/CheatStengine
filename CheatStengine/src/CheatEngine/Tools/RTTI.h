#pragma once

#include <CheatEngine/Core/Process.h>

#include <Windows.h>

#include <cstdint>
#include <optional>
#include <string>

namespace RTTI {
    struct RTTICompleteObjectLocator {
        DWORD signature;
        DWORD offset;
        DWORD cdOffset;
        int typeDescriptor;
        int classDescriptor;
        int baseOffset;
    };

    struct TypeDescriptor {
        void* vtable;
        uint64_t ptr;
        char name[255];
    };

    std::string DemangleSymbol(const std::string& name);
    std::optional<std::string> GetRTTIName(Process& proc, uintptr_t address);
}