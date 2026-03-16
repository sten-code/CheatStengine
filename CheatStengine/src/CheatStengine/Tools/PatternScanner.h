#pragma once

#include <string>
#include <vector>

struct PatternScanResult {
    std::string Pattern;
    std::vector<uintptr_t> Results;
    size_t SelectedIndex = 0;
};