#include "Monitor.h"

#include "Device.h"

#include <chrono>
#include <limits>
#include <memory>

using namespace std::chrono_literals;

Monitor::~Monitor()
{
    Stop();
}

void Monitor::Start()
{
    std::scoped_lock lock(m_ThreadMutex);
    if (m_Thread.joinable()) {
        return;
    }

    MonitorSnapshot snapshot;
    snapshot.Running = true;
    SetSnapshot(snapshot);
    m_Running.store(true, std::memory_order_release);
    m_Thread = std::jthread([this](std::stop_token stopToken) {
        Run(stopToken);
    });
}

void Monitor::Stop()
{
    std::scoped_lock lock(m_ThreadMutex);
    if (m_Thread.joinable()) {
        m_Thread.request_stop();
        m_Wake.notify_all();
        m_Thread.join();
    }

    m_Running.store(false, std::memory_order_release);
    MonitorSnapshot snapshot = GetSnapshot();
    snapshot.Running = false;
    snapshot.Connected = false;
    snapshot.Mode = Protocol::LoadMode::Unknown;
    SetSnapshot(snapshot);
}

bool Monitor::IsRunning() const
{
    return m_Running.load(std::memory_order_acquire);
}

MonitorSnapshot Monitor::GetSnapshot() const
{
    std::scoped_lock lock(m_SnapshotMutex);
    return m_Snapshot;
}

void Monitor::Run(std::stop_token stopToken)
{
    std::unique_ptr<Device> device;
    while (!stopToken.stop_requested()) {
        bool newConnection = false;
        if (!device) {
            device = std::make_unique<Device>();
            newConnection = device->IsOpen();
        }

        MonitorSnapshot snapshot = GetSnapshot();
        snapshot.Running = true;
        snapshot.Connected = device->IsOpen();
        snapshot.Mode = device->GetMode();
        snapshot.Error = device->GetError();

        if (snapshot.Connected) {
            const uint64_t sequence = m_NextSequence.fetch_add(1, std::memory_order_relaxed);
            if (sequence == 0
                || sequence > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                snapshot.Connected = false;
                snapshot.Error = ERROR_ARITHMETIC_OVERFLOW;
                SetSnapshot(snapshot);
                break;
            }

            const std::optional<Protocol::HeartbeatResponse> response = device->Heartbeat(sequence);
            if (response) {
                if (newConnection) {
                    snapshot.Generation = m_NextGeneration.fetch_add(1, std::memory_order_relaxed);
                }
                snapshot.Connected = true;
                snapshot.Sequence = response->Sequence;
                snapshot.KernelTime = response->KernelTime;
                snapshot.RemainingIdleSeconds = response->RemainingIdleSeconds;
                snapshot.State = static_cast<Protocol::RuntimeState>(response->State);
                snapshot.Error = ERROR_SUCCESS;
            } else {
                snapshot.Connected = false;
                snapshot.Error = device->GetError();
                device.reset();
            }
        } else {
            device.reset();
        }

        SetSnapshot(snapshot);
        if (Wait(stopToken)) {
            break;
        }
    }
}

bool Monitor::Wait(std::stop_token stopToken)
{
    std::unique_lock lock(m_WaitMutex);
    return m_Wake.wait_for(lock, 5s, [&stopToken] {
        return stopToken.stop_requested();
    });
}

void Monitor::SetSnapshot(const MonitorSnapshot& snapshot)
{
    std::scoped_lock lock(m_SnapshotMutex);
    m_Snapshot = snapshot;
}
