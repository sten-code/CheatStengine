#pragma once

#include <CheatStengine/Core/Process.h>

#include <Windows.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

enum class ValueType {
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    COUNT
};

std::string ValueTypeToString(ValueType type);
size_t GetTypeSize(ValueType type);

struct MemoryAddress {
    std::string Description;
    uintptr_t Address;
    ValueType Type;

    [[nodiscard]] std::string ReadValue(const Process& proc) const;
    bool WriteValue(const Process& proc, const std::string& value) const;
};

using ScanValue = std::variant<
    int8_t, int16_t, int32_t, int64_t,
    uint8_t, uint16_t, uint32_t, uint64_t,
    float, double>;

std::string ScanValueToString(const ScanValue& value, bool isHex);

struct ScannedAddress {
    uintptr_t Address;
    ScanValue PreviousValue;
    ScanValue FirstValue;

    ScannedAddress(uintptr_t addr, ScanValue value)
        : Address(addr)
        , PreviousValue(value)
        , FirstValue(value) {}

    [[nodiscard]] ScanValue ReadValue(const Process& proc) const;
};

enum class ScanType {
    ExactValue,
    BiggerThan,
    SmallerThan,
    ValueBetween,
    UnknownInitialValue,
    COUNT
};

class MemoryScanner {
public:
    explicit MemoryScanner(const Process& proc)
        : m_Process(proc)
    {
        m_CurrentResults.reserve(100000);
    }

    bool FirstScan(ValueType valueType, ScanType scanType,
        uintptr_t minAddress,
        uintptr_t maxAddress,
        const std::string& lowerValue = "",
        const std::string& upperValue = "");
    void NextScan(ValueType valueType, ScanType scanType,
        uintptr_t minAddress,
        uintptr_t maxAddress,
        const std::string& lowerValue = "",
        const std::string& upperValue = "");

    [[nodiscard]] const std::vector<ScannedAddress>& GetResults() const { return m_CurrentResults; }
    [[nodiscard]] std::mutex& GetMutex() { return m_Mutex; }
    [[nodiscard]] bool IsScanning() const { return m_Scanning; }

private:
    template <typename T>
    static std::vector<uint8_t> ValueToBytes(T value)
    {
        std::vector<uint8_t> bytes(sizeof(T));
        memcpy(bytes.data(), &value, sizeof(T));
        return bytes;
    }
    static bool CompareValues(ScanType scanType, const ScanValue& value, const ScanValue& targetValue1, const ScanValue& targetValue2);
    static bool CompareByteArrays(ScanType scanType, ValueType valueType, uint8_t* data, const std::vector<uint8_t>& target1, const std::vector<uint8_t>& target2);

    static std::vector<uint8_t> StringToBytes(const std::string& str, ValueType type);
    static ScanValue StringToValue(const std::string& str, ValueType type);
    static bool IsValidMemoryRegion(const MEMORY_BASIC_INFORMATION& memInfo);

private:
    const Process& m_Process;

    std::vector<ScannedAddress> m_CurrentResults;
    std::mutex m_Mutex;
    bool m_Scanning = false;
};
