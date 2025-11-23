#include "Process.h"

#include <Engine/Core/Log.h>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <string_view>
#include <unordered_set>

BOOL CALLBACK EnumWindowsToVector(HWND hwnd, LPARAM lParam)
{
    std::vector<Process::Window>* handles = reinterpret_cast<std::vector<Process::Window>*>(lParam);
    uint32_t pid;
    GetWindowThreadProcessId(hwnd, reinterpret_cast<LPDWORD>(&pid));
    char title[256];
    GetWindowTextA(hwnd, title, 256);
    handles->emplace_back(hwnd, pid, title);
    return TRUE;
}

Process::Process(uint32_t pid)
    : m_Pid(pid)
    , m_Handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid))
{
}

Process::Process(HANDLE handle)
    : m_Pid(GetProcessId(handle))
    , m_Handle(handle)
{
}

Process::Process(const std::string& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snapshot, &entry)) {
        do {
            if (strncmp(entry.szExeFile, processName.c_str(), processName.size()) == 0) {
                m_Pid = entry.th32ProcessID;
                m_Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_Pid);
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

Process::Process(Process&& other) noexcept
    : m_Pid(other.m_Pid)
    , m_Handle(other.m_Handle)
{
    other.m_Handle = nullptr;
    other.m_Pid = 0;
}

Process& Process::operator=(Process&& other) noexcept
{
    if (this != &other) {
        m_Handle = other.m_Handle;
        m_Pid = other.m_Pid;
        other.m_Handle = nullptr;
        other.m_Pid = 0;
    }
    return *this;
}

Process::~Process()
{
    Close();
}

std::vector<PROCESSENTRY32> Process::EnumerateProcesses()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    std::vector<PROCESSENTRY32> procs;
    if (Process32First(snapshot, &pe32)) {
        do {
            procs.push_back(pe32);
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return procs;
}
std::vector<Process::Window> Process::EnumerateWindows()
{
    std::vector<Window> handles;
    EnumWindows(EnumWindowsToVector, reinterpret_cast<LPARAM>(&handles));
    return handles;
}
std::vector<Process::Window> Process::EnumerateApplications()
{
    std::vector<Window> apps;
    for (HWND hwnd = GetTopWindow(GetDesktopWindow()); hwnd; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) {
        DWORD windowPid;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (!IsWindowVisible(hwnd)) {
            continue;
        }

        size_t len = GetWindowTextLength(hwnd);
        if (len == 0) {
            continue;
        }

        std::vector<char> title(len + 1);
        GetWindowTextA(hwnd, title.data(), title.size());
        apps.emplace_back(hwnd, windowPid, title.data());
    }
    return apps;
}

MODULEENTRY32 Process::GetModuleEntry(const std::string& name) const
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_Pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    MODULEENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Module32First(snapshot, &entry)) {
        do {
            if (_stricmp(entry.szModule, name.c_str()) == 0) {
                CloseHandle(snapshot);
                return entry;
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return {};
}

std::vector<MODULEENTRY32> Process::GetModuleEntries(bool refresh)
{
    if (!IsValid()) {
        return {};
    }

    if (!refresh && !m_Modules.empty()
        && std::chrono::system_clock::now() - m_LastModuleUpdate < std::chrono::seconds(5)) {
        return m_Modules;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_Pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    MODULEENTRY32 entry;
    entry.dwSize = sizeof(entry);

    std::vector<MODULEENTRY32> modules;
    if (Module32First(snapshot, &entry)) {
        std::unordered_set<std::string> seenModules;
        do {
            std::string moduleName = entry.szModule;
            if (seenModules.insert(moduleName).second) { // Only add if not already seen
                modules.push_back(entry);
            }
        } while (Module32Next(snapshot, &entry));
    }

    m_LastModuleUpdate = std::chrono::system_clock::now();
    m_Modules = modules;
    CloseHandle(snapshot);
    return modules;
}

uintptr_t Process::GetModuleBase(const std::string& moduleName) const
{
    return reinterpret_cast<uintptr_t>(GetModuleEntry(moduleName).modBaseAddr);
}

uintptr_t Process::GetModuleProc(uintptr_t moduleBase, const std::string& functionName) const
{
    IMAGE_DOS_HEADER dosHeader = Read<IMAGE_DOS_HEADER>(moduleBase);
    IMAGE_NT_HEADERS ntHeaders = Read<IMAGE_NT_HEADERS>(moduleBase + dosHeader.e_lfanew);

    const IMAGE_DATA_DIRECTORY& exportDataDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDataDir.VirtualAddress == 0 || exportDataDir.Size == 0) {
        std::cerr << "No export directory found: " << functionName << std::endl;
        return 0;
    }

    IMAGE_EXPORT_DIRECTORY exportDir = Read<IMAGE_EXPORT_DIRECTORY>(moduleBase + exportDataDir.VirtualAddress);

    std::vector<DWORD> nameRVAs = ReadArray<DWORD>(moduleBase + exportDir.AddressOfNames, exportDir.NumberOfNames);
    std::vector<WORD> ordinals = ReadArray<WORD>(moduleBase + exportDir.AddressOfNameOrdinals, exportDir.NumberOfNames);
    std::vector<DWORD> functionRVAs = ReadArray<DWORD>(moduleBase + exportDir.AddressOfFunctions, exportDir.NumberOfFunctions);

    if (nameRVAs.empty() || ordinals.empty() || functionRVAs.empty()) {
        std::cerr << "Failed to read export tables" << std::endl;
        return 0;
    }

    for (DWORD i = 0; i < exportDir.NumberOfNames; ++i) {
        std::string name = ReadString(moduleBase + nameRVAs[i]);
        if (name == functionName) {
            WORD ordinal = ordinals[i];
            if (ordinal >= functionRVAs.size()) {
                std::cerr << "Invalid ordinal" << std::endl;
                return 0;
            }
            DWORD functionRVA = functionRVAs[ordinal];
            return moduleBase + functionRVA;
        }
    }

    std::cout << "Function not found in module exports" << std::endl;
    return 0;
}

ULONG_PTR boffset(SIZE_T size, ULONG_PTR start, ULONG_PTR current)
{
    return (current - start) % size;
}

static std::vector<int> ParsePattern(std::string_view pattern)
{
    std::vector<int> bytes;
    bytes.reserve(std::count(pattern.begin(), pattern.end(), ' ') + 1);

    while (!pattern.empty()) {
        // Find next space or end
        size_t space_pos = pattern.find(' ');

        // Extract the current byte token
        std::string_view byte = pattern.substr(0, space_pos);

        // Parse the byte
        if (byte == "?" || byte == "??") {
            bytes.push_back(-1);
        } else {
            int value = 0;
            auto [ptr, ec] = std::from_chars(byte.data(), byte.data() + byte.size(), value, 16);
            if (ec == std::errc()) {
                bytes.push_back(value);
            } else {
                // Handle parse error (maybe throw or return empty vector)
                bytes.push_back(-1); // Or return {} to indicate error
            }
        }

        // Move to next token
        if (space_pos == std::string_view::npos) {
            break;
        }
        pattern.remove_prefix(space_pos + 1);
    }

    return bytes;
}

static bool CompareSignature(const uint8_t* data, const int* sig, size_t sig_size)
{
    // This can be further optimized with SIMD instructions
    for (size_t j = 0; j < sig_size; ++j) {
        if (sig[j] != -1 && data[j] != static_cast<uint8_t>(sig[j])) {
            return false;
        }
    }
    return true;
}

uintptr_t Process::PatternScan(std::string_view pattern, uintptr_t start, uintptr_t end) const
{
    auto results = PatternScanMulti({ { "pattern", pattern } }, start, end);
    return results.empty() ? 0 : results.begin()->second;
}

static uintptr_t SigScanInBuffer(uintptr_t base, const std::vector<uint8_t>& buffer, const std::vector<int>& sig)
{
    if (sig.empty() || buffer.size() < sig.size())
        return 0;

    const uint8_t* data = buffer.data();
    const size_t size = buffer.size();
    const size_t sig_size = sig.size();
    const int* sig_data = sig.data();

    // Quick check for single-byte pattern
    if (sig_size == 1) {
        if (sig[0] == -1)
            return base; // Wildcard-only pattern
        const uint8_t target = static_cast<uint8_t>(sig[0]);
        auto it = std::find(data, data + size, target);
        return (it != data + size) ? base + (it - data) : 0;
    }

    // Main scanning loop
    for (size_t i = 0; i <= size - sig_size;) {
        if (CompareSignature(data + i, sig_data, sig_size)) {
            return base + i;
        }

        // Skip ahead optimization
        if (sig.back() != -1) {
            const uint8_t last_byte = static_cast<uint8_t>(sig.back());
            const uint8_t* next = std::find(data + i + sig_size, data + size, last_byte);
            if (next == data + size)
                break;
            i = (next - data) - (sig_size - 1);
        } else {
            i++;
        }
    }

    return 0;
}

std::unordered_map<std::string, uintptr_t> Process::PatternScanMulti(
    const std::unordered_map<std::string_view, std::string_view>& patterns,
    uintptr_t start, uintptr_t end) const
{
    std::unordered_map<std::string, uintptr_t> results;
    if (patterns.empty())
        return results;

    // Parse all patterns first
    std::vector<std::pair<std::string, std::vector<int>>> parsed_patterns;
    parsed_patterns.reserve(patterns.size());

    for (const auto& [name, pattern] : patterns) {
        parsed_patterns.emplace_back(
            name,
            ParsePattern(pattern));
    }

    while (start < end && results.size() < patterns.size()) {
        std::optional<MEMORY_BASIC_INFORMATION> mbi = Query(start);
        if (!mbi)
            break;

        const size_t region_size = mbi->RegionSize;
        const uintptr_t region_end = start + region_size;

        // Skip uninteresting regions
        if ((mbi->State != MEM_COMMIT) || (mbi->Protect & PAGE_GUARD) || !(mbi->Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READWRITE))) {
            start = region_end;
            continue;
        }

        // Read memory once per region
        std::vector<uint8_t> buffer(region_size);
        SIZE_T bytes_read;
        if (!ReadProcessMemory(m_Handle, reinterpret_cast<LPCVOID>(start),
                buffer.data(), region_size, &bytes_read)
            || bytes_read != region_size) {
            start = region_end;
            continue;
        }

        // Check all patterns against this memory region
        for (const auto& [pattern_str, sig] : parsed_patterns) {
            if (results.count(pattern_str))
                continue; // Already found this pattern

            if (const uintptr_t found = SigScanInBuffer(start, buffer, sig)) {
                results[pattern_str] = found;
                if (results.size() == patterns.size()) {
                    return results; // Found all requested patterns
                }
            }
        }

        start = region_end;
    }

    return results;
}

std::vector<Process::CodeCave> Process::FindCodeCaves(size_t minSize, bool stopAtFirst, uint32_t protection) const
{
    std::vector<Process::CodeCave> codeCaves;

    uintptr_t address = 0;

    while (true) {
        auto mbi = Query(address);
        if (!mbi) {
            break;
        }

        if (mbi->State == MEM_COMMIT && (mbi->Protect & protection) != 0) {
            std::vector<uint8_t> buffer(mbi->RegionSize);
            size_t bytesRead = ReadBuffer((uintptr_t)mbi->BaseAddress, buffer.data(), mbi->RegionSize);

            if (bytesRead == mbi->RegionSize) {
                size_t currentCaveSize = 0;
                uintptr_t currentCaveStart = 0;

                for (size_t i = 0; i < buffer.size(); i++) {
                    if (buffer[i] == 0x00 || buffer[i] == 0x90 || buffer[i] == 0xCC) {
                        if (currentCaveSize == 0) {
                            currentCaveStart = (uintptr_t)mbi->BaseAddress + i;
                        }
                        currentCaveSize++;
                    } else {
                        if (currentCaveSize >= minSize) {
                            codeCaves.emplace_back(currentCaveStart, currentCaveSize);
                            if (stopAtFirst) {
                                return codeCaves;
                            }
                        }
                        currentCaveSize = 0;
                    }
                }

                // Check if we ended on a cave
                if (currentCaveSize >= minSize) {
                    codeCaves.emplace_back(currentCaveStart, currentCaveSize);
                    if (stopAtFirst) {
                        return codeCaves;
                    }
                }
            }
        }

        // Move to the next region
        address = (uintptr_t)mbi->BaseAddress + mbi->RegionSize;

        // Check for overflow/wrap-around
        if (address < (uintptr_t)mbi->BaseAddress) {
            break;
        }
    }

    // Sort by size (largest first)
    std::sort(codeCaves.begin(), codeCaves.end(),
        [](const auto& a, const auto& b) { return a.size > b.size; });

    return codeCaves;
}

std::optional<Process::CodeCave> Process::FindCodeCave(size_t minSize, uint32_t protection) const
{
    std::vector<Process::CodeCave> codeCaves = FindCodeCaves(minSize, true, protection);
    if (codeCaves.empty()) {
        return std::nullopt;
    }
    return codeCaves.front();
}

std::optional<Process::CodeCave> Process::FindCodeCaveInModule(size_t minSize, const std::string& moduleName, uint32_t protection) const
{
    std::vector<Process::CodeCave> codeCaves = FindCodeCavesInModule(minSize, moduleName, true, protection);
    if (codeCaves.empty()) {
        return std::nullopt;
    }
    return codeCaves.front();
}

std::vector<Process::CodeCave> Process::FindCodeCavesInModule(size_t minSize, const std::string& moduleName, bool stopAtFirst, uint32_t protection) const
{
    std::vector<Process::CodeCave> codeCaves;

    MODULEENTRY32 entry = GetModuleEntry(moduleName);
    uintptr_t moduleAddr = (uintptr_t)entry.modBaseAddr;
    uintptr_t moduleSize = entry.modBaseSize;
    uintptr_t moduleEnd = moduleAddr + moduleSize;

    uintptr_t address = moduleAddr;

    while (address < moduleEnd) {
        std::optional<MEMORY_BASIC_INFORMATION> mbi = Query(address);
        if (!mbi) {
            break;
        }

        if (mbi->State == MEM_COMMIT
            && (mbi->Protect & (PAGE_NOACCESS | PAGE_GRAPHICS_NOACCESS | PAGE_GUARD | PAGE_TARGETS_INVALID | PAGE_ENCLAVE_THREAD_CONTROL | PAGE_ENCLAVE_UNVALIDATED)) == 0
            && (mbi->Protect & protection) != 0) {
            std::vector<uint8_t> buffer = ReadBuffer((uintptr_t)mbi->BaseAddress, mbi->RegionSize);

            uintptr_t currentCaveStart = 0;
            size_t currentCaveSize = 0;

            for (size_t i = 0; i < buffer.size(); i++) {
                if (buffer[i] == 0x00 || buffer[i] == 0x90 || buffer[i] == 0xCC) {
                    if (currentCaveSize == 0) {
                        currentCaveStart = (uintptr_t)mbi->BaseAddress + i;
                    }
                    currentCaveSize++;
                } else {
                    if (currentCaveSize >= 0x50) {
                        codeCaves.emplace_back(currentCaveStart, currentCaveSize);
                        if (stopAtFirst) {
                            return codeCaves;
                        }
                    }
                    currentCaveSize = 0;
                }
            }

            // Check if we ended on a cave
            if (currentCaveSize >= minSize) {
                codeCaves.emplace_back(currentCaveStart, currentCaveSize);
                if (stopAtFirst) {
                    return codeCaves;
                }
            }
        }

        // Move to the next region
        address = (uintptr_t)mbi->BaseAddress + mbi->RegionSize;

        // Check for overflow/wrap-around
        if (address < (uintptr_t)mbi->BaseAddress) {
            break;
        }
    }

    // Sort by size (largest first)
    std::sort(codeCaves.begin(), codeCaves.end(), [](const Process::CodeCave& a, const Process::CodeCave& b) {
        return a.size > b.size;
    });

    return codeCaves;
}

std::vector<Process::CodeCave> Process::FindCodeCavesInModules(size_t minSize, const std::unordered_set<std::string>& moduleNames, uint32_t protection) const
{
    std::vector<Process::CodeCave> codeCaves;
    for (const std::string& moduleName : moduleNames) {
        std::vector<Process::CodeCave> moduleCodeCaves = FindCodeCavesInModule(minSize, moduleName, protection);
        codeCaves.insert(codeCaves.begin(), moduleCodeCaves.begin(), moduleCodeCaves.end());
    }
    return codeCaves;
}

std::string Process::ReadString(uintptr_t address, size_t maxLength) const
{
    std::string result;
    result.resize(maxLength);
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(m_Handle, (LPCVOID)address, result.data(), maxLength, &bytesRead)) {
        return {};
    }
    size_t length = strnlen(result.c_str(), maxLength);
    result.resize(length);
    return result;
}

std::string Process::ReadStdString(uintptr_t address) const
{
    uint32_t check = Read<uint32_t>(address + 0x18);
    if (check > 15)
        address = Read<uintptr_t>(address);
    return ReadString(address);
}

std::vector<uint8_t> Process::ReadBuffer(uintptr_t address, size_t size) const
{
    std::vector<uint8_t> buffer(size);
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(m_Handle, (LPCVOID)address, buffer.data(), size, &bytesRead)) {
        return {};
    }
    return buffer;
}

size_t Process::ReadBuffer(uintptr_t address, void* buffer, size_t size) const
{
    size_t bytesRead = 0;
    if (!ReadProcessMemory(m_Handle, (LPCVOID)address, buffer, size, &bytesRead))
        return 0;
    return bytesRead;
}

std::optional<uint32_t> Process::Protect(uintptr_t address, size_t size, uint32_t protection) const
{
    DWORD oldProtect;
    if (!VirtualProtectEx(m_Handle, reinterpret_cast<LPVOID>(address), size, protection, &oldProtect)) {
        return std::nullopt;
    }
    return oldProtect;
}

bool Process::IsAddressReadable(uintptr_t address) const
{
    const std::optional<MEMORY_BASIC_INFORMATION> mbi = Query(address);
    if (!mbi) {
        return false;
    }

    return mbi->Protect & (PAGE_WRITECOPY | PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_GRAPHICS_READONLY | PAGE_GRAPHICS_READWRITE | PAGE_GRAPHICS_EXECUTE_READ | PAGE_GRAPHICS_EXECUTE_READWRITE);
}

bool Process::WriteBuffer(uintptr_t address, const void* buffer, size_t size) const
{
    SIZE_T bytesWritten;
    return WriteProcessMemory(m_Handle, (LPVOID)address, buffer, size, &bytesWritten) && bytesWritten == size;
}

bool Process::WriteBuffer(uintptr_t address, const std::vector<uint8_t>& buffer) const
{
    return WriteBuffer(address, buffer.data(), buffer.size());
}

bool Process::WriteString(uintptr_t address, const std::string& string) const
{
    if (!WriteBuffer(address, string.data(), string.size()))
        return false;
    return Write<uint8_t>(address + string.size(), 0);
}

uintptr_t Process::Allocate(size_t size, uint32_t protection, uint32_t allocationType) const
{
    return reinterpret_cast<uintptr_t>(VirtualAllocEx(m_Handle, nullptr, size, allocationType, protection));
}

void Process::Free(uintptr_t address, uint32_t freeType) const
{
    VirtualFreeEx(m_Handle, reinterpret_cast<LPVOID>(address), 0, freeType);
}

std::optional<MEMORY_BASIC_INFORMATION> Process::Query(uintptr_t address) const
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
        return std::nullopt;
    }
    return mbi;
}

void Process::Close()
{
    if (m_Handle) {
        CloseHandle(m_Handle);
        m_Handle = 0;
    }
}
