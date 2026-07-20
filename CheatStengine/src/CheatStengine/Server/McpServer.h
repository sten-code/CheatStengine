#pragma once

#include <CheatStengine/Core/State.h>
#include <CheatStengine/Server/AuthManager.h>
#include <CheatStengine/Server/CommandQueue.h>
#include <CheatStengine/Server/JobManager.h>
#include <CheatStengine/Server/SessionManager.h>
#include <CheatStengine/Server/ToolRegistry.h>
#include <CheatStengine/Tools/MemoryScanner.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <thread>

namespace Server {

    class McpServer {
    public:
        McpServer();
        ~McpServer();

        McpServer(const McpServer&) = delete;
        McpServer& operator=(const McpServer&) = delete;

         bool Start(State& state, const std::string& host = "127.0.0.1", uint16_t port = 13777, uint16_t portAttempts = 32);

        void Stop();

        [[nodiscard]] bool IsRunning() const { return m_Running; }

        void DrainCommands() { m_CommandQueue.Drain(); }

        [[nodiscard]] const std::string& GetToken() const { return m_Auth.GetToken(); }
        [[nodiscard]] uint16_t GetPort() const { return m_Port; }
        [[nodiscard]] const std::string& GetUrl() const { return m_Url; }
        void SetAuthRequired(bool required) { m_Auth.SetRequired(required); }
        [[nodiscard]] bool IsAuthRequired() const { return m_Auth.IsRequired(); }

        [[nodiscard]] nlohmann::json DescribeTools() const { return m_Tools.DescribeAll(); }
        bool SetToolEnabled(const std::string& name, bool enabled) { return m_Tools.SetEnabled(name, enabled); }

        void ReinstallClients() const;

    private:
        void RegisterRoutes();
        [[nodiscard]] nlohmann::json HandleRpc(const nlohmann::json& request);
        [[nodiscard]] nlohmann::json HandleInitialize(const nlohmann::json& params);
        [[nodiscard]] nlohmann::json HandleToolsCall(const nlohmann::json& params);
        [[nodiscard]] nlohmann::json DispatchToolCall(const std::string& name, const nlohmann::json& arguments);
        [[nodiscard]] static nlohmann::json MakeError(const nlohmann::json& id, int code, const std::string& message);

        State* m_State = nullptr;

        AuthManager m_Auth;
        SessionManager m_Sessions;
        ToolRegistry m_Tools;
        CommandQueue m_CommandQueue;
        JobManager m_Jobs;

        std::unique_ptr<MemoryScanner> m_Scanner;

        httplib::Server m_Http;
        std::thread m_ListenThread;
        std::string m_Host;
        std::string m_Url;
        uint16_t m_Port = 0;
        std::atomic<bool> m_Running = false;
        bool m_RoutesRegistered = false;
    };

}
