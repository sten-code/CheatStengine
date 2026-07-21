#include "KernelSettings.h"

#include <CheatStengine/Process/Device.h>

#include <imgui.h>

#include <chrono>
#include <format>
#include <system_error>
#include <utility>

namespace {

const char* GetModeName(Protocol::LoadMode mode)
{
    switch (mode) {
        case Protocol::LoadMode::Pnp: return "PnP";
        case Protocol::LoadMode::Service: return "Service";
        case Protocol::LoadMode::Mapped: return "Mapped";
        default: return "Unknown";
    }
}

const char* GetStateName(Protocol::RuntimeState state)
{
    switch (state) {
        case Protocol::RuntimeState::Starting: return "Starting";
        case Protocol::RuntimeState::Ready: return "Ready";
        case Protocol::RuntimeState::Idle: return "Idle";
        case Protocol::RuntimeState::Stopping: return "Stopping";
        case Protocol::RuntimeState::Removed: return "Removed";
        default: return "Unknown";
    }
}

std::string GetErrorText(DWORD error)
{
    if (error == ERROR_SUCCESS) {
        return "None";
    }
    return std::format("{} ({})", std::system_category().message(error), error);
}

void DrawRow(const char* name, const std::string& value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", name);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value.c_str());
}

}

KernelSettings::KernelSettings(
    std::string name,
    Monitor& monitor,
    std::function<bool()> visible)
    : m_Name(std::move(name))
    , m_Monitor(monitor)
    , m_Visible(std::move(visible))
{
}

void KernelSettings::Draw()
{
    PollTasks();
    if (m_Visible && !m_Visible()) {
        return;
    }
    const MonitorSnapshot monitor = m_Monitor.GetSnapshot();
    const bool connectionChanged = monitor.Connected
        && monitor.Generation != m_LastPingGeneration;
    if ((!m_LastPing || connectionChanged) && !m_PingTask.valid()) {
        StartPing();
    }

    ImGui::PushID(m_Name.c_str());
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Kernel Mode");
    ImGui::Spacing();

    DrawStatus();

    const bool pinging = m_PingTask.valid();
    if (pinging) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(pinging ? "Testing..." : "Ping Test")) {
        StartPing();
    }
    if (pinging) {
        ImGui::EndDisabled();
    }

    int timeout = static_cast<int>(m_TempValue);
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.4f);
    if (ImGui::SliderInt(
            "Idle timeout",
            &timeout,
            static_cast<int>(Protocol::MinimumIdleTimeoutSeconds),
            static_cast<int>(Protocol::MaximumIdleTimeoutSeconds),
            "%d seconds")) {
        m_TempValue = static_cast<uint32_t>(timeout);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Time before the driver enters its idle state after the last client disconnects.");
    }

    if (m_ConfigureTask.valid() || m_QueuedConfigure) {
        ImGui::TextDisabled("Applying idle timeout...");
    } else if (m_LastConfigure) {
        if (m_LastConfigure->Success) {
            ImGui::TextDisabled("Idle timeout applied: %u seconds", m_LastConfigure->EffectiveSeconds);
        } else {
            const std::string error = GetErrorText(m_LastConfigure->Error);
            ImGui::TextColored(ImVec4 { 1.0f, 0.4f, 0.4f, 1.0f }, "Apply failed: %s", error.c_str());
        }
    }

    ImGui::PopID();
}

void KernelSettings::Restore()
{
    m_TempValue = m_Value;
}

void KernelSettings::Apply()
{
    m_Value = m_TempValue;
    if (!m_Visible || m_Visible()) {
        QueueConfigure(m_Value);
    }
}

std::string KernelSettings::GetName() const
{
    return m_Name;
}

std::string KernelSettings::GetDescription() const
{
    return {};
}

bool KernelSettings::HasValueChanged() const
{
    return m_Value != m_TempValue;
}

void KernelSettings::PollTasks()
{
    if (m_PingTask.valid()
        && m_PingTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_LastPing = m_PingTask.get();
        m_LastPingGeneration = m_PendingPingGeneration;
    }

    if (m_ConfigureTask.valid()
        && m_ConfigureTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        ConfigureResult result = m_ConfigureTask.get();
        if (result.Success) {
            m_Value = result.EffectiveSeconds;
            if (m_TempValue == result.RequestedSeconds) {
                m_TempValue = result.EffectiveSeconds;
            }
        }
        m_LastConfigure = result;
        if (result.Success && !m_PingTask.valid()) {
            StartPing();
        }
    }

    StartConfigure();
}

void KernelSettings::StartPing()
{
    if (m_PingTask.valid()) {
        return;
    }

    const uint64_t nonce = m_NextNonce++;
    const MonitorSnapshot monitor = m_Monitor.GetSnapshot();
    m_PendingPingGeneration = monitor.Connected ? monitor.Generation : 0;
    m_PingTask = std::async(std::launch::async, [nonce] {
        return RunPing(nonce);
    });
}

void KernelSettings::QueueConfigure(uint32_t seconds)
{
    m_QueuedConfigure = seconds;
    StartConfigure();
}

void KernelSettings::StartConfigure()
{
    if (m_ConfigureTask.valid() || !m_QueuedConfigure) {
        return;
    }

    const uint32_t seconds = *m_QueuedConfigure;
    m_QueuedConfigure.reset();
    m_ConfigureTask = std::async(std::launch::async, [seconds] {
        return RunConfigure(seconds);
    });
}

void KernelSettings::DrawStatus() const
{
    const MonitorSnapshot monitor = m_Monitor.GetSnapshot();
    Protocol::LoadMode mode = monitor.Mode;
    Protocol::RuntimeState state = monitor.State;
    DWORD error = monitor.Error;
    const bool currentPing = m_LastPing
        && m_LastPing->Response
        && (!monitor.Connected || m_LastPingGeneration == monitor.Generation);

    if (m_LastPing) {
        if (!monitor.Connected) {
            mode = m_LastPing->Mode;
        }
        if (!monitor.Connected || m_LastPingGeneration == monitor.Generation) {
            error = m_LastPing->Error;
        }
        if (!monitor.Connected && m_LastPing->Response) {
            state = static_cast<Protocol::RuntimeState>(m_LastPing->Response->State);
        }
    }

    if (ImGui::BeginTable("Status", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
        DrawRow(
            "Connection",
            monitor.Connected
                ? "Connected"
                : currentPing
                ? "Reachable"
                : monitor.Running ? "Connecting"
                                  : "Stopped");
        DrawRow("Load", GetModeName(mode));
        DrawRow("State", GetStateName(state));

        if (currentPing) {
            const Protocol::PingResponse& response = *m_LastPing->Response;
            DrawRow("Driver", std::format(
                                  "{}.{}.{}",
                                  response.DriverMajorVersion,
                                  response.DriverMinorVersion,
                                  response.DriverPatchVersion));
            DrawRow("Protocol", std::format(
                                    "{}.{}",
                                    response.Header.MajorVersion,
                                    response.Header.MinorVersion));
            DrawRow("Capabilities", std::format("0x{:016X}", response.Capabilities));
            DrawRow("Active sessions", std::to_string(response.ActiveSessions));
            DrawRow("Idle timeout", std::format("{} seconds", response.IdleTimeoutSeconds));
            DrawRow(
                "Idle remaining",
                std::format(
                    "{} seconds",
                    monitor.Connected ? monitor.RemainingIdleSeconds : response.RemainingIdleSeconds));
            DrawRow("RTT", std::format("{:.3f} ms", m_LastPing->Milliseconds));
        } else {
            DrawRow("Heartbeat", monitor.Sequence == 0 ? "Waiting" : std::format("#{}", monitor.Sequence));
            DrawRow("Idle remaining", std::format("{} seconds", monitor.RemainingIdleSeconds));
        }

        DrawRow("Error", GetErrorText(error));
        ImGui::EndTable();
    }

    ImGui::Spacing();
}

KernelSettings::PingResult KernelSettings::RunPing(uint64_t nonce)
{
    PingResult result;
    Device device;
    result.Mode = device.GetMode();
    if (!device.IsOpen()) {
        result.Error = device.GetError();
        return result;
    }

    const auto started = std::chrono::steady_clock::now();
    result.Response = device.Ping(nonce);
    const auto finished = std::chrono::steady_clock::now();
    result.Milliseconds = std::chrono::duration<double, std::milli>(finished - started).count();
    result.Error = device.GetError();
    if (result.Response) {
        result.Mode = static_cast<Protocol::LoadMode>(result.Response->LoadMode);
    }
    return result;
}

KernelSettings::ConfigureResult KernelSettings::RunConfigure(uint32_t seconds)
{
    ConfigureResult result;
    result.RequestedSeconds = seconds;

    Device device;
    if (!device.IsOpen()) {
        result.Error = device.GetError();
        return result;
    }

    const std::optional<Protocol::ConfigureResponse> response = device.Configure(seconds);
    result.Error = device.GetError();
    if (response) {
        result.EffectiveSeconds = response->IdleTimeoutSeconds;
        result.Success = true;
    }
    return result;
}
