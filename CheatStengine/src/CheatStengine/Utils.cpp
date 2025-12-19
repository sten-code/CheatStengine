#include "Utils.h"

#include <cstdint>

namespace Utils {

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

    std::string EscapeString(const std::string& str)
    {
        std::string escaped;
        escaped.reserve(str.size() * 2);
        static const char* hex = "0123456789ABCDEF";
        for (uint8_t c : str) {
            switch (c) {
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                case '\\': escaped += "\\\\"; break;
                case '\"': escaped += "\\\""; break;
                default: {
                    if (c >= 32 && c <= 126) {
                        escaped.push_back(static_cast<char>(c));
                    } else {
                        escaped += "\\x";
                        escaped += hex[c >> 4];
                        escaped += hex[c & 0xF];
                    }
                }
            }
        }
        return escaped;
    }

}