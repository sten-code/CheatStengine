#include "PatternScanner.h"

static std::vector<int> ParsePattern(std::string_view pattern)
{
    std::vector<int> bytes;
    bytes.reserve(std::count(pattern.begin(), pattern.end(), ' ') + 1);

    while (!pattern.empty()) {
        size_t spacePos = pattern.find(' ');
        std::string_view byte = pattern.substr(0, spacePos);

        if (byte == "?" || byte == "??") {
            bytes.push_back(-1);
        } else {
            int value = 0;
            auto [ptr, ec] = std::from_chars(byte.data(), byte.data() + byte.size(), value, 16);
            if (ec == std::errc()) {
                bytes.push_back(value);
            } else {
                bytes.push_back(-1);
            }
        }

        if (spacePos == std::string_view::npos) {
            break;
        }
        pattern.remove_prefix(spacePos + 1);
    }

    return bytes;
}

static bool CompareSignature(const uint8_t* data, const int* sig, size_t sigSize)
{
    // This can be further optimized with SIMD instructions
    for (size_t j = 0; j < sigSize; ++j) {
        if (sig[j] != -1 && data[j] != static_cast<uint8_t>(sig[j])) {
            return false;
        }
    }
    return true;
}

static uintptr_t SigScanInBuffer(uintptr_t base, const std::vector<uint8_t>& buffer, const std::vector<int>& sig)
{
    if (sig.empty() || buffer.size() < sig.size()) {
        return 0;
    }

    const uint8_t* data = buffer.data();
    size_t size = buffer.size();
    size_t sigSize = sig.size();
    const int* sig_data = sig.data();

    // Quick check for single-byte pattern
    if (sigSize == 1) {
        if (sig[0] == -1) {
            return base; // Wildcard-only pattern
        }
        const uint8_t target = static_cast<uint8_t>(sig[0]);
        auto it = std::find(data, data + size, target);
        return (it != data + size) ? base + (it - data) : 0;
    }

    // Main scanning loop
    for (size_t i = 0; i <= size - sigSize;) {
        if (CompareSignature(data + i, sig_data, sigSize)) {
            return base + i;
        }

        // Skip ahead optimization
        if (sig.back() != -1) {
            const uint8_t last_byte = static_cast<uint8_t>(sig.back());
            const uint8_t* next = std::find(data + i + sigSize, data + size, last_byte);
            if (next == data + size) {
                break;
            }
            i = (next - data) - (sigSize - 1);
        } else {
            i++;
        }
    }

    return 0;
}

PatternScanner::PatternScanner(Process& process)
    : m_Process(process)
{
}

std::vector<uintptr_t> PatternScanner::PatternScan(std::string_view pattern, uintptr_t start, uintptr_t end, bool stopAtFirst) const
{
    std::vector<int> sig = ParsePattern(pattern);

    std::vector<uintptr_t> results;
    while (start < end) {
        std::optional<MEMORY_BASIC_INFORMATION> mbi = m_Process.Query(start);
        if (!mbi) {
            break;
        }

        size_t regionSize = mbi->RegionSize;
        uintptr_t regionEnd = start + regionSize;

        // Skip uninteresting regions
        if ((mbi->State != MEM_COMMIT) || (mbi->Protect & PAGE_GUARD) || !(mbi->Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READWRITE))) {
            start = regionEnd;
            continue;
        }

        std::vector<uint8_t> buffer(regionSize);
        if (!m_Process.ReadBuffer(start, buffer.data(), regionSize)) {
            start = regionEnd;
            continue;
        }

        if (uintptr_t found = SigScanInBuffer(start, buffer, sig)) {
            results.push_back(found);
            if (stopAtFirst) {
                break;
            }
        }

        start = regionEnd;
    }

    return results;
}

std::vector<uintptr_t> PatternScanner::PatternScan(std::string_view pattern, std::string_view moduleName, bool stopAtFirst) const
{
    MODULEENTRY32 moduleEntry = m_Process.GetModuleEntry(moduleName);
    return PatternScan(pattern,
        reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr),
        reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr) + moduleEntry.modBaseSize, stopAtFirst);
}

std::optional<uintptr_t> PatternScanner::PatternScanOnce(std::string_view pattern, uintptr_t start, uintptr_t end) const
{
    std::vector<uintptr_t> results = PatternScan(pattern, start, end, true);
    if (!results.empty()) {
        return results[0];
    }
    return std::nullopt;
}

std::optional<uintptr_t> PatternScanner::PatternScanOnce(std::string_view pattern, std::string_view moduleName) const
{
    MODULEENTRY32 moduleEntry = m_Process.GetModuleEntry(moduleName);
    return PatternScanOnce(pattern,
        reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr),
        reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr) + moduleEntry.modBaseSize);
}