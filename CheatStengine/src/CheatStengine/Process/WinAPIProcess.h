#pragma once

#include "Process.h"

class WinAPIProcess final : public Process {
public:
    explicit WinAPIProcess(DWORD pid);
    explicit WinAPIProcess(const std::string& procName);
    ~WinAPIProcess() override;

    [[nodiscard]] uintptr_t Allocate(size_t size, uint32_t protection, uint32_t allocationType = MEM_COMMIT | MEM_RESERVE) const override;
    void Free(uintptr_t address, uint32_t freeType = MEM_DECOMMIT) const override;

    [[nodiscard]] std::optional<MEMORY_BASIC_INFORMATION> Query(uintptr_t address) const override;
    std::optional<uint32_t> Protect(uintptr_t address, size_t size, uint32_t protection) const override;

    bool ReadBuffer(uintptr_t address, void* buffer, size_t size) const override;
    bool WriteBuffer(uintptr_t address, const void* buffer, size_t size) const override;

    [[nodiscard]] MODULEENTRY32 GetModuleEntry(std::string_view name) const override;
    [[nodiscard]] std::vector<MODULEENTRY32> GetModuleEntries(bool refresh = false) const override;

    [[nodiscard]] std::string GetName() const override;

private:
    HANDLE m_Handle = nullptr;
};