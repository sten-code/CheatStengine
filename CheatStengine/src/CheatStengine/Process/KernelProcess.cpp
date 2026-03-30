#include "KernelProcess.h"

#include <Engine/Core/Log.h>
#include <unordered_set>

#define IOCTL_CS_COMMAND CTL_CODE(FILE_DEVICE_UNKNOWN, 0x6969, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct CommandHeader {
    enum Type : uint32_t {
        ReadMemory = 0,
        WriteMemory = 1,
        QueryMemory = 2,
        AllocateMemory = 3,
        FreeMemory = 4,
        ProtectMemory = 5,
    } Type;

    uint32_t Pid;

    union {
        struct
        {
            uintptr_t Address;
            size_t Size;
            void* Buffer;
        } ReadMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            void* Buffer;
        } WriteMemoryData;

        struct
        {
            uintptr_t Address;
            MEMORY_BASIC_INFORMATION* Mbi;
        } QueryMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            uint32_t AllocationType;
            uint32_t Protect;
            uintptr_t* AllocatedAddressPtr;
        } AllocateMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            uint32_t FreeType;
        } FreeMemoryData;

        struct
        {
            uintptr_t Address;
            size_t Size;
            uint32_t NewProtect;
            uint32_t* OldProtectPtr;
        } ProtectMemoryData;
    };
};

static bool SendBuffer(HANDLE deviceHandle, CommandHeader* command, void* outputBuffer = nullptr, size_t outputBufferSize = 0)
{
    DWORD bytesReturned = 0;
    BOOL result = FALSE;
    result = DeviceIoControl(
        deviceHandle,
        IOCTL_CS_COMMAND,
        command,
        sizeof(CommandHeader),
        outputBuffer,
        outputBufferSize,
        &bytesReturned,
        nullptr);
    if (!result) {
        ERR("DeviceIoControl failed. Error Code: {}", GetLastError());
        return false;
    }

    return true;
}

KernelProcess::KernelProcess(DWORD pid)
    : Process(pid)
{
    m_DeviceHandle = CreateFileW(
        L"\\\\.\\CheatStengineDriver",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (m_DeviceHandle == INVALID_HANDLE_VALUE) {
        ERR("Failed to open handle to driver. Error Code: {}", GetLastError());
    }
}

KernelProcess::KernelProcess(const std::string& procName)
    : KernelProcess(0)
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
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

KernelProcess::~KernelProcess()
{
}

uintptr_t KernelProcess::Allocate(size_t size, uint32_t protection, uint32_t allocationType) const
{
    if (!IsValid()) {
        return 0;
    }

    CommandHeader command {};
    command.Type = CommandHeader::Type::AllocateMemory;
    command.Pid = m_Pid;
    command.AllocateMemoryData.Address = 0;
    command.AllocateMemoryData.Size = size;
    command.AllocateMemoryData.AllocationType = allocationType;
    command.AllocateMemoryData.Protect = protection;

    uintptr_t allocatedAddress = 0;
    command.AllocateMemoryData.AllocatedAddressPtr = &allocatedAddress;

    SendBuffer(m_DeviceHandle, &command);
    return allocatedAddress;
}

void KernelProcess::Free(uintptr_t address, uint32_t freeType) const
{
    if (!IsValid()) {
        return;
    }

    CommandHeader command {};
    command.Type = CommandHeader::Type::FreeMemory;
    command.Pid = m_Pid;
    command.FreeMemoryData.Address = address;
    command.FreeMemoryData.Size = 0;
    command.FreeMemoryData.FreeType = freeType;
    SendBuffer(m_DeviceHandle, &command);
}

std::optional<MEMORY_BASIC_INFORMATION> KernelProcess::Query(uintptr_t address) const
{
    if (!IsValid()) {
        return std::nullopt;
    }

    CommandHeader command {};
    command.Type = CommandHeader::Type::QueryMemory;
    command.Pid = m_Pid;
    command.QueryMemoryData.Address = address;
    MEMORY_BASIC_INFORMATION mbi {};
    if (!SendBuffer(m_DeviceHandle, &command, &mbi, sizeof(MEMORY_BASIC_INFORMATION))) {
        return std::nullopt;
    }
    return mbi;
}

std::optional<uint32_t> KernelProcess::Protect(uintptr_t address, size_t size, uint32_t protection) const
{
    if (!IsValid()) {
        return false;
    }

    CommandHeader command {};
    command.Type = CommandHeader::Type::ProtectMemory;
    command.Pid = m_Pid;
    command.ProtectMemoryData.Address = address;
    command.ProtectMemoryData.Size = size;
    command.ProtectMemoryData.NewProtect = protection;

    uint32_t oldProtect = 0;
    command.ProtectMemoryData.OldProtectPtr = &oldProtect;
    if (!SendBuffer(m_DeviceHandle, &command)) {
        return std::nullopt;
    }
    return oldProtect;
}

bool KernelProcess::ReadBuffer(uintptr_t address, void* buffer, size_t size) const
{
    if (!IsValid()) {
        return false;
    }

    CommandHeader command {};
    command.Type = CommandHeader::Type::ReadMemory;
    command.Pid = m_Pid;
    command.ReadMemoryData.Address = address;
    command.ReadMemoryData.Size = size;
    command.ReadMemoryData.Buffer = buffer;
    return SendBuffer(m_DeviceHandle, &command);
}

bool KernelProcess::WriteBuffer(uintptr_t address, const void* buffer, size_t size) const
{
    if (!IsValid()) {
        return false;
    }

    CommandHeader command {};
    command.Type = CommandHeader::Type::WriteMemory;
    command.Pid = m_Pid;
    command.WriteMemoryData.Address = address;
    command.WriteMemoryData.Size = size;
    command.WriteMemoryData.Buffer = const_cast<void*>(buffer); // It's up to the kernel driver to ensure this is not written to.
    return SendBuffer(m_DeviceHandle, &command);
}

MODULEENTRY32 KernelProcess::GetModuleEntry(std::string_view name) const
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

std::vector<MODULEENTRY32> KernelProcess::GetModuleEntries(bool refresh) const
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

std::string KernelProcess::GetName()
{
    if (!m_Name.empty()) {
        return m_Name;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return "";

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe)) {
        do {
            if (pe.th32ProcessID == m_Pid) {
                CloseHandle(snapshot);
                m_Name = pe.szExeFile;
                return pe.szExeFile;
            }
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return {};
}
