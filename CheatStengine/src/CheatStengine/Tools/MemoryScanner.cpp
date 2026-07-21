#include "MemoryScanner.h"

#include <CheatStengine/Process/KernelProcess.h>
#include <CheatStengine/Process/Process.h>
#include <Engine/Core/Core.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace {

    constexpr size_t ScanReadChunkSize = 1024 * 1024;
    constexpr size_t MaximumConsecutiveQueryFailures = 16;

    std::unique_ptr<Process> CloneProcess(const std::unique_ptr<Process>& process)
    {
        if (!process || !process->IsValid()) {
            return {};
        }

        const ProcessMode mode = dynamic_cast<const KernelProcess*>(process.get())
            ? ProcessMode::Kernel
            : ProcessMode::WinAPI;
        try {
            std::unique_ptr<Process> clone = Process::Create(process->GetPid(), mode);
            if (!clone || !clone->IsValid() || !clone->Query(0)) {
                WARN("Failed to open an independent process connection for the memory scan");
                return {};
            }
            return clone;
        } catch (const std::exception& error) {
            WARN("Failed to open an independent process connection for the memory scan: {}", error.what());
            return {};
        }
    }

    std::string_view Trim(std::string_view value)
    {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return {};
        }

        const size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    template <typename T>
    T ParseInteger(std::string_view input)
    {
        static_assert(std::is_integral_v<T>);

        const std::string_view trimmed = Trim(input);
        if (trimmed.empty()) {
            throw std::invalid_argument("value is empty");
        }

        const size_t prefixOffset = (trimmed.front() == '+' || trimmed.front() == '-') ? 1 : 0;
        const bool isNegative = trimmed.front() == '-';
        const bool isHex = trimmed.size() >= prefixOffset + 2
            && trimmed[prefixOffset] == '0'
            && (trimmed[prefixOffset + 1] == 'x' || trimmed[prefixOffset + 1] == 'X');
        const int base = isHex ? 16 : 10;
        const std::string text(trimmed);
        size_t parsedCharacters = 0;

        if constexpr (std::is_unsigned_v<T>) {
            if (isNegative) {
                throw std::out_of_range("negative value cannot be stored in an unsigned type");
            }

            const unsigned long long parsed = std::stoull(text, &parsedCharacters, base);
            if (parsedCharacters != text.size() || parsed > std::numeric_limits<T>::max()) {
                throw std::out_of_range("value is outside the selected type's range");
            }
            return static_cast<T>(parsed);
        } else {
            if (isHex && !isNegative) {
                using UnsignedT = std::make_unsigned_t<T>;
                const unsigned long long parsed = std::stoull(text, &parsedCharacters, base);
                if (parsedCharacters != text.size() || parsed > std::numeric_limits<UnsignedT>::max()) {
                    throw std::out_of_range("value is outside the selected type's range");
                }

                const UnsignedT bits = static_cast<UnsignedT>(parsed);
                T value {};
                std::memcpy(&value, &bits, sizeof(value));
                return value;
            }

            const long long parsed = std::stoll(text, &parsedCharacters, base);
            if (parsedCharacters != text.size()
                || parsed < static_cast<long long>(std::numeric_limits<T>::min())
                || parsed > static_cast<long long>(std::numeric_limits<T>::max())) {
                throw std::out_of_range("value is outside the selected type's range");
            }
            return static_cast<T>(parsed);
        }
    }

    template <typename T>
    T ParseFloatingPoint(std::string_view input)
    {
        const std::string_view trimmed = Trim(input);
        if (trimmed.empty()) {
            throw std::invalid_argument("value is empty");
        }

        const std::string text(trimmed);
        size_t parsedCharacters = 0;
        T value {};
        if constexpr (std::is_same_v<T, float>) {
            value = std::stof(text, &parsedCharacters);
        } else {
            value = std::stod(text, &parsedCharacters);
        }

        if (parsedCharacters != text.size()) {
            throw std::invalid_argument("value contains invalid characters");
        }
        return value;
    }

    ScanValue ParseScanValue(std::string_view input, ValueType type)
    {
        switch (type) {
            case ValueType::Int8: return ParseInteger<int8_t>(input);
            case ValueType::Int16: return ParseInteger<int16_t>(input);
            case ValueType::Int32: return ParseInteger<int32_t>(input);
            case ValueType::Int64: return ParseInteger<int64_t>(input);
            case ValueType::UInt8: return ParseInteger<uint8_t>(input);
            case ValueType::UInt16: return ParseInteger<uint16_t>(input);
            case ValueType::UInt32: return ParseInteger<uint32_t>(input);
            case ValueType::UInt64: return ParseInteger<uint64_t>(input);
            case ValueType::Float: return ParseFloatingPoint<float>(input);
            case ValueType::Double: return ParseFloatingPoint<double>(input);
            default: throw std::invalid_argument("invalid value type");
        }
    }

    template <typename T>
    T ReadUnaligned(const uint8_t* data)
    {
        T value {};
        std::memcpy(&value, data, sizeof(value));
        return value;
    }

    ScanValue ScanValueFromBytes(ValueType type, const uint8_t* data)
    {
        switch (type) {
            case ValueType::Int8: return ReadUnaligned<int8_t>(data);
            case ValueType::Int16: return ReadUnaligned<int16_t>(data);
            case ValueType::Int32: return ReadUnaligned<int32_t>(data);
            case ValueType::Int64: return ReadUnaligned<int64_t>(data);
            case ValueType::UInt8: return ReadUnaligned<uint8_t>(data);
            case ValueType::UInt16: return ReadUnaligned<uint16_t>(data);
            case ValueType::UInt32: return ReadUnaligned<uint32_t>(data);
            case ValueType::UInt64: return ReadUnaligned<uint64_t>(data);
            case ValueType::Float: return ReadUnaligned<float>(data);
            case ValueType::Double: return ReadUnaligned<double>(data);
            default: throw std::invalid_argument("invalid value type");
        }
    }

    bool TryReadScanValue(const Process& process, uintptr_t address, const ScanValue& previousValue, ScanValue& value)
    {
        return std::visit([&](auto previous) {
            using T = decltype(previous);
            T current {};
            if (!process.ReadBuffer(address, &current, sizeof(current))) {
                return false;
            }
            value = current;
            return true;
        },
            previousValue);
    }

    bool AreBoundsOrdered(const ScanValue& lower, const ScanValue& upper)
    {
        return std::visit([&](auto lowerValue) {
            using T = decltype(lowerValue);
            const T* upperValue = std::get_if<T>(&upper);
            return upperValue && lowerValue <= *upperValue;
        },
            lower);
    }

    template <typename T>
    bool CompareRawValues(ScanType scanType, const uint8_t* data,
        const std::vector<uint8_t>& target1, const std::vector<uint8_t>& target2)
    {
        if (scanType == ScanType::UnknownInitialValue) {
            return true;
        }
        if (target1.size() != sizeof(T)) {
            return false;
        }

        const T currentValue = ReadUnaligned<T>(data);
        const T lowerValue = ReadUnaligned<T>(target1.data());
        switch (scanType) {
            case ScanType::ExactValue: return currentValue == lowerValue;
            case ScanType::BiggerThan: return currentValue > lowerValue;
            case ScanType::SmallerThan: return currentValue < lowerValue;
            case ScanType::ValueBetween:
                return target2.size() == sizeof(T)
                    && currentValue >= lowerValue
                    && currentValue <= ReadUnaligned<T>(target2.data());
            default: return false;
        }
    }

} // namespace

MemoryScanner::MemoryScanner(const std::unique_ptr<Process>& process)
    : m_Process(CloneProcess(process))
{
    m_CurrentResults.reserve(100000);
}

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
    try {
        const ScanValue parsedValue = ParseScanValue(value, Type);
        return std::visit([&](auto typedValue) {
            return proc.Write(Address, typedValue);
        },
            parsedValue);
    } catch (const std::exception& error) {
        ERR("Invalid value '{}': {}", value, error.what());
        return false;
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
    const size_t typeSize = GetTypeSize(valueType);
    if (!m_Process || !m_Process->IsValid() || typeSize == 0
        || scanType < ScanType::ExactValue || scanType >= ScanType::COUNT
        || minAddress >= maxAddress) {
        ERR("Invalid memory scan configuration");
        return false;
    }

    std::vector<uint8_t> targetValue1;
    std::vector<uint8_t> targetValue2;
    try {
        if (scanType != ScanType::UnknownInitialValue) {
            if (lowerValue.empty()) {
                ERR("A scan value is required");
                return false;
            }
            targetValue1 = StringToBytes(lowerValue, valueType);
        }

        if (scanType == ScanType::ValueBetween) {
            if (upperValue.empty()) {
                ERR("An upper scan value is required");
                return false;
            }
            targetValue2 = StringToBytes(upperValue, valueType);
            if (!AreBoundsOrdered(StringToValue(lowerValue, valueType), StringToValue(upperValue, valueType))) {
                ERR("The lower scan value must not exceed the upper scan value");
                return false;
            }
        }
    } catch (const std::exception& error) {
        ERR("Invalid scan value: {}", error.what());
        return false;
    }

    std::scoped_lock threadLock(m_ThreadMutex);
    if (m_Scanning.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }
    m_ValueType = valueType;

    try {
        m_ScanThread = std::jthread([this, valueType, scanType, minAddress, maxAddress,
                                        typeSize, targetValue1 = std::move(targetValue1),
                                        targetValue2 = std::move(targetValue2)](std::stop_token stopToken) {
            try {
                auto startTime = std::chrono::high_resolution_clock::now();
                {
                    std::lock_guard lock(m_Mutex);
                    m_CurrentResults.clear();
                }

                std::vector<std::pair<uintptr_t, size_t>> regions;
                regions.reserve(256);
                uintptr_t currentAddress = minAddress;
                size_t consecutiveQueryFailures = 0;
                while (currentAddress < maxAddress && !stopToken.stop_requested()) {
                    const std::optional<MEMORY_BASIC_INFORMATION> mbi = m_Process->Query(currentAddress);
                    if (!mbi) {
                        ++consecutiveQueryFailures;
                        if (consecutiveQueryFailures >= MaximumConsecutiveQueryFailures) {
                            WARN("Stopping memory-region discovery after {} consecutive query failures at 0x{:X}",
                                consecutiveQueryFailures, currentAddress);
                            break;
                        }
                        currentAddress += std::min<uintptr_t>(0x1000, maxAddress - currentAddress);
                        continue;
                    }
                    consecutiveQueryFailures = 0;

                    const uintptr_t base = reinterpret_cast<uintptr_t>(mbi->BaseAddress);
                    uintptr_t regionEnd = std::numeric_limits<uintptr_t>::max();
                    if (mbi->RegionSize <= std::numeric_limits<uintptr_t>::max() - base) {
                        regionEnd = base + static_cast<uintptr_t>(mbi->RegionSize);
                    }

                    if (regionEnd <= currentAddress) {
                        currentAddress += std::min<uintptr_t>(0x1000, maxAddress - currentAddress);
                        continue;
                    }

                    const uintptr_t scanStart = std::max({ minAddress, currentAddress, base });
                    const uintptr_t scanEnd = std::min(maxAddress, regionEnd);
                    if (IsValidMemoryRegion(*mbi)
                        && scanEnd > scanStart
                        && scanEnd - scanStart >= typeSize) {
                        regions.emplace_back(scanStart, static_cast<size_t>(scanEnd - scanStart));
                    }
                    currentAddress = regionEnd;
                }

                INFO("Found {} regions to scan", regions.size());
                INFO("Region query took: {}ms", std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count());
                startTime = std::chrono::high_resolution_clock::now();

                std::vector<ScannedAddress> newResults;
                if (!regions.empty() && !stopToken.stop_requested()) {
                    const size_t hardwareThreads = std::max<size_t>(1, std::thread::hardware_concurrency());
                    const size_t workerCount = std::min(hardwareThreads, regions.size());
                    const size_t regionsPerWorker = (regions.size() + workerCount - 1) / workerCount;

                    std::vector<std::future<std::vector<ScannedAddress>>> futures;
                    futures.reserve(workerCount);
                    for (size_t worker = 0; worker < workerCount; ++worker) {
                        const size_t firstRegion = worker * regionsPerWorker;
                        const size_t lastRegion = std::min(firstRegion + regionsPerWorker, regions.size());
                        if (firstRegion >= lastRegion) {
                            break;
                        }

                        futures.emplace_back(std::async(std::launch::async,
                            [this, &regions, firstRegion, lastRegion, typeSize, scanType, valueType,
                                &targetValue1, &targetValue2, stopToken] {
                                std::vector<ScannedAddress> localResults;
                                for (size_t regionIndex = firstRegion;
                                    regionIndex < lastRegion && !stopToken.stop_requested();
                                    ++regionIndex) {
                                    const auto [address, regionSize] = regions[regionIndex];
                                    std::vector<uint8_t> buffer(std::min(ScanReadChunkSize, regionSize));

                                    for (size_t regionOffset = 0;
                                        regionOffset < regionSize && !stopToken.stop_requested();) {
                                        const size_t readSize = std::min(buffer.size(), regionSize - regionOffset);
                                        if (m_Process->ReadBuffer(address + regionOffset, buffer.data(), readSize)) {
                                            for (size_t offset = 0; offset + typeSize <= readSize; offset += typeSize) {
                                                const uint8_t* valueBytes = buffer.data() + offset;
                                                if (CompareByteArrays(scanType, valueType, valueBytes,
                                                        targetValue1, targetValue2)) {
                                                    localResults.emplace_back(address + regionOffset + offset,
                                                        ScanValueFromBytes(valueType, valueBytes));
                                                }
                                            }
                                        }
                                        regionOffset += readSize;
                                    }
                                }
                                return localResults;
                            }));
                    }

                    for (auto& future : futures) {
                        std::vector<ScannedAddress> results = future.get();
                        newResults.insert(newResults.end(),
                            std::make_move_iterator(results.begin()),
                            std::make_move_iterator(results.end()));
                    }
                }

                if (!stopToken.stop_requested()) {
                    const size_t resultCount = newResults.size();
                    {
                        std::lock_guard lock(m_Mutex);
                        m_CurrentResults = std::move(newResults);
                    }

                    INFO("Found {} results", resultCount);
                    INFO("Memory scan took: {}ms", std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - startTime).count());
                }
            } catch (const std::exception& error) {
                ERR("Memory scan failed: {}", error.what());
            } catch (...) {
                ERR("Memory scan failed with an unknown error");
            }
            m_Scanning.store(false, std::memory_order_release);
        });
    } catch (const std::exception& error) {
        m_ValueType = ValueType::COUNT;
        m_Scanning.store(false, std::memory_order_release);
        ERR("Failed to start memory scan: {}", error.what());
        return false;
    }

    return true;
}

bool MemoryScanner::NextScan(ValueType valueType, ScanType scanType, uintptr_t minAddress, uintptr_t maxAddress, const std::string& lowerValue, const std::string& upperValue)
{
    const size_t typeSize = GetTypeSize(valueType);
    if (!m_Process || !m_Process->IsValid() || typeSize == 0
        || scanType < ScanType::ExactValue || scanType >= ScanType::COUNT
        || minAddress >= maxAddress) {
        ERR("Invalid memory scan configuration");
        return false;
    }
    if (valueType != m_ValueType) {
        ERR("The value type cannot be changed while refining a scan");
        return false;
    }

    {
        std::lock_guard lock(m_Mutex);
        if (m_CurrentResults.empty()) {
            ERR("No previous scan results");
            return false;
        }
    }

    ScanValue targetValue1 = int8_t {};
    ScanValue targetValue2 = int8_t {};
    try {
        if (scanType != ScanType::UnknownInitialValue) {
            if (lowerValue.empty()) {
                ERR("A scan value is required");
                return false;
            }
            targetValue1 = StringToValue(lowerValue, valueType);
        }
        if (scanType == ScanType::ValueBetween) {
            if (upperValue.empty()) {
                ERR("An upper scan value is required");
                return false;
            }
            targetValue2 = StringToValue(upperValue, valueType);
            if (!AreBoundsOrdered(targetValue1, targetValue2)) {
                ERR("The lower scan value must not exceed the upper scan value");
                return false;
            }
        }
    } catch (const std::exception& error) {
        ERR("Invalid scan value: {}", error.what());
        return false;
    }

    std::scoped_lock threadLock(m_ThreadMutex);
    if (m_Scanning.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }

    try {
        m_ScanThread = std::jthread([this, scanType, minAddress, maxAddress, typeSize,
                                        targetValue1 = std::move(targetValue1),
                                        targetValue2 = std::move(targetValue2)](std::stop_token stopToken) {
            try {
                size_t totalAddresses = 0;
                {
                    std::lock_guard lock(m_Mutex);
                    totalAddresses = m_CurrentResults.size();
                }
                INFO("Refining {} results...", totalAddresses);

                const size_t hardwareThreads = std::max<size_t>(1, std::thread::hardware_concurrency());
                const size_t workerCount = std::max<size_t>(1, std::min(hardwareThreads, totalAddresses));
                const size_t addressesPerWorker = (totalAddresses + workerCount - 1) / workerCount;

                std::vector<std::future<std::vector<ScannedAddress>>> futures;
                futures.reserve(workerCount);
                for (size_t worker = 0; worker < workerCount; ++worker) {
                    const size_t firstAddress = worker * addressesPerWorker;
                    const size_t lastAddress = std::min(firstAddress + addressesPerWorker, totalAddresses);
                    if (firstAddress >= lastAddress) {
                        break;
                    }

                    futures.emplace_back(std::async(std::launch::async,
                        [this, scanType, &targetValue1, &targetValue2, minAddress, maxAddress, typeSize,
                            firstAddress, lastAddress, stopToken] {
                            std::vector<ScannedAddress> localResults;
                            for (size_t index = firstAddress;
                                index < lastAddress && !stopToken.stop_requested();
                                ++index) {
                                const ScannedAddress& address = m_CurrentResults[index];
                                if (address.Address < minAddress || address.Address >= maxAddress
                                    || typeSize > maxAddress - address.Address) {
                                    continue;
                                }

                                ScanValue value = address.PreviousValue;
                                if (!TryReadScanValue(*m_Process, address.Address, address.PreviousValue, value)) {
                                    continue;
                                }
                                if (CompareValues(scanType, value, targetValue1, targetValue2)) {
                                    localResults.push_back(address);
                                    localResults.back().PreviousValue = value;
                                }
                            }
                            return localResults;
                        }));
                }

                std::vector<ScannedAddress> newResults;
                for (auto& future : futures) {
                    std::vector<ScannedAddress> results = future.get();
                    newResults.insert(newResults.end(),
                        std::make_move_iterator(results.begin()),
                        std::make_move_iterator(results.end()));
                }

                if (!stopToken.stop_requested()) {
                    const size_t resultCount = newResults.size();
                    {
                        std::lock_guard lock(m_Mutex);
                        m_CurrentResults = std::move(newResults);
                    }
                    INFO("Found {} results", resultCount);
                }
            } catch (const std::exception& error) {
                ERR("Memory scan refinement failed: {}", error.what());
            } catch (...) {
                ERR("Memory scan refinement failed with an unknown error");
            }
            m_Scanning.store(false, std::memory_order_release);
        });
    } catch (const std::exception& error) {
        m_Scanning.store(false, std::memory_order_release);
        ERR("Failed to start memory scan refinement: {}", error.what());
        return false;
    }

    return true;
}

void MemoryScanner::Cancel()
{
    std::scoped_lock lock(m_ThreadMutex);
    if (m_ScanThread.joinable()) {
        m_ScanThread.request_stop();
    }
}

bool MemoryScanner::CompareValues(ScanType scanType, const ScanValue& value, const ScanValue& targetValue1, const ScanValue& targetValue2)
{
    if (scanType == ScanType::UnknownInitialValue) {
        return true;
    }

    return std::visit([&](auto currentValue) {
        using T = decltype(currentValue);
        const T* lowerValue = std::get_if<T>(&targetValue1);
        if (!lowerValue) {
            return false;
        }

        switch (scanType) {
            case ScanType::ExactValue: return currentValue == *lowerValue;
            case ScanType::BiggerThan: return currentValue > *lowerValue;
            case ScanType::SmallerThan: return currentValue < *lowerValue;
            case ScanType::ValueBetween: {
                const T* upperValue = std::get_if<T>(&targetValue2);
                return upperValue && currentValue >= *lowerValue && currentValue <= *upperValue;
            }
            default: return false;
        }
    },
        value);
}

bool MemoryScanner::CompareByteArrays(ScanType scanType, ValueType valueType, const uint8_t* data, const std::vector<uint8_t>& target1, const std::vector<uint8_t>& target2)
{
    if (!data) {
        return false;
    }

    switch (valueType) {
        case ValueType::Int8: return CompareRawValues<int8_t>(scanType, data, target1, target2);
        case ValueType::Int16: return CompareRawValues<int16_t>(scanType, data, target1, target2);
        case ValueType::Int32: return CompareRawValues<int32_t>(scanType, data, target1, target2);
        case ValueType::Int64: return CompareRawValues<int64_t>(scanType, data, target1, target2);
        case ValueType::UInt8: return CompareRawValues<uint8_t>(scanType, data, target1, target2);
        case ValueType::UInt16: return CompareRawValues<uint16_t>(scanType, data, target1, target2);
        case ValueType::UInt32: return CompareRawValues<uint32_t>(scanType, data, target1, target2);
        case ValueType::UInt64: return CompareRawValues<uint64_t>(scanType, data, target1, target2);
        case ValueType::Float: return CompareRawValues<float>(scanType, data, target1, target2);
        case ValueType::Double: return CompareRawValues<double>(scanType, data, target1, target2);
        default: return false;
    }
}

std::vector<uint8_t> MemoryScanner::StringToBytes(const std::string& str, ValueType type)
{
    const ScanValue value = ParseScanValue(str, type);
    return std::visit([](auto typedValue) {
        return ValueToBytes(typedValue);
    },
        value);
}

ScanValue MemoryScanner::StringToValue(const std::string& str, ValueType type)
{
    return ParseScanValue(str, type);
}

bool MemoryScanner::IsValidMemoryRegion(const MEMORY_BASIC_INFORMATION& memInfo)
{
    if (memInfo.State != MEM_COMMIT || (memInfo.Protect & PAGE_GUARD) != 0) {
        return false;
    }

    switch (memInfo.Protect & 0xFF) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY: return true;
        default: return false;
    }
}
