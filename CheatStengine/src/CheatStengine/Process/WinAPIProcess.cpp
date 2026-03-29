#include "WinAPIProcess.h"

#include <Psapi.h>
#include <TlHelp32.h>
#include <unordered_set>

WinAPIProcess::WinAPIProcess(DWORD pid)
    : Process(pid)
    , m_Handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid))
{
}

WinAPIProcess::WinAPIProcess(const std::string& procName)
    : Process(0)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snapshot, &entry)) {
        do {
            if (strncmp(entry.szExeFile, procName.c_str(), procName.size()) == 0) {
                m_Pid = entry.th32ProcessID;
                m_Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_Pid);
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

WinAPIProcess::~WinAPIProcess()
{
}

uintptr_t WinAPIProcess::Allocate(size_t size, uint32_t protection, uint32_t allocationType) const
{
    return reinterpret_cast<uintptr_t>(VirtualAllocEx(m_Handle, nullptr, size, allocationType, protection));
}

void WinAPIProcess::Free(uintptr_t address, uint32_t freeType) const
{
    VirtualFreeEx(m_Handle, reinterpret_cast<LPVOID>(address), 0, freeType);
}

std::optional<uint32_t> WinAPIProcess::Protect(uintptr_t address, size_t size, uint32_t protection) const
{
    DWORD oldProtect;
    if (!VirtualProtectEx(m_Handle, reinterpret_cast<LPVOID>(address), size, protection, &oldProtect)) {
        return std::nullopt;
    }
    return oldProtect;
}

std::optional<MEMORY_BASIC_INFORMATION> WinAPIProcess::Query(uintptr_t address) const
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQueryEx(m_Handle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(MEMORY_BASIC_INFORMATION)) == 0) {
        return std::nullopt;
    }
    return mbi;
}

bool WinAPIProcess::ReadBuffer(uintptr_t address, void* buffer, size_t size) const
{
    SIZE_T bytesRead;
    return ReadProcessMemory(m_Handle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
}

bool WinAPIProcess::WriteBuffer(uintptr_t address, const void* buffer, size_t size) const
{
    SIZE_T bytesWritten;
    return WriteProcessMemory(m_Handle, reinterpret_cast<LPVOID>(address), buffer, size, &bytesWritten) && bytesWritten == size;
}

MODULEENTRY32 WinAPIProcess::GetModuleEntry(std::string_view name) const
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_Pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    MODULEENTRY32 entry;
    entry.dwSize = sizeof(entry);

    if (Module32First(snapshot, &entry)) {
        do {
            if (_stricmp(entry.szModule, name.data()) == 0) {
                CloseHandle(snapshot);
                return entry;
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return {};
}

std::vector<MODULEENTRY32> WinAPIProcess::GetModuleEntries(bool refresh) const
{
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
            if (seenModules.insert(moduleName).second) {
                modules.push_back(entry);
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return modules;
}

std::string WinAPIProcess::GetName()
{
    char buffer[MAX_PATH];
    if (GetModuleBaseNameA(m_Handle, nullptr, buffer, sizeof(buffer))) {
        return buffer;
    }
    return {};
}