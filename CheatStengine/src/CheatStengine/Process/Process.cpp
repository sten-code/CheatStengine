#include "Process.h"

#include "WinAPIProcess.h"

#include <Psapi.h>

static BOOL CALLBACK EnumWindowsToVector(HWND hwnd, LPARAM lParam)
{
    std::vector<Process::Window>* handles = reinterpret_cast<std::vector<Process::Window>*>(lParam);
    uint32_t pid;
    GetWindowThreadProcessId(hwnd, reinterpret_cast<LPDWORD>(&pid));
    char title[256];
    GetWindowTextA(hwnd, title, 256);
    handles->emplace_back(hwnd, pid, title);
    return TRUE;
}

Process::Process(DWORD pid)
    : m_Pid(pid)
{
}

bool Process::IsAddressReadable(uintptr_t address) const
{
    const std::optional<MEMORY_BASIC_INFORMATION> mbi = Query(address);
    if (!mbi) {
        return false;
    }

    return mbi->Protect & (PAGE_WRITECOPY | PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_GRAPHICS_READONLY | PAGE_GRAPHICS_READWRITE | PAGE_GRAPHICS_EXECUTE_READ | PAGE_GRAPHICS_EXECUTE_READWRITE);
}

std::vector<uint8_t> Process::ReadBytes(uintptr_t address, size_t size) const
{
    std::vector<uint8_t> buffer;
    buffer.resize(size);
    if (!ReadBuffer(address, buffer.data(), size)) {
        return {};
    }
    return buffer;
}

std::string Process::ReadString(uintptr_t address, size_t maxLength) const
{
    std::string result;
    result.resize(maxLength);
    if (!ReadBuffer(address, result.data(), maxLength)) {
        return {};
    }
    size_t length = strnlen(result.c_str(), maxLength);
    result.resize(length);
    return result;
}

uintptr_t Process::GetModuleProc(uintptr_t moduleBase, const std::string& functionName) const
{
    IMAGE_DOS_HEADER dosHeader = Read<IMAGE_DOS_HEADER>(moduleBase);
    IMAGE_NT_HEADERS ntHeaders = Read<IMAGE_NT_HEADERS>(moduleBase + dosHeader.e_lfanew);

    const IMAGE_DATA_DIRECTORY& exportDataDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (exportDataDir.VirtualAddress == 0 || exportDataDir.Size == 0) {
        return 0;
    }

    IMAGE_EXPORT_DIRECTORY exportDir = Read<IMAGE_EXPORT_DIRECTORY>(moduleBase + exportDataDir.VirtualAddress);

    std::vector<DWORD> nameRVAs = ReadArray<DWORD>(moduleBase + exportDir.AddressOfNames, exportDir.NumberOfNames);
    std::vector<WORD> ordinals = ReadArray<WORD>(moduleBase + exportDir.AddressOfNameOrdinals, exportDir.NumberOfNames);
    std::vector<DWORD> functionRVAs = ReadArray<DWORD>(moduleBase + exportDir.AddressOfFunctions, exportDir.NumberOfFunctions);
    if (nameRVAs.empty() || ordinals.empty() || functionRVAs.empty()) {
        return 0;
    }

    for (DWORD i = 0; i < exportDir.NumberOfNames; ++i) {
        std::string name = ReadString(moduleBase + nameRVAs[i]);
        if (name == functionName) {
            WORD ordinal = ordinals[i];
            if (ordinal >= functionRVAs.size()) {
                return 0;
            }
            DWORD functionRVA = functionRVAs[ordinal];
            return moduleBase + functionRVA;
        }
    }

    return 0;
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

std::unique_ptr<Process> Process::Create(DWORD pid, ProcessMode mode)
{
    switch (mode) {
        case ProcessMode::WinAPI: return std::make_unique<WinAPIProcess>(pid);
        default: return nullptr;
    }
}

std::unique_ptr<Process> Process::Create(const std::string& procName, ProcessMode mode)
{
    switch (mode) {
        case ProcessMode::WinAPI: return std::make_unique<WinAPIProcess>(procName);
        default: return nullptr;
    }
}