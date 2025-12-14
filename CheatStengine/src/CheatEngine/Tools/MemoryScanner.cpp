#include "MemoryScanner.h"

#include <Engine/Core/Core.h>

#include <future>

std::string ValueTypeToString(ValueType type)
{
    switch (type) {
        case ValueType::Int8: return "Int8";
        case ValueType::Int16: return "Int16";
        case ValueType::Int32: return "Int32";
        case ValueType::Int64: return "Int64";
        case ValueType::UInt8: return "UInt8";
        case ValueType::UInt16: return "UInt16";
        case ValueType::UInt32: return "UInt32";
        case ValueType::UInt64: return "UInt64";
        case ValueType::Float: return "Float";
        case ValueType::Double: return "Double";
        default: return "Unknown";
    }
}

ScanValue ScannedAddress::ReadValue(const Process& proc) const
{
    overloads visitor = {
        [this, &proc]<typename T>(T) -> ScanValue { return proc.Read<T>(Address); },
    };
    return std::visit(visitor, PreviousValue);
}

size_t GetTypeSize(ValueType type)
{
    switch (type) {
        case ValueType::Int8:
        case ValueType::UInt8: return 1;
        case ValueType::Int16:
        case ValueType::UInt16: return 2;
        case ValueType::Int32:
        case ValueType::UInt32:
        case ValueType::Float: return 4;
        case ValueType::Int64:
        case ValueType::UInt64:
        case ValueType::Double: return 8;
        default: return 0;
    }
}

std::string MemoryAddress::ReadValue(const Process& proc) const
{
    switch (Type) {
        case ValueType::Int8: return std::to_string(proc.Read<int8_t>(Address));
        case ValueType::Int16: return std::to_string(proc.Read<int16_t>(Address));
        case ValueType::Int32: return std::to_string(proc.Read<int32_t>(Address));
        case ValueType::Int64: return std::to_string(proc.Read<int64_t>(Address));
        case ValueType::UInt8: return std::to_string(proc.Read<uint8_t>(Address));
        case ValueType::UInt16: return std::to_string(proc.Read<uint16_t>(Address));
        case ValueType::UInt32: return std::to_string(proc.Read<uint32_t>(Address));
        case ValueType::UInt64: return std::to_string(proc.Read<uint64_t>(Address));
        case ValueType::Float: return std::to_string(proc.Read<float>(Address));
        case ValueType::Double: return std::to_string(proc.Read<double>(Address));
        default: return "Unknown";
    }
}

bool MemoryAddress::WriteValue(const Process& proc, const std::string& value) const
{
    switch (Type) {
        case ValueType::Int8: return proc.Write<int8_t>(Address, static_cast<int8_t>(std::stoi(value)));
        case ValueType::Int16: return proc.Write<int16_t>(Address, static_cast<int16_t>(std::stoi(value)));
        case ValueType::Int32: return proc.Write<int32_t>(Address, static_cast<int32_t>(std::stoi(value)));
        case ValueType::Int64: return proc.Write<int64_t>(Address, static_cast<int64_t>(std::stoll(value)));
        case ValueType::UInt8: return proc.Write<uint8_t>(Address, static_cast<uint8_t>(std::stoul(value)));
        case ValueType::UInt16: return proc.Write<uint16_t>(Address, static_cast<uint16_t>(std::stoul(value)));
        case ValueType::UInt32: return proc.Write<uint32_t>(Address, static_cast<uint32_t>(std::stoul(value)));
        case ValueType::UInt64: return proc.Write<uint64_t>(Address, static_cast<uint64_t>(std::stoull(value)));
        case ValueType::Float: return proc.Write<float>(Address, std::stof(value));
        case ValueType::Double: return proc.Write<double>(Address, std::stod(value));
        default: return false;
    }
}

std::string ScanValueToString(const ScanValue& value, bool isHex)
{
    const overloads data = {
        [&](int8_t v) { return isHex ? std::format("0x{:X}", static_cast<uint8_t>(v)) : std::to_string(v); },
        [&](int16_t v) { return isHex ? std::format("0x{:X}", static_cast<uint16_t>(v)) : std::to_string(v); },
        [&](int32_t v) { return isHex ? std::format("0x{:X}", static_cast<uint32_t>(v)) : std::to_string(v); },
        [&](int64_t v) { return isHex ? std::format("0x{:X}", static_cast<uint64_t>(v)) : std::to_string(v); },
        [&](uint8_t v) { return isHex ? std::format("0x{:X}", v) : std::to_string(v); },
        [&](uint16_t v) { return isHex ? std::format("0x{:X}", v) : std::to_string(v); },
        [&](uint32_t v) { return isHex ? std::format("0x{:X}", v) : std::to_string(v); },
        [&](uint64_t v) { return isHex ? std::format("0x{:X}", v) : std::to_string(v); },
        [&](float v) { return std::to_string(v); },
        [&](double v) { return std::to_string(v); },
    };
    return std::visit(data, value);
}

bool MemoryScanner::FirstScan(ValueType valueType, ScanType scanType, uintptr_t minAddress, uintptr_t maxAddress, const std::string& lowerValue, const std::string& upperValue)
{
    if (m_Scanning) {
        return false;
    }
    std::vector<uint8_t> targetValue1, targetValue2;
    if (scanType != ScanType::UnknownInitialValue) {
        if (lowerValue.empty()) {
            return false;
        }
        targetValue1 = StringToBytes(lowerValue, valueType);
    }
    if (scanType == ScanType::ValueBetween) {
        if (upperValue.empty()) {
            return false;
        }
        targetValue2 = StringToBytes(upperValue, valueType);
    }

    std::thread([this](ValueType valueType, ScanType scanType,
                    uintptr_t minAddress, uintptr_t maxAddress,
                    const std::vector<uint8_t>& targetValue1, const std::vector<uint8_t>& targetValue2) {
        auto start = std::chrono::high_resolution_clock::now();

        {
            std::lock_guard lock(m_Mutex);
            m_CurrentResults.clear();
        }

        m_Scanning = true;
        std::vector<std::pair<uintptr_t, size_t>> regions;
        regions.reserve(256);
        uintptr_t currentAddress = minAddress;
        while (currentAddress < maxAddress) {
            std::optional<MEMORY_BASIC_INFORMATION> mbi = m_Process.Query(currentAddress);
            if (!mbi) {
                currentAddress += 0x1000;
                continue;
            }

            uintptr_t base = reinterpret_cast<uintptr_t>(mbi->BaseAddress);
            if (!IsValidMemoryRegion(*mbi)) {
                currentAddress = base + mbi->RegionSize;
                continue;
            }

            regions.emplace_back(base, mbi->RegionSize);
            currentAddress = base + mbi->RegionSize;
        }

        INFO("Found {} regions to scan", regions.size());
        INFO("Took: {}ms", std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count());
        start = std::chrono::high_resolution_clock::now();
        size_t typeSize = GetTypeSize(valueType);

        std::vector<std::future<std::vector<ScannedAddress>>> futures;
        futures.reserve(regions.size());

        for (const auto& [address, size] : regions) {
            futures.emplace_back(std::async(std::launch::async, [this, address, size, typeSize, scanType, valueType, &targetValue1, &targetValue2]() -> std::vector<ScannedAddress> {
                std::vector<uint8_t> buffer(size);
                if (!m_Process.ReadBuffer(address, buffer.data(), size)) {
                    return {};
                }

                std::vector<ScannedAddress> localResults;
                for (size_t offset = 0; offset < size; offset += typeSize) {
                    uintptr_t currentAddress = address + offset;

                    if (CompareByteArrays(scanType, valueType, buffer.data() + offset, targetValue1, targetValue2)) {
                        switch (valueType) {
                            case ValueType::Int8: localResults.emplace_back(currentAddress, *reinterpret_cast<int8_t*>(buffer.data() + offset)); break;
                            case ValueType::Int16: localResults.emplace_back(currentAddress, *reinterpret_cast<int16_t*>(buffer.data() + offset)); break;
                            case ValueType::Int32: localResults.emplace_back(currentAddress, *reinterpret_cast<int32_t*>(buffer.data() + offset)); break;
                            case ValueType::Int64: localResults.emplace_back(currentAddress, *reinterpret_cast<int64_t*>(buffer.data() + offset)); break;
                            case ValueType::UInt8: localResults.emplace_back(currentAddress, *reinterpret_cast<uint8_t*>(buffer.data() + offset)); break;
                            case ValueType::UInt16: localResults.emplace_back(currentAddress, *reinterpret_cast<uint16_t*>(buffer.data() + offset)); break;
                            case ValueType::UInt32: localResults.emplace_back(currentAddress, *reinterpret_cast<uint32_t*>(buffer.data() + offset)); break;
                            case ValueType::UInt64: localResults.emplace_back(currentAddress, *reinterpret_cast<uint64_t*>(buffer.data() + offset)); break;
                            case ValueType::Float: localResults.emplace_back(currentAddress, *reinterpret_cast<float*>(buffer.data() + offset)); break;
                            case ValueType::Double: localResults.emplace_back(currentAddress, *reinterpret_cast<double*>(buffer.data() + offset)); break;
                        }
                    }
                    // Additional scan types can be implemented here.
                }
                return localResults;
            }));
        }

        for (auto& future : futures) {
            std::vector<ScannedAddress> results = future.get();
            std::lock_guard lock(m_Mutex);
            m_CurrentResults.insert(m_CurrentResults.end(),
                std::make_move_iterator(results.begin()),
                std::make_move_iterator(results.end()));
        }

        INFO("Found {} results", m_CurrentResults.size());
        INFO("Took: {}ms", std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count());
        m_Scanning = false;
    },
        valueType, scanType, minAddress, maxAddress, targetValue1, targetValue2)
        .detach();
    return true;
}

void MemoryScanner::NextScan(ValueType valueType, ScanType scanType, uintptr_t minAddress, uintptr_t maxAddress, const std::string& lowerValue, const std::string& upperValue)
{
    if (m_CurrentResults.empty()) {
        ERR("Not previous scan results");
        return;
    }
    if (m_Scanning) {
        return;
    }

    std::thread([this](ValueType valueType, ScanType scanType, uintptr_t minAddress, uintptr_t maxAddress, const std::string& lowerValue, const std::string& upperValue) {
        INFO("Refining {} results...", m_CurrentResults.size());

        ScanValue targetValue1, targetValue2;
        if (scanType != ScanType::UnknownInitialValue && !lowerValue.empty()) {
            targetValue1 = StringToValue(lowerValue, valueType);
        }
        if (scanType == ScanType::ValueBetween && !upperValue.empty()) {
            targetValue2 = StringToValue(upperValue, valueType);
        }

        m_Scanning = true;

        uint32_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4;
        }

        size_t totalAddresses = m_CurrentResults.size();
        size_t chunkSize = (totalAddresses + numThreads - 1) / numThreads;

        std::vector<std::future<std::vector<ScannedAddress>>> futures;
        for (size_t i = 0; i < numThreads; ++i) {
            size_t start = i * chunkSize;
            size_t end = std::min(start + chunkSize, totalAddresses);
            if (start >= totalAddresses) {
                break;
            }

            futures.push_back(std::async(std::launch::async, [this, scanType, &targetValue1, &targetValue2, minAddress, maxAddress](size_t startIdx, size_t endIdx) {
            std::vector<ScannedAddress> threadResults;

            for (size_t j = startIdx; j < endIdx; j++) {
                ScannedAddress& addr = m_CurrentResults[j];
                if (addr.Address < minAddress || addr.Address >= maxAddress) {
                    continue;
                }
                ScanValue value = addr.ReadValue(m_Process);
                if (CompareValues(scanType, value, targetValue1, targetValue2)) {
                    threadResults.push_back(addr);
                    threadResults.back().PreviousValue = value;
                }
            }

            return threadResults; }, start, end));
        }

        std::vector<ScannedAddress> newResults;
        for (auto& future : futures) {
            auto threadResult = future.get();
            newResults.insert(newResults.end(),
                std::make_move_iterator(threadResult.begin()),
                std::make_move_iterator(threadResult.end()));
        }

        {
            std::lock_guard lock(m_Mutex);
            m_CurrentResults = std::move(newResults);
        }
        m_Scanning = false;
        INFO("Found {} results", m_CurrentResults.size());
    },
        valueType, scanType, minAddress, maxAddress, lowerValue, upperValue)
        .detach();
}

bool MemoryScanner::CompareValues(ScanType scanType, const ScanValue& value, const ScanValue& targetValue1, const ScanValue& targetValue2)
{
    if (scanType == ScanType::ExactValue) {
        return value == targetValue1;
    }

    if (scanType == ScanType::BiggerThan) {
        return std::visit(overloads {
                              [&](int8_t v) { return v > std::get<int8_t>(targetValue1); },
                              [&](int16_t v) { return v > std::get<int16_t>(targetValue1); },
                              [&](int32_t v) { return v > std::get<int32_t>(targetValue1); },
                              [&](int64_t v) { return v > std::get<int64_t>(targetValue1); },
                              [&](uint8_t v) { return v > std::get<uint8_t>(targetValue1); },
                              [&](uint16_t v) { return v > std::get<uint16_t>(targetValue1); },
                              [&](uint32_t v) { return v > std::get<uint32_t>(targetValue1); },
                              [&](uint64_t v) { return v > std::get<uint64_t>(targetValue1); },
                              [&](float v) { return v > std::get<float>(targetValue1); },
                              [&](double v) { return v > std::get<double>(targetValue1); },
                          },
            value);
    }

    if (scanType == ScanType::SmallerThan) {
        return std::visit(overloads {
                              [&](int8_t v) { return v < std::get<int8_t>(targetValue1); },
                              [&](int16_t v) { return v < std::get<int16_t>(targetValue1); },
                              [&](int32_t v) { return v < std::get<int32_t>(targetValue1); },
                              [&](int64_t v) { return v < std::get<int64_t>(targetValue1); },
                              [&](uint8_t v) { return v < std::get<uint8_t>(targetValue1); },
                              [&](uint16_t v) { return v < std::get<uint16_t>(targetValue1); },
                              [&](uint32_t v) { return v < std::get<uint32_t>(targetValue1); },
                              [&](uint64_t v) { return v < std::get<uint64_t>(targetValue1); },
                              [&](float v) { return v < std::get<float>(targetValue1); },
                              [&](double v) { return v < std::get<double>(targetValue1); },
                          },
            value);
    }
    return false;
}

bool MemoryScanner::CompareByteArrays(ScanType scanType, ValueType valueType, uint8_t* data, const std::vector<uint8_t>& target1, const std::vector<uint8_t>& target2)
{
    if (scanType == ScanType::ExactValue) {
        return memcmp(data, target1.data(), target1.size()) == 0;
    }

    if (scanType == ScanType::BiggerThan) {
        switch (valueType) {
            case ValueType::Int8: return *(int8_t*)data > *(int8_t*)target1.data();
            case ValueType::Int16: return *(int16_t*)data > *(int16_t*)target1.data();
            case ValueType::Int32: return *(int32_t*)data > *(int32_t*)target1.data();
            case ValueType::Int64: return *(int64_t*)data > *(int64_t*)target1.data();
            case ValueType::UInt8: return *(uint8_t*)data > *(uint8_t*)target1.data();
            case ValueType::UInt16: return *(uint16_t*)data > *(uint16_t*)target1.data();
            case ValueType::UInt32: return *(uint32_t*)data > *(uint32_t*)target1.data();
            case ValueType::UInt64: return *(uint64_t*)data > *(uint64_t*)target1.data();
            case ValueType::Float: return *(float*)data > *(float*)target1.data();
            case ValueType::Double: return *(double*)data > *(double*)target1.data();
            default: return false;
        }
    }

    if (scanType == ScanType::SmallerThan) {
        switch (valueType) {
            case ValueType::Int8: return *(int8_t*)data < *(int8_t*)target1.data();
            case ValueType::Int16: return *(int16_t*)data < *(int16_t*)target1.data();
            case ValueType::Int32: return *(int32_t*)data < *(int32_t*)target1.data();
            case ValueType::Int64: return *(int64_t*)data < *(int64_t*)target1.data();
            case ValueType::UInt8: return *(uint8_t*)data < *(uint8_t*)target1.data();
            case ValueType::UInt16: return *(uint16_t*)data < *(uint16_t*)target1.data();
            case ValueType::UInt32: return *(uint32_t*)data < *(uint32_t*)target1.data();
            case ValueType::UInt64: return *(uint64_t*)data < *(uint64_t*)target1.data();
            case ValueType::Float: return *(float*)data < *(float*)target1.data();
            case ValueType::Double: return *(double*)data < *(double*)target1.data();
            default: return false;
        }
    }

    if (scanType == ScanType::ValueBetween) {
        switch (valueType) {
            case ValueType::Int8: return *(int8_t*)data >= *(int8_t*)target1.data() && *(int8_t*)data <= *(int8_t*)target2.data();
            case ValueType::Int16: return *(int16_t*)data >= *(int16_t*)target1.data() && *(int16_t*)data <= *(int16_t*)target2.data();
            case ValueType::Int32: return *(int32_t*)data >= *(int32_t*)target1.data() && *(int32_t*)data <= *(int32_t*)target2.data();
            case ValueType::Int64: return *(int64_t*)data >= *(int64_t*)target1.data() && *(int64_t*)data <= *(int64_t*)target2.data();
            case ValueType::UInt8: return *(uint8_t*)data >= *(uint8_t*)target1.data() && *(uint8_t*)data <= *(uint8_t*)target2.data();
            case ValueType::UInt16: return *(uint16_t*)data >= *(uint16_t*)target1.data() && *(uint16_t*)data <= *(uint16_t*)target2.data();
            case ValueType::UInt32: return *(uint32_t*)data >= *(uint32_t*)target1.data() && *(uint32_t*)data <= *(uint32_t*)target2.data();
            case ValueType::UInt64: return *(uint64_t*)data >= *(uint64_t*)target1.data() && *(uint64_t*)data <= *(uint64_t*)target2.data();
            case ValueType::Float: return *(float*)data >= *(float*)target1.data() && *(float*)data <= *(float*)target2.data();
            case ValueType::Double: return *(double*)data >= *(double*)target1.data() && *(double*)data <= *(double*)target2.data();
            default: return false;
        }
    }

    return true;
}

std::vector<uint8_t> MemoryScanner::StringToBytes(const std::string& str, ValueType type)
{
    std::string val = str.starts_with("0x") ? str.substr(2) : str;
    switch (type) {
        case ValueType::Int8: return ValueToBytes<int8_t>(std::stoi(val));
        case ValueType::UInt8: return ValueToBytes<uint8_t>(std::stoul(val));
        case ValueType::Int16: return ValueToBytes<int16_t>(std::stoi(val));
        case ValueType::UInt16: return ValueToBytes<uint16_t>(std::stoul(val));
        case ValueType::Int32: return ValueToBytes<int32_t>(std::stol(val));
        case ValueType::UInt32: return ValueToBytes<uint32_t>(std::stoul(val));
        case ValueType::Int64: return ValueToBytes<int64_t>(std::stoll(val));
        case ValueType::UInt64: return ValueToBytes<uint64_t>(std::stoull(val));
        case ValueType::Float: return ValueToBytes<float>(std::stof(val));
        case ValueType::Double: return ValueToBytes<double>(std::stod(val));
        default: return {};
    }
}

ScanValue MemoryScanner::StringToValue(const std::string& str, ValueType type)
{
    switch (type) {
        case ValueType::Int8: return static_cast<int8_t>(std::stoi(str));
        case ValueType::UInt8: return static_cast<uint8_t>(std::stoul(str));
        case ValueType::Int16: return static_cast<int16_t>(std::stoi(str));
        case ValueType::UInt16: return static_cast<uint16_t>(std::stoul(str));
        case ValueType::Int32: return static_cast<int32_t>(std::stol(str));
        case ValueType::UInt32: return static_cast<uint32_t>(std::stoul(str));
        case ValueType::Int64: return static_cast<int64_t>(std::stoll(str));
        case ValueType::UInt64: return static_cast<uint64_t>(std::stoull(str));
        case ValueType::Float: return static_cast<float>(std::stof(str));
        case ValueType::Double: return static_cast<double>(std::stod(str));
        default: return {};
    }
}

bool MemoryScanner::IsValidMemoryRegion(const MEMORY_BASIC_INFORMATION& memInfo)
{
    return (memInfo.State == MEM_COMMIT)
        && (memInfo.Protect & PAGE_GUARD) == 0
        && (memInfo.Protect != PAGE_NOACCESS)
        && ((memInfo.Protect & PAGE_READWRITE)
            || (memInfo.Protect & PAGE_READONLY)
            || (memInfo.Protect & PAGE_EXECUTE_READ)
            || (memInfo.Protect & PAGE_EXECUTE_READWRITE));
}