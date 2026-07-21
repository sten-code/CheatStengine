#pragma once

#include "Setting.h"

#include <CheatStengine/Process/Monitor.h>

#include <Protocol.h>

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>

class KernelSettings final : public Setting {
public:
    KernelSettings(std::string name, Monitor& monitor, std::function<bool()> visible);

    void Draw() override;
    void Restore() override;
    void Apply() override;

    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] std::string GetDescription() const override;
    [[nodiscard]] bool HasValueChanged() const override;

private:
    struct PingResult {
        std::optional<Protocol::PingResponse> Response;
        Protocol::LoadMode Mode = Protocol::LoadMode::Unknown;
        DWORD Error = ERROR_SUCCESS;
        double Milliseconds = 0.0;
    };

    struct ConfigureResult {
        uint32_t RequestedSeconds = 0;
        uint32_t EffectiveSeconds = 0;
        DWORD Error = ERROR_SUCCESS;
        bool Success = false;
    };

    void PollTasks();
    void StartPing();
    void QueueConfigure(uint32_t seconds);
    void StartConfigure();
    void DrawStatus() const;

    [[nodiscard]] static PingResult RunPing(uint64_t nonce);
    [[nodiscard]] static ConfigureResult RunConfigure(uint32_t seconds);

private:
    std::string m_Name;
    Monitor& m_Monitor;
    std::function<bool()> m_Visible;
    uint32_t m_Value = Protocol::DefaultIdleTimeoutSeconds;
    uint32_t m_TempValue = Protocol::DefaultIdleTimeoutSeconds;
    uint64_t m_NextNonce = 1;
    uint64_t m_PendingPingGeneration = 0;
    uint64_t m_LastPingGeneration = 0;

    std::future<PingResult> m_PingTask;
    std::optional<PingResult> m_LastPing;

    std::future<ConfigureResult> m_ConfigureTask;
    std::optional<uint32_t> m_QueuedConfigure;
    std::optional<ConfigureResult> m_LastConfigure;
};
