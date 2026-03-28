#pragma once

#include <CheatStengine/Process/Process.h>

#include <cstdint>
#include <string_view>

class PatternScanner {
public:
    explicit PatternScanner(std::unique_ptr<Process>& process);

    [[nodiscard]] std::vector<uintptr_t> PatternScan(std::string_view pattern, uintptr_t start = 0x0, uintptr_t end = 0x7FFFFFFFFFFF, bool stopAtFirst = false) const;
    [[nodiscard]] std::vector<uintptr_t> PatternScan(std::string_view pattern, std::string_view moduleName, bool stopAtFirst = false) const;
    [[nodiscard]] std::optional<uintptr_t> PatternScanOnce(std::string_view pattern, uintptr_t start = 0x0, uintptr_t end = 0x7FFFFFFFFFFF) const;
    [[nodiscard]] std::optional<uintptr_t> PatternScanOnce(std::string_view pattern, std::string_view moduleName) const;

private:
    std::unique_ptr<Process>& m_Process;
};
