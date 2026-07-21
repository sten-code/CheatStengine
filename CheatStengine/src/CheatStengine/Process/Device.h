#pragma once

#include <Protocol.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

class Device final {
public:
    Device();
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] bool IsOpen() const;
    [[nodiscard]] bool Bind(uint32_t processId) const;

    [[nodiscard]] std::optional<Protocol::PingResponse> Ping(uint64_t nonce = 0) const;
    [[nodiscard]] std::optional<Protocol::HeartbeatResponse> Heartbeat(uint64_t sequence) const;
    [[nodiscard]] std::optional<Protocol::ConfigureResponse> Configure(uint32_t idleTimeoutSeconds) const;

    [[nodiscard]] bool Read(uintptr_t address, void* buffer, size_t size) const;
    [[nodiscard]] bool Write(uintptr_t address, const void* buffer, size_t size) const;

    [[nodiscard]] std::optional<MEMORY_BASIC_INFORMATION> Query(uintptr_t address) const;
    [[nodiscard]] uintptr_t Allocate(
        size_t size,
        uint32_t protection,
        uint32_t allocationType = MEM_COMMIT | MEM_RESERVE,
        uintptr_t preferredAddress = 0) const;
    [[nodiscard]] bool Free(
        uintptr_t address,
        uint32_t freeType = MEM_RELEASE,
        size_t size = 0) const;
    [[nodiscard]] std::optional<uint32_t> Protect(
        uintptr_t address,
        size_t size,
        uint32_t protection) const;

    [[nodiscard]] Protocol::LoadMode GetMode() const;
    [[nodiscard]] uint64_t GetCapabilities() const;
    [[nodiscard]] uint64_t GetMaximumTransferSize() const;
    [[nodiscard]] DWORD GetError() const;

private:
    [[nodiscard]] bool Open();
    [[nodiscard]] bool OpenPath(const std::wstring& path, Protocol::LoadMode mode);
    [[nodiscard]] bool ReadInformation();
    [[nodiscard]] bool HasCapability(uint64_t capability) const;
    [[nodiscard]] bool Control(
        DWORD code,
        const void* input,
        DWORD inputSize,
        const void* directInput,
        void* output,
        DWORD bufferSize,
        DWORD& transferred) const;

    void Close();
    void CloseLocked() const;
    void SetError(DWORD error) const;

private:
    mutable std::mutex m_ControlMutex;
    mutable HANDLE m_Handle = INVALID_HANDLE_VALUE;
    mutable std::atomic<Protocol::LoadMode> m_Mode = Protocol::LoadMode::Unknown;
    mutable std::atomic<uint64_t> m_Capabilities = 0;
    mutable std::atomic<uint64_t> m_MaximumTransferSize = 0;
    mutable std::atomic<DWORD> m_Error = ERROR_SUCCESS;
};
