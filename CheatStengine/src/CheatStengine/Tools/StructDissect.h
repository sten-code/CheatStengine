#pragma once

#include <CheatStengine/Core/Process.h>

#include <Engine/Core/Core.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

enum class FieldType {
    UnsignedDecInt,
    SignedDecInt,
    HexInt,
    Float,
    Double,
    String,
    Pointer,
    StartEndPointer,
    Dissection
};

std::ostream& operator<<(std::ostream& os, const FieldType& type);
IMPLEMENT_FORMATTER(FieldType);

struct Pointer {
    uintptr_t Address;
};

struct StartEndPointer {
    uintptr_t Start, End;
};

using FieldValue = std::variant<
    int8_t, int16_t, int32_t, int64_t,
    uint8_t, uint16_t, uint32_t, uint64_t,
    Pointer, std::string, StartEndPointer,
    float, double>;

struct Field {
    struct Pointed {
        uintptr_t Address;
        size_t Size;
    };

    uintptr_t Offset;
    FieldType Type;
    size_t Size;
    std::string Name;
    std::vector<Field> Children;
    bool Explored = false;
    bool Expanded = false;

    [[nodiscard]] FieldValue ReadField(const Process& proc, uintptr_t baseAddress) const;
    bool WriteField(const Process& proc, uintptr_t baseAddress, const FieldValue& value);
    [[nodiscard]] Pointed GetPointedAddress(const Process& proc, uintptr_t baseAddress) const;
};

std::vector<Field> ExploreAddress(Process& proc, uintptr_t baseAddress, size_t size);

class Dissection {
public:
    Dissection(Process& proc, const std::string& name, uintptr_t address);

    [[nodiscard]] std::string_view GetName() const { return m_Name; }
    [[nodiscard]] uintptr_t GetAddress() const { return m_Address; }
    [[nodiscard]] Field& GetField() { return m_Field; }

private:
    std::string m_Name;
    uintptr_t m_Address;
    Field m_Field;
};