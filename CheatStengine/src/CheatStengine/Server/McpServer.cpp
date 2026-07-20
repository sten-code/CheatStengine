#include "McpServer.h"

#include <CheatStengine/Server/McpInstaller.h>
#include <CheatStengine/Server/WebUI.h>
#include <Engine/Core/Log.h>

#include <chrono>
#include <exception>
#include <format>
#include <fstream>
#include <iterator>

namespace Server {

    static constexpr const char* kProtocolVersion = "0.1.45";

    McpServer::McpServer() = default;

    McpServer::~McpServer()
    {
        Stop();
    }

    bool McpServer::Start(State& state, const std::string& host, uint16_t port, uint16_t portAttempts)
    {
        if (m_Running) {
            return true;
        }

        m_State = &state;
        m_Host = host;

        m_CommandQueue.Reset();

        if (!m_RoutesRegistered) {
            RegisterRoutes();
            m_RoutesRegistered = true;
        }

        uint16_t bound = 0;
        for (uint16_t offset = 0; offset < portAttempts; ++offset) {
            int candidate = static_cast<int>(port) + offset;
            if (candidate > 65535) {
                break; // don't wrap past the valid port range
            }
            if (m_Http.bind_to_port(m_Host.c_str(), static_cast<uint16_t>(candidate))) {
                bound = static_cast<uint16_t>(candidate);
                break;
            }
        }

        if (bound == 0) {
            ERR("MCP server could not bind any port in [{}, {}]", port, port + portAttempts - 1);
            return false;
        }

        m_Port = bound;
        m_Url = std::format("http://{}:{}", m_Host, m_Port);

        m_Running = true;
        m_ListenThread = std::thread([this] {
            m_Http.listen_after_bind();
        });

        INFO("MCP control server listening on {}", m_Url);
        INFO("MCP endpoint: {}/mcp  (auth {})", m_Url, m_Auth.IsRequired() ? "required" : "off");
        INFO("MCP bearer token: {}", m_Auth.GetToken());

        McpInstaller(m_Url, m_Auth.GetToken()).InstallAll();
        return true;
    }

    void McpServer::ReinstallClients() const
    {
        McpInstaller(m_Url, m_Auth.GetToken()).InstallAll();
    }

    void McpServer::Stop()
    {
        if (!m_Running) {
            return;
        }

        m_Running = false;
        m_Http.stop();

        m_CommandQueue.Shutdown();

        if (m_ListenThread.joinable()) {
            m_ListenThread.join();
        }

        if (m_Scanner) {
            for (int waited = 0; m_Scanner->IsScanning() && waited < 5000; waited += 10) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        INFO("MCP control server stopped");
    }

    void McpServer::RegisterRoutes()
    {
        m_Http.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(kDashboardHtml, "text/html");
        });

        m_Http.Get("/config.html", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(kConfigHtml, "text/html");
        });

        // Serve the branded icon straight off disk (Resources sits next to the
        // executable). Browsers request /favicon.ico automatically, and the
        // dashboard pages point their <link rel="icon"> here too.
        m_Http.Get("/favicon.ico", [](const httplib::Request&, httplib::Response& res) {
            std::ifstream file("Resources/favicon.ico", std::ios::binary);
            if (!file) {
                res.status = 404;
                return;
            }
            std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            res.set_content(bytes, "image/x-icon");
        });

        auto authorized = [this](const httplib::Request& req, httplib::Response& res) -> bool {
            if (!m_Auth.IsAuthorized(req)) {
                res.status = 401;
                res.set_content(R"({"error":"unauthorized"})", "application/json");
                return false;
            }
            return true;
        };

        m_Http.Get("/status", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) {
                return;
            }

            nlohmann::json process = m_CommandQueue.Push([this]() -> nlohmann::json {
                                                    bool attached = m_State && m_State->Process && m_State->Process->IsValid();
                                                    nlohmann::json out { { "attached", attached } };
                                                    if (attached) {
                                                        out["processName"] = m_State->Process->GetName();
                                                        out["pid"] = m_State->Process->GetPid();
                                                    }
                                                    return out;
                                                })
                                          .get();

            process["sessions"] = m_Sessions.DescribeAll();
            process["jobs"] = m_Jobs.DescribeAll();
            res.set_content(process.dump(), "application/json");
        });

        m_Http.Get("/config", [this](const httplib::Request&, httplib::Response& res) {
            McpInstaller installer(m_Url, m_Auth.GetToken());
            nlohmann::json targets = nlohmann::json::array();
            for (const auto& t : installer.Describe()) {
                targets.push_back({
                    { "key", t.Key },
                    { "label", t.Label },
                    { "path", t.Path },
                    { "available", t.Available },
                    { "installed", t.Installed },
                });
            }

            nlohmann::json out {
                { "url", m_Url },
                { "endpoint", m_Url + "/mcp" },
                { "token", m_Auth.GetToken() },
                { "authRequired", m_Auth.IsRequired() },
                { "targets", targets },
            };
            res.set_content(out.dump(), "application/json");
        });

        m_Http.Post("/install", [this](const httplib::Request& req, httplib::Response& res) {
            std::string target;
            try {
                nlohmann::json body = nlohmann::json::parse(req.body);
                target = body.value("target", "");
            } catch (const std::exception&) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"invalid json"})", "application/json");
                return;
            }

            McpInstaller installer(m_Url, m_Auth.GetToken());
            bool ok = installer.InstallOne(target);
            res.status = ok ? 200 : 400;
            res.set_content(nlohmann::json { { "ok", ok }, { "target", target } }.dump(), "application/json");
        });

        m_Http.Post("/auth", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                nlohmann::json body = nlohmann::json::parse(req.body);
                m_Auth.SetRequired(body.value("required", m_Auth.IsRequired()));
            } catch (const std::exception&) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"invalid json"})", "application/json");
                return;
            }
            res.set_content(nlohmann::json { { "ok", true }, { "authRequired", m_Auth.IsRequired() } }.dump(),
                "application/json");
        });

        // The tool catalogue with each tool's enabled flag, for the dashboard's
        // control panel. Authenticated like /status.
        m_Http.Get("/tools", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) {
                return;
            }
            res.set_content(nlohmann::json { { "tools", m_Tools.DescribeAll() } }.dump(), "application/json");
        });

        // Enable or disable a single tool. Body: {"name":"read_memory","enabled":false}.
        m_Http.Post("/tools/toggle", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) {
                return;
            }
            std::string name;
            bool enabled = true;
            try {
                nlohmann::json body = nlohmann::json::parse(req.body);
                name = body.value("name", "");
                enabled = body.value("enabled", true);
            } catch (const std::exception&) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"invalid json"})", "application/json");
                return;
            }
            bool ok = m_Tools.SetEnabled(name, enabled);
            res.status = ok ? 200 : 404;
            res.set_content(nlohmann::json { { "ok", ok }, { "name", name }, { "enabled", enabled } }.dump(),
                "application/json");
        });

        m_Http.Post("/mcp", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) {
                return;
            }

            // Keep the client's session alive on activity. MCP echoes the id from
            // initialize back in this header; touching it stops an actively-used
            // session from being reaped at its TTL and keeps /status honest.
            if (auto it = req.headers.find("Mcp-Session-Id"); it != req.headers.end()) {
                (void)m_Sessions.Touch(it->second);
            }

            nlohmann::json request;
            try {
                request = nlohmann::json::parse(req.body);
            } catch (const std::exception&) {
                res.set_content(MakeError(nullptr, -32700, "parse error").dump(), "application/json");
                return;
            }

            nlohmann::json response = HandleRpc(request);
            if (response.is_null()) {
                res.status = 202;
                return;
            }
            res.set_content(response.dump(), "application/json");
        });

        m_Http.Get("/sse", [this, authorized](const httplib::Request& req, httplib::Response& res) {
            if (!authorized(req, res)) {
                return;
            }

            res.set_header("Content-Type", "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_chunked_content_provider("text/event-stream",
                [this](size_t, httplib::DataSink& sink) -> bool {
                    std::string event = "event: endpoint\ndata: /mcp\n\n";
                    if (!sink.write(event.data(), event.size())) {
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(15));
                    return m_Running.load();
                });
        });
    }

    nlohmann::json McpServer::HandleRpc(const nlohmann::json& request)
    {
        if (!request.is_object() || !request.contains("method")) {
            return MakeError(request.value("id", nlohmann::json(nullptr)), -32600, "invalid request");
        }

        std::string method = request.value("method", "");
        nlohmann::json id = request.value("id", nlohmann::json(nullptr));
        nlohmann::json params = request.value("params", nlohmann::json::object());

        bool isNotification = !request.contains("id");

        if (method == "initialize") {
            return { { "jsonrpc", "2.0" }, { "id", id }, { "result", HandleInitialize(params) } };
        }
        if (method == "notifications/initialized" || method == "notifications/cancelled") {
            return nullptr;
        }
        if (method == "ping") {
            return { { "jsonrpc", "2.0" }, { "id", id }, { "result", nlohmann::json::object() } };
        }
        if (method == "tools/list") {
            return { { "jsonrpc", "2.0" }, { "id", id }, { "result", { { "tools", m_Tools.ListTools() } } } };
        }
        if (method == "tools/call") {
            return { { "jsonrpc", "2.0" }, { "id", id }, { "result", HandleToolsCall(params) } };
        }

        if (isNotification) {
            return nullptr;
        }
        return MakeError(id, -32601, "method not found: " + method);
    }

    nlohmann::json McpServer::HandleInitialize(const nlohmann::json& params)
    {
        std::string clientName = "unknown";
        std::string clientVersion = "?";
        if (params.contains("clientInfo")) {
            clientName = params["clientInfo"].value("name", clientName);
            clientVersion = params["clientInfo"].value("version", clientVersion);
        }

        std::string sessionId = m_Sessions.Create(clientName, clientVersion);
        INFO("MCP client initialized: {} {} (session {})", clientName, clientVersion, sessionId.substr(0, 8));

        return {
            { "protocolVersion", kProtocolVersion },
            { "capabilities", { { "tools", nlohmann::json::object() } } },
            { "serverInfo", { { "name", "CheatStengine" }, { "version", "0.1.0" } } },
            { "sessionId", sessionId },
        };
    }

    nlohmann::json McpServer::HandleToolsCall(const nlohmann::json& params)
    {
        std::string name = params.value("name", "");
        nlohmann::json arguments = params.value("arguments", nlohmann::json::object());
        if (name.empty()) {
            return {
                { "isError", true },
                { "content", { { { "type", "text" }, { "text", "tools/call requires a tool name" } } } },
            };
        }
        return DispatchToolCall(name, arguments);
    }

    nlohmann::json McpServer::DispatchToolCall(const std::string& name, const nlohmann::json& arguments)
    {
        std::future<nlohmann::json> future = m_CommandQueue.Push([this, name, arguments]() -> nlohmann::json {
            try {
                ToolContext context { *m_State, m_Jobs, m_Scanner };
                nlohmann::json result = m_Tools.CallTool(context, name, arguments);
                return { { "ok", true }, { "data", result } };
            } catch (const std::exception& e) {
                return { { "ok", false }, { "message", e.what() } };
            }
        });

        nlohmann::json outcome = future.get();

        if (!outcome.value("ok", false)) {
            // Normal handler failures carry "message"; a queue that's shutting
            // down resolves the future with {"error","code":503} instead, so fall
            // back to "error" before the generic text.
            std::string message = outcome.contains("message")
                ? outcome.value("message", "tool failed")
                : outcome.value("error", "tool failed");
            return {
                { "isError", true },
                { "content", { { { "type", "text" }, { "text", message } } } },
            };
        }

        nlohmann::json data = outcome["data"];
        return {
            { "content", { { { "type", "text" }, { "text", data.dump(2) } } } },
            { "structuredContent", data },
        };
    }

    nlohmann::json McpServer::MakeError(const nlohmann::json& id, int code, const std::string& message)
    {
        return {
            { "jsonrpc", "2.0" },
            { "id", id },
            { "error", { { "code", code }, { "message", message } } },
        };
    }

}
