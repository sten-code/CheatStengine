#pragma once

#include <Windows.h>

#include <TlHelp32.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class ProcessMode : uint8_t {
    WinAPI,
    Kernel,
};

class Process {
public:
    struct Window {
        HWND Handle;
        uint32_t Pid;
        std::string Title;
    };

    explicit Process(DWORD pid);
    virtual ~Process() = default;

    [[nodiscard]] virtual uintptr_t Allocate(size_t size, uint32_t protection, uint32_t allocationType = MEM_COMMIT | MEM_RESERVE) const = 0;
    virtual bool Free(uintptr_t address, uint32_t freeType = MEM_DECOMMIT) const = 0;

    [[nodiscard]] virtual std::optional<MEMORY_BASIC_INFORMATION> Query(uintptr_t address) const = 0;
    virtual std::optional<uint32_t> Protect(uintptr_t address, size_t size, uint32_t protection) const = 0;

    virtual bool ReadBuffer(uintptr_t address, void* buffer, size_t size) const = 0;
    virtual bool WriteBuffer(uintptr_t address, const void* buffer, size_t size) const = 0;

    [[nodiscard]] virtual MODULEENTRY32 GetModuleEntry(std::string_view name) const = 0;
    [[nodiscard]] virtual std::vector<MODULEENTRY32> GetModuleEntries(bool refresh = false) const = 0;

    [[nodiscard]] virtual std::string GetName() = 0;

    // Implemented methods that rely on the above virtual methods

    [[nodiscard]] bool IsAddressReadable(uintptr_t address) const;

    template <typename T>
    [[nodiscard]] T Read(uintptr_t address) const
    {
        T value {};
        if (!ReadBuffer(address, &value, sizeof(T))) {
            return {};
        }
        return value;
    }

    template <typename T>
    bool Write(uintptr_t address, const T& value) const
    {
        return WriteBuffer(address, &value, sizeof(T));
    }

    std::vector<uint8_t> ReadBytes(uintptr_t address, size_t size) const;

    [[nodiscard]] std::string ReadString(uintptr_t address, size_t maxLength = 256) const;

    template <typename T>
    [[nodiscard]] std::vector<T> ReadArray(uintptr_t address, size_t count) const
    {
        std::vector<T> elements(count);
        if (!ReadBuffer(address, elements.data(), sizeof(T) * count)) {
            return {};
        }
        return elements;
    }

    template <typename T>
    [[nodiscard]] std::vector<T> ReadVector(uintptr_t address, size_t stride = 0) const
    {
        uintptr_t top = Read<uintptr_t>(address);
        uintptr_t end = Read<uintptr_t>(address + 0x8);

        size_t elementSize = sizeof(T) + stride;
        uintptr_t elementCount = (end - top) / elementSize;
        std::vector<T> elements(elementCount);
        for (uintptr_t ptr = top; ptr < end; ptr += elementSize) {
            uintptr_t addr = Read<uintptr_t>(ptr);
            if (!addr) {
                continue;
            }

            T element = Read<T>(addr);
            elements.push_back(element);
        }

        return elements;
    }

    [[nodiscard]] uintptr_t GetModuleProc(uintptr_t moduleBase, const std::string& functionName) const;

    [[nodiscard]] virtual bool IsValid() const { return m_Pid > 0; }
    [[nodiscard]] uint32_t GetPid() const { return m_Pid; }

    static std::vector<PROCESSENTRY32> EnumerateProcesses();
    static std::vector<Window> EnumerateWindows();
    static std::vector<Window> EnumerateApplications();
    static std::unique_ptr<Process> Create(DWORD pid, ProcessMode mode);
    static std::unique_ptr<Process> Create(const std::string& procName, ProcessMode mode);

protected:
    DWORD m_Pid = 0;
};