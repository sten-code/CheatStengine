#pragma once

#include <Protocol.h>

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

struct MonitorSnapshot {
    bool Running = false;
    bool Connected = false;
    Protocol::LoadMode Mode = Protocol::LoadMode::Unknown;
    Protocol::RuntimeState State = Protocol::RuntimeState::Starting;
    uint64_t Sequence = 0;
    uint64_t Generation = 0;
    uint64_t KernelTime = 0;
    uint32_t RemainingIdleSeconds = 0;
    DWORD Error = ERROR_SUCCESS;
};

class Monitor final {
public:
    Monitor() = default;
    ~Monitor();

    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;
    Monitor(Monitor&&) = delete;
    Monitor& operator=(Monitor&&) = delete;

    void Start();
    void Stop();

    [[nodiscard]] bool IsRunning() const;
    [[nodiscard]] MonitorSnapshot GetSnapshot() const;

private:
    void Run(std::stop_token stopToken);
    [[nodiscard]] bool Wait(std::stop_token stopToken);
    void SetSnapshot(const MonitorSnapshot& snapshot);

private:
    mutable std::mutex m_ThreadMutex;
    std::jthread m_Thread;
    std::atomic<bool> m_Running = false;
    std::atomic<uint64_t> m_NextSequence = 1;
    std::atomic<uint64_t> m_NextGeneration = 1;

    mutable std::mutex m_SnapshotMutex;
    MonitorSnapshot m_Snapshot;

    std::mutex m_WaitMutex;
    std::condition_variable m_Wake;
};
