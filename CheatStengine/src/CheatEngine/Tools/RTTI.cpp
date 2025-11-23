#include "RTTI.h"

#include <CheatEngine/Utils.h>

#include <DbgHelp.h>

namespace RTTI {
    std::string DemangleSymbol(const std::string& name)
    {
        std::string demangledName = std::string(1024, '\0');
        std::string mangledName = name;

        if (mangledName.starts_with(".?AV")) {
            mangledName = "?" + mangledName.substr(4);
        }

        DWORD length = UnDecorateSymbolName(mangledName.c_str(), demangledName.data(), demangledName.capacity(), UNDNAME_COMPLETE);
        if (!length) {
            return mangledName;
        }
        demangledName.resize(length);

        if (demangledName.starts_with(" ??")) {
            demangledName = demangledName.substr(4);
        }

        return demangledName;
    }

    std::optional<std::string> GetRTTIName(Process& proc, uintptr_t address)
    {
        uintptr_t vtable = proc.Read<uintptr_t>(address);
        if (!proc.IsAddressReadable(vtable - sizeof(uintptr_t))) {
            return std::nullopt;
        }

        uintptr_t colAddr = proc.Read<uintptr_t>(vtable - sizeof(uintptr_t));
        if (!proc.IsAddressReadable(colAddr)) {
            return std::nullopt;
        }

        RTTICompleteObjectLocator col = proc.Read<RTTICompleteObjectLocator>(colAddr);
        if (col.typeDescriptor == 0) {
            return std::nullopt;
        }

        std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(colAddr, proc.GetModuleEntries());
        if (!modEntry) {
            return std::nullopt;
        }

        uintptr_t typeInfoAddr = reinterpret_cast<uintptr_t>(modEntry->modBaseAddr) + col.typeDescriptor;
        if (!proc.IsAddressReadable(typeInfoAddr)) {
            return std::nullopt;
        }

        TypeDescriptor typeInfo = proc.Read<TypeDescriptor>(typeInfoAddr);
        // INFO("Found RTTI type: {}", typeInfo.name);
        return DemangleSymbol(typeInfo.name);
    }
}