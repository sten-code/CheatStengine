#include "KernelProcess.h"

#include <Engine/Core/Log.h>

#include <unordered_set>

namespace {

DWORD FindProcessId(std::string_view processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    const std::string name(processName);
    DWORD processId = 0;
    PROCESSENTRY32 entry {};
    entry.dwSize = sizeof(entry);
    if (Process32First(snapshot, &entry)) {
        do {
            if (_stricmp(entry.szExeFile, name.c_str()) == 0) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return processId;
}

}

KernelProcess::KernelProcess(DWORD pid)
    : Process(pid)
{
    if (Process::IsValid()) {
        m_Bound = m_Device.Bind(m_Pid);
        if (!m_Bound) {
            ERR("Failed to bind the driver to process {}: {}", m_Pid, m_Device.GetError());
        }
    }
}

KernelProcess::KernelProcess(const std::string& procName)
    : Process(FindProcessId(procName))
{
    if (Process::IsValid()) {
        m_Bound = m_Device.Bind(m_Pid);
        if (!m_Bound) {
            ERR("Failed to bind the driver to process {}: {}", m_Pid, m_Device.GetError());
        }
    }
}

KernelProcess::~KernelProcess() = default;

uintptr_t KernelProcess::Allocate(size_t size, uint32_t protection, uint32_t allocationType) const
{
    if (!IsValid() || size == 0) {
        return 0;
    }
    return m_Device.Allocate(size, protection, allocationType);
}

bool KernelProcess::Free(uintptr_t address, uint32_t freeType) const
{
    if (!IsValid() || address == 0) {
        return false;
    }
    return m_Device.Free(address, freeType);
}

std::optional<MEMORY_BASIC_INFORMATION> KernelProcess::Query(uintptr_t address) const
{
    if (!IsValid()) {
        return std::nullopt;
    }
    return m_Device.Query(address);
}

std::optional<uint32_t> KernelProcess::Protect(
    uintptr_t address,
    size_t size,
    uint32_t protection) const
{
    if (!IsValid() || address == 0 || size == 0) {
        return std::nullopt;
    }
    return m_Device.Protect(address, size, protection);
}

bool KernelProcess::ReadBuffer(uintptr_t address, void* buffer, size_t size) const
{
    if (!IsValid()) {
        return false;
    }
    return m_Device.Read(address, buffer, size);
}

bool KernelProcess::WriteBuffer(uintptr_t address, const void* buffer, size_t size) const
{
    if (!IsValid()) {
        return false;
    }
    return m_Device.Write(address, buffer, size);
}

MODULEENTRY32 KernelProcess::GetModuleEntry(std::string_view name) const
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_Pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    const std::string moduleName(name);
    MODULEENTRY32 entry {};
    entry.dwSize = sizeof(entry);
    if (Module32First(snapshot, &entry)) {
        do {
            if (_stricmp(entry.szModule, moduleName.c_str()) == 0) {
                CloseHandle(snapshot);
                return entry;
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return {};
}

std::vector<MODULEENTRY32> KernelProcess::GetModuleEntries(bool refresh) const
{
    static_cast<void>(refresh);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_Pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    MODULEENTRY32 entry {};
    entry.dwSize = sizeof(entry);
    std::vector<MODULEENTRY32> modules;
    std::unordered_set<std::string> seenModules;
    if (Module32First(snapshot, &entry)) {
        do {
            if (seenModules.insert(entry.szModule).second) {
                modules.push_back(entry);
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return modules;
}

std::string KernelProcess::GetName()
{
    if (!m_Name.empty()) {
        return m_Name;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32 entry {};
    entry.dwSize = sizeof(entry);
    if (Process32First(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == m_Pid) {
                m_Name = entry.szExeFile;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return m_Name;
}
