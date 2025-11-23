#pragma once

#include <Windows.h>
#include <tlHelp32.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Process {
public:
    struct CodeCave {
        uintptr_t address;
        size_t size;
    };
    struct Window {
        HWND Handle;
        uint32_t Pid;
        std::string Title;
    };

    explicit Process(uint32_t pid);
    explicit Process(HANDLE handle);
    explicit Process(const std::string& name);
    Process() = default;

    // Delete the copy constructor and copy assignment
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;

    ~Process();

    static std::vector<PROCESSENTRY32> EnumerateProcesses();
    static std::vector<Window> EnumerateWindows();
    static std::vector<Window> EnumerateApplications();

    /// <summary>
    /// Finds a module by name.
    /// </summary>
    /// <param name="name"></param>
    /// <returns>If no module is found, an empty MODULEENTRY32 struct is returned.</returns>
    [[nodiscard]] MODULEENTRY32 GetModuleEntry(const std::string& name) const;

    /// <summary>
    /// Finds all modules within the process.
    /// </summary>
    /// <returns>A vector of MODULEENTRY32</returns>
    [[nodiscard]] std::vector<MODULEENTRY32> GetModuleEntries(bool refresh = false);

    /// <summary>
    /// Finds the base address of a specific module by name.
    /// </summary>
    /// <param name="moduleName"></param>
    /// <returns>If no module is found, 0 is returned.</returns>
    [[nodiscard]] uintptr_t GetModuleBase(const std::string& moduleName) const;

    [[nodiscard]] uintptr_t GetModuleProc(uintptr_t moduleBase, const std::string& functionName) const;

    /// <summary>
    /// Virtual protects a specific region in memory.
    /// </summary>
    /// <param name="address"></param>
    /// <param name="size"></param>
    /// <param name="protection"></param>
    /// <returns>If something went wrong an std::nullopt is returned, otherwise the old protection is returned.</returns>
    [[nodiscard]] std::optional<uint32_t> Protect(uintptr_t address, size_t size, uint32_t protection) const;

    [[nodiscard]] bool IsAddressReadable(uintptr_t address) const;

    /// <summary>
    /// Scan a region of memory for a specific sequence of bytes
    /// </summary>
    /// <param name="pattern"></param>
    /// <param name="start"></param>
    /// <param name="end"></param>
    /// <returns></returns>
    [[nodiscard]] uintptr_t PatternScan(std::string_view pattern,
        uintptr_t start = 0x1000, uintptr_t end = 0x7FFFFFFF0000) const;

    /// <summary>
    /// Scan a region of memory for multiple sequences of bytes.
    /// This is faster than `PatternScan`, because it does the same amount of ReadProcessMemory for multiple patterns.
    /// </summary>
    /// <param name="patterns">A map of all the patterns. The key is the name, the value is the pattern itself.</param>
    /// <param name="start"></param>
    /// <param name="end"></param>
    /// <returns></returns>
    [[nodiscard]] std::unordered_map<std::string, uintptr_t> PatternScanMulti(
        const std::unordered_map<std::string_view, std::string_view>& patterns,
        uintptr_t start = 0x1000, uintptr_t end = 0x7FFFFFFF0000) const;

    [[nodiscard]] std::vector<CodeCave> FindCodeCaves(size_t minSize, bool stopAtFirst = false, uint32_t protection = ~0) const;
    [[nodiscard]] std::optional<CodeCave> FindCodeCave(size_t minSize, uint32_t protection = ~0) const;

    [[nodiscard]] std::optional<CodeCave> FindCodeCaveInModule(size_t minSize, const std::string& moduleName, uint32_t protection = ~0) const;
    [[nodiscard]] std::vector<CodeCave> FindCodeCavesInModule(size_t minSize, const std::string& module, bool stopAtFirst = false, uint32_t protection = ~0) const;
    [[nodiscard]] std::vector<CodeCave> FindCodeCavesInModules(size_t minSize, const std::unordered_set<std::string>& modules, uint32_t protection = ~0) const;

    template <typename T>
    [[nodiscard]] T Read(uintptr_t address) const;

    /// <summary>
    /// Read a sequence of bytes into a string
    /// </summary>
    /// <param name="address"></param>
    /// <param name="maxLength"></param>
    /// <returns></returns>
    [[nodiscard]] std::string ReadString(uintptr_t address, size_t maxLength = 256) const;

    /// <summary>
    /// Read an std::string. The std::string stores data differently depending on it's length.
    /// This makes sure it's read correctly.
    /// </summary>
    /// <param name="address"></param>
    /// <returns></returns>
    [[nodiscard]] std::string ReadStdString(uintptr_t address) const;

    template <typename T>
    [[nodiscard]] std::vector<T> ReadVector(uintptr_t address, size_t skip) const;

    template <typename T>
    [[nodiscard]] std::vector<T> ReadArray(uintptr_t address, size_t count) const;

    [[nodiscard]] std::vector<uint8_t> ReadBuffer(uintptr_t address, size_t size) const;

    /// <summary>
    /// Read a specific amount of memory into a buffer.
    /// </summary>
    /// <param name="address"></param>
    /// <param name="buffer"></param>
    /// <param name="size"></param>
    /// <returns>The amount of bytes that actually ended up being read.</returns>
    size_t ReadBuffer(uintptr_t address, void* buffer, size_t size) const;

    template <typename T>
    bool Write(uintptr_t address, const T& value) const;

    bool WriteBuffer(uintptr_t address, const void* buffer, size_t size) const;
    bool WriteBuffer(uintptr_t address, const std::vector<uint8_t>& buffer) const;

    bool WriteString(uintptr_t address, const std::string& string) const;

    [[nodiscard]] uintptr_t Allocate(size_t size, uint32_t protection, uint32_t allocationType = MEM_COMMIT | MEM_RESERVE) const;
    void Free(uintptr_t address, uint32_t freeType = MEM_DECOMMIT) const;

    [[nodiscard]] std::optional<MEMORY_BASIC_INFORMATION> Query(uintptr_t address) const;

    void Close();

    [[nodiscard]] bool IsValid() const { return m_Handle != nullptr; }
    [[nodiscard]] uint32_t GetPid() const { return m_Pid; }
    [[nodiscard]] HANDLE GetHandle() const { return m_Handle; }

private:
    uint32_t m_Pid = 0;
    HANDLE m_Handle = nullptr;

    std::vector<MODULEENTRY32> m_Modules;
    std::chrono::system_clock::time_point m_LastModuleUpdate;
};

template <typename T>
T Process::Read(uintptr_t address) const
{
    T buffer {};
    SIZE_T bytesRead;
    if (ReadProcessMemory(m_Handle, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), &bytesRead) && bytesRead == sizeof(T))
        return buffer;
    return T();
}

template <typename T>
std::vector<T> Process::ReadVector(uintptr_t address, size_t skip) const
{
    uintptr_t top = Read<uintptr_t>(address);
    uintptr_t end = Read<uintptr_t>(address + 0x8);
    uintptr_t elementCount = (end - top) / (sizeof(T) + skip);

    std::vector<T> elements(elementCount);
    for (uintptr_t ptr = top; ptr < end; ptr += sizeof(T) + skip) {
        uintptr_t addr = Read<uintptr_t>(ptr);
        if (!addr) {
            continue;
        }

        T element = Read<T>(addr);
        elements.push_back(element);
    }

    return elements;
}

template <typename T>
std::vector<T> Process::ReadArray(uintptr_t address, size_t count) const
{
    std::vector<T> elements(count);
    if (!ReadProcessMemory(m_Handle, reinterpret_cast<LPCVOID>(address), elements.data(), sizeof(T) * count, nullptr)) {
        return {};
    }
    return elements;
}

template <typename T>
bool Process::Write(uintptr_t address, const T& value) const
{
    SIZE_T bytesWritten;
    return WriteProcessMemory(m_Handle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), &bytesWritten) && bytesWritten == sizeof(T);
}
