#include "StructDissect.h"

#include <CheatEngine/Core/Process.h>
#include <CheatEngine/Tools/RTTI.h>

#include <imgui.h>

static bool IsPrintableChar(uint8_t c) { return (c >= 32 && c <= 126) || c == '\r' || c == '\n' || c == '\t'; }

std::vector<Field> ExploreAddress(Process& proc, uintptr_t baseAddress, size_t size)
{
    if (!proc.IsAddressReadable(baseAddress)) {
        return {};
    }

    std::vector<uint8_t> buffer = proc.ReadBuffer(baseAddress, size);
    if (buffer.empty()) {
        return {};
    }

    std::vector<Field> fields;

    // helper to safely read u64 from unaligned buffer
    auto read_u64 = [&](size_t off) -> uint64_t {
        uint64_t v = 0;
        if (off + sizeof(v) <= buffer.size()) {
            std::memcpy(&v, &buffer[off], sizeof(v));
        }
        return v;
    };

    for (size_t offset = 0; offset < buffer.size();) {
        uintptr_t address = baseAddress + offset;

        // Check for Start/End pointer pair (two consecutive uintptr_t / u64)
        // if (address % 8 == 0 && offset + 16 <= buffer.size()) {
        //     uint64_t start = read_u64(offset);
        //     uint64_t end = read_u64(offset + 8);
        //     // basic sanity checks: aligned, readable, start < end, and reasonable size
        //     if (start < end
        //         && proc.IsAddressReadable(start)
        //         && proc.IsAddressReadable(end - 1)
        //         && (end - start) >= 4
        //         && (end - start) <= 0x1000) {
        //         INFO("Found StartEndPointer [0x{:X} - 0x{:X}] at offset 0x{:X}", start, end, offset);
        //         fields.emplace_back(offset, FieldType::StartEndPointer, 16);
        //         offset += 16;
        //         continue;
        //     }
        // }

        // Check if it's a valid pointer
        if (address % alignof(uintptr_t) == 0
            && offset + sizeof(uintptr_t) <= buffer.size()) {
            uint64_t pointed = read_u64(offset);
            if (proc.IsAddressReadable(pointed)) {
                std::string name = RTTI::GetRTTIName(proc, pointed).value_or("Pointer");
                // INFO("Found pointer to 0x{:X} ({}) at offset 0x{:X}", pointed, name, offset);
                fields.emplace_back(offset, FieldType::Pointer, 8, name);
                offset += 8;
                continue;
            }
        }

        // Check for printable string
        if (IsPrintableChar(buffer[offset])) {
            size_t strLen = 0;
            while (offset + strLen < buffer.size() && IsPrintableChar(buffer[offset + strLen])) {
                strLen++;
            }
            if (strLen >= 4) {
                INFO("Found string of length {} at offset 0x{:X}: {}", strLen, offset, std::string(reinterpret_cast<char*>(&buffer[offset]), strLen));
                fields.emplace_back(offset, FieldType::String, strLen);
                offset += strLen;
                continue;
            }
        }

        if (address % alignof(double) == 0 && offset + sizeof(double) <= buffer.size()) {
            uint64_t bits64 = 0;
            std::memcpy(&bits64, &buffer[offset], sizeof(bits64));
            uint64_t exp64 = (bits64 >> 52) & 0x7FF;
            if (exp64 != 0 && exp64 != 0x7FF) {
                double d;
                std::memcpy(&d, &bits64, sizeof(d));
                // finite and within a reasonable range
                if (std::isfinite(d) && std::fabs(d) <= 1e300) {
                    fields.emplace_back(offset, FieldType::Double, 8);
                    offset += 8;
                    continue;
                }
            }
        }

        if (address % alignof(float) == 0 && offset + sizeof(float) <= buffer.size()) {
            uint32_t bits32 = 0;
            std::memcpy(&bits32, &buffer[offset], sizeof(bits32));
            uint32_t exp32 = (bits32 >> 23) & 0xFF;
            if (exp32 != 0 && exp32 != 0xFF) {
                float f;
                std::memcpy(&f, &bits32, sizeof(f));
                // finite and within a reasonable range
                if (std::isfinite(f) && std::fabs(f) <= 1e8f) {
                    fields.emplace_back(offset, FieldType::Float, 4);
                    offset += 4;
                    continue;
                }
            }
        }

        // Fallback integers
        if (address % alignof(uint32_t) == 0) {
            fields.emplace_back(offset, FieldType::SignedDecInt, 4);
            offset += 4;
            continue;
        }
        if (address % alignof(uint16_t) == 0) {
            fields.emplace_back(offset, FieldType::SignedDecInt, 2);
            offset += 2;
            continue;
        }

        fields.emplace_back(offset, FieldType::HexInt, 1);
        offset += 1;
    }
    return fields;
}

std::ostream& operator<<(std::ostream& os, const FieldType& type)
{
    switch (type) {
        case FieldType::HexInt: os << "Hex"; break;
        case FieldType::SignedDecInt: os << "Signed Dec"; break;
        case FieldType::UnsignedDecInt: os << "Unsigned Dec"; break;
        case FieldType::Pointer: os << "Pointer"; break;
        case FieldType::String: os << "String"; break;
        case FieldType::Float: os << "Float"; break;
        case FieldType::Double: os << "Double"; break;
        case FieldType::StartEndPointer: os << "Start/End Pointer"; break;
        case FieldType::Dissection: os << "Dissection"; break;
    }
    return os;
}

FieldValue Field::ReadField(const Process& proc, uintptr_t baseAddress) const
{
    switch (Type) {
        case FieldType::HexInt:
        case FieldType::UnsignedDecInt:
            switch (Size) {
                case 1: return proc.Read<uint8_t>(baseAddress + Offset);
                case 2: return proc.Read<uint16_t>(baseAddress + Offset);
                case 4: return proc.Read<uint32_t>(baseAddress + Offset);
                case 8: return proc.Read<uint64_t>(baseAddress + Offset);
                default: return {};
            }
        case FieldType::SignedDecInt:
            switch (Size) {
                case 1: return proc.Read<int8_t>(baseAddress + Offset);
                case 2: return proc.Read<int16_t>(baseAddress + Offset);
                case 4: return proc.Read<int32_t>(baseAddress + Offset);
                case 8: return proc.Read<int64_t>(baseAddress + Offset);
                default: return {};
            }
        case FieldType::Pointer: return Pointer { proc.Read<uintptr_t>(baseAddress + Offset) };
        case FieldType::String: return proc.ReadString(baseAddress + Offset, Size);
        case FieldType::Float: return proc.Read<float>(baseAddress + Offset);
        case FieldType::Double: return proc.Read<double>(baseAddress + Offset);
        case FieldType::StartEndPointer: return proc.Read<StartEndPointer>(baseAddress + Offset);
        case FieldType::Dissection: break;
    }
    return {};
}

bool Field::WriteField(const Process& proc, uintptr_t baseAddress, const FieldValue& value)
{
    return std::visit(overloads {
                          [&](int8_t v) { return proc.Write<int8_t>(baseAddress + Offset, v); },
                          [&](int16_t v) { return proc.Write<int16_t>(baseAddress + Offset, v); },
                          [&](int32_t v) { return proc.Write<int32_t>(baseAddress + Offset, v); },
                          [&](int64_t v) { return proc.Write<int64_t>(baseAddress + Offset, v); },
                          [&](uint8_t v) { return proc.Write<uint8_t>(baseAddress + Offset, v); },
                          [&](uint16_t v) { return proc.Write<uint16_t>(baseAddress + Offset, v); },
                          [&](uint32_t v) { return proc.Write<uint32_t>(baseAddress + Offset, v); },
                          [&](uint64_t v) { return proc.Write<uint64_t>(baseAddress + Offset, v); },
                          [&](Pointer ptr) { return proc.Write<uintptr_t>(baseAddress + Offset, ptr.Address); },
                          [&](const std::string& str) {
                              Size = str.size();
                              return proc.WriteBuffer(baseAddress + Offset, str.data(), str.size());
                          },
                          [&](StartEndPointer ptr) { return proc.Write<StartEndPointer>(baseAddress + Offset, ptr); },
                          [&](float v) { return proc.Write<float>(baseAddress + Offset, v); },
                          [&](double v) { return proc.Write<double>(baseAddress + Offset, v); } },
        value);
}

Field::Pointed Field::GetPointedAddress(const Process& proc, uintptr_t baseAddress) const
{
    if (Type == FieldType::StartEndPointer) {
        FieldValue value = ReadField(proc, baseAddress);
        StartEndPointer pair = std::get<StartEndPointer>(value);
        uintptr_t pointedAddress = pair.Start;
        size_t size = pair.End - pair.Start;
        return { pointedAddress, size };
    }

    if (Type == FieldType::Pointer) {
        FieldValue value = ReadField(proc, baseAddress);
        uintptr_t pointedAddress = std::get<Pointer>(value).Address;
        return { pointedAddress, 0x400 };
    }

    return { baseAddress, 0x400 };
}

Dissection::Dissection(Process& proc, const std::string& name, uintptr_t address)
    : m_Name(name), m_Address(address)
{
    std::vector<Field> fields = ExploreAddress(proc, address, 0x400);
    m_Field = Field {
        0,
        FieldType::Dissection,
        0x400,
        name,
        fields,
        true,
        true
    };
}
