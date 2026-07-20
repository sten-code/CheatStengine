#include "McpInstaller.h"

#include <Engine/Core/Log.h>

#include <nlohmann/json.hpp>

#include <Windows.h>

#include <Shlobj.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#pragma comment(lib, "Shell32.lib")

namespace Server {

    namespace {

        std::string ReadFile(const std::string& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) {
                return {};
            }
            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        bool WriteFile(const std::string& path, const std::string& content)
        {
            try {
                std::filesystem::path fsPath(path);
                if (fsPath.has_parent_path()) {
                    std::filesystem::create_directories(fsPath.parent_path());
                }
                std::ofstream stream(path, std::ios::binary | std::ios::trunc);
                if (!stream) {
                    WARN("MCP install: could not open '{}' for writing", path);
                    return false;
                }
                stream << content;
                return true;
            } catch (const std::exception& e) {
                WARN("MCP install: write to '{}' failed: {}", path, e.what());
                return false;
            }
        }

    }

    void McpInstaller::InstallAll() const
    {
        WritePortableConfig();
        WriteClaudeCodeConfig();
        WriteCursorConfig();
        WriteClaudeDesktopConfig();
        WriteCodexConfig();
        WriteSkill();
        INFO("MCP client configs and skill written; agents can connect to {}/mcp", m_Url);
    }

    nlohmann::json McpInstaller::HttpServerEntry() const
    {
        return {
            { "type", "http" },
            { "url", m_Url + "/mcp" },
            { "headers", { { "Authorization", "Bearer " + m_Token } } },
        };
    }

    nlohmann::json McpInstaller::BridgeServerEntry() const
    {
        return {
            { "command", "npx" },
            { "args", { "-y", "mcp-remote", m_Url + "/mcp", "--header", "Authorization: Bearer " + m_Token } },
        };
    }

    std::vector<McpInstaller::Target> McpInstaller::ResolveTargets() const
    {
        std::vector<Target> targets;

        std::string exeDir = GetExecutableDir();
        targets.push_back({ "portable", "Portable (.mcp.json next to the exe)",
            exeDir + "\\.mcp.json", !exeDir.empty(), false });

        std::string cwd;
        try {
            cwd = std::filesystem::current_path().string();
        } catch (const std::exception&) {
            cwd.clear();
        }
        targets.push_back({ "claude-code", "Claude Code (project .mcp.json)",
            cwd.empty() ? std::string() : cwd + "\\.mcp.json", !cwd.empty(), false });
        targets.push_back({ "cursor", "Cursor (.cursor/mcp.json)",
            cwd.empty() ? std::string() : cwd + "\\.cursor\\mcp.json", !cwd.empty(), false });

        std::string appData = GetAppDataDir();
        targets.push_back({ "claude-desktop", "Claude Desktop (via mcp-remote)",
            appData.empty() ? std::string() : appData + "\\Claude\\claude_desktop_config.json",
            !appData.empty(), false });

        std::string home = GetHomeDir();
        targets.push_back({ "codex", "Codex CLI (~/.codex/config.toml)",
            home.empty() ? std::string() : home + "\\.codex\\config.toml", !home.empty(), false });
        targets.push_back({ "skill", "Claude Code skill (~/.claude/skills)",
            home.empty() ? std::string() : home + "\\.claude\\skills\\cheatstengine\\SKILL.md",
            !home.empty(), false });

        return targets;
    }

    bool McpInstaller::IsInstalled(const std::string& key, const std::string& path) const
    {
        if (path.empty()) {
            return false;
        }
        std::string existing = ReadFile(path);
        if (existing.empty()) {
            return false;
        }

        if (key == "codex") {
            return existing.find("[mcp_servers.cheatstengine]") != std::string::npos;
        }
        if (key == "skill") {
            return true; 
        }

        try {
            nlohmann::json root = nlohmann::json::parse(existing);
            return root.contains("mcpServers") && root["mcpServers"].is_object()
                && root["mcpServers"].contains("cheatstengine");
        } catch (const std::exception&) {
            return false;
        }
    }

    std::vector<McpInstaller::Target> McpInstaller::Describe() const
    {
        std::vector<Target> targets = ResolveTargets();
        for (Target& t : targets) {
            t.Installed = t.Available && IsInstalled(t.Key, t.Path);
        }
        return targets;
    }

    bool McpInstaller::InstallOne(const std::string& key) const
    {
        if (key == "portable") {
            WritePortableConfig();
            return true;
        }
        if (key == "claude-code") {
            WriteClaudeCodeConfig();
            return true;
        }
        if (key == "cursor") {
            WriteCursorConfig();
            return true;
        }
        if (key == "claude-desktop") {
            WriteClaudeDesktopConfig();
            return true;
        }
        if (key == "codex") {
            WriteCodexConfig();
            return true;
        }
        if (key == "skill") {
            WriteSkill();
            return true;
        }
        WARN("MCP install: unknown target '{}'", key);
        return false;
    }

    void McpInstaller::MergeJsonServer(const std::string& path, const std::string& topKey,
        const std::string& serverKey, const std::string& serverJson) const
    {
        nlohmann::json root = nlohmann::json::object();

        std::string existing = ReadFile(path);
        if (!existing.empty()) {
            try {
                root = nlohmann::json::parse(existing);
            } catch (const std::exception&) {
                WARN("MCP install: '{}' wasn't valid JSON; rewriting it", path);
                root = nlohmann::json::object();
            }
        }

        if (!root.contains(topKey) || !root[topKey].is_object()) {
            root[topKey] = nlohmann::json::object();
        }

        try {
            root[topKey][serverKey] = nlohmann::json::parse(serverJson);
        } catch (const std::exception& e) {
            WARN("MCP install: internal server json invalid: {}", e.what());
            return;
        }

        WriteFile(path, root.dump(2));
    }

    void McpInstaller::WritePortableConfig() const
    {
        nlohmann::json root { { "mcpServers", { { "cheatstengine", HttpServerEntry() } } } };

        std::string dir = GetExecutableDir();
        std::string payload = root.dump(2);
        WriteFile(dir + "\\.mcp.json", payload);
        WriteFile(dir + "\\mcp.json", payload);
    }

    void McpInstaller::WriteClaudeCodeConfig() const
    {
        std::string cwd;
        try {
            cwd = std::filesystem::current_path().string();
        } catch (const std::exception&) {
            return;
        }
        if (cwd == GetExecutableDir()) {
            return;
        }

        MergeJsonServer(cwd + "\\.mcp.json", "mcpServers", "cheatstengine", HttpServerEntry().dump());
    }

    void McpInstaller::WriteCursorConfig() const
    {
        std::string cwd;
        try {
            cwd = std::filesystem::current_path().string();
        } catch (const std::exception&) {
            return;
        }

        MergeJsonServer(cwd + "\\.cursor\\mcp.json", "mcpServers", "cheatstengine", HttpServerEntry().dump());
    }

    void McpInstaller::WriteClaudeDesktopConfig() const
    {
        std::string appData = GetAppDataDir();
        if (appData.empty()) {
            return;
        }

        MergeJsonServer(appData + "\\Claude\\claude_desktop_config.json", "mcpServers", "cheatstengine",
            BridgeServerEntry().dump());
    }

    void McpInstaller::WriteCodexConfig() const
    {
        std::string home = GetHomeDir();
        if (home.empty()) {
            return;
        }

        std::string path = home + "\\.codex\\config.toml";
        std::string existing = ReadFile(path);
        if (existing.find("[mcp_servers.cheatstengine]") != std::string::npos) {
            return; // already registered
        }

        std::ostringstream block;
        block << "\n[mcp_servers.cheatstengine]\n";
        block << "command = \"npx\"\n";
        block << "args = [\"-y\", \"mcp-remote\", \"" << m_Url << "/mcp\", \"--header\", \"Authorization: Bearer "
              << m_Token << "\"]\n";

        WriteFile(path, existing + block.str());
    }

    std::string McpInstaller::SkillContent() const
    {
        std::ostringstream skill;
        skill << "---\n";
        skill << "name: cheatstengine\n";
        skill << "description: Drive Cheat Stengine (a Windows reverse-engineering tool) over MCP: "
                 "list/attach processes, read/write and scan memory, disassemble, assemble, dissect "
                 "structs, and generate byte signatures. Use when inspecting or modifying a running "
                 "process's memory on this machine.\n";
        skill << "---\n\n";
        skill << "# Cheat Stengine MCP\n\n";
        skill << "This tool exposes a live reverse-engineering engine over MCP at `" << m_Url << "/mcp`.\n";
        skill << "Auth is off by default, so you can connect with just the URL. If the operator turns "
                 "auth on, send `Authorization: Bearer " << m_Token << "`.\n\n";
        skill << "## Workflow\n\n";
        skill << "1. `list_processes` - find the target's pid.\n";
        skill << "2. `open_process` { pid } - attach. Everything below needs an attached process.\n";
        skill << "3. Inspect: `list_modules`, `read_memory`, `query_memory`, `disassemble`, `dissect_struct`.\n";
        skill << "4. Resolve addresses symbolically with `resolve_address` { expression: \"module.dll+0x1234\" }; "
                 "most tools also accept an expression string directly in place of a numeric address.\n";
        skill << "5. Modify: `write_memory` { address, hex }, or `assemble` { source, address, write: true }.\n\n";
        skill << "## Async tools (poll, don't block)\n\n";
        skill << "- `pattern_scan` { pattern } returns a `jobId`. Poll `job_status` { id } until status is "
                 "`completed`, then read `result.matches`.\n";
        skill << "- `scan_value` { valueType, scanType, value } starts a value scan. Poll `scan_results` "
                 "until `scanning` is false. Refine with `scan_value` { ..., next: true }.\n\n";
        skill << "## Notes\n\n";
        skill << "- Addresses in results are hex strings like `0x7FF6...`.\n";
        skill << "- Value types: int8/16/32/64, uint8/16/32/64, float, double.\n";
        skill << "- Scan types: exact, bigger, smaller, between, unknown.\n";
        skill << "- `pattern` is space-separated hex with `??` wildcards, e.g. `48 8B ?? C3`.\n";

        return skill.str();
    }

    void McpInstaller::WriteSkill() const
    {
        std::string content = SkillContent();

        std::string home = GetHomeDir();
        if (!home.empty()) {
            WriteFile(home + "\\.claude\\skills\\cheatstengine\\SKILL.md", content);
        }
        WriteFile(GetExecutableDir() + "\\SKILL.md", content);
    }

    std::string McpInstaller::GetExecutableDir()
    {
        wchar_t buffer[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (len == 0) {
            return ".";
        }
        try {
            return std::filesystem::path(std::wstring(buffer, len)).parent_path().string();
        } catch (const std::exception&) {
            return ".";
        }
    }

    std::string McpInstaller::GetHomeDir()
    {
        if (const char* profile = std::getenv("USERPROFILE")) {
            return profile;
        }

        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &path))) {
            std::string result;
            try {
                result = std::filesystem::path(path).string();
            } catch (const std::exception&) {
            }
            CoTaskMemFree(path);
            return result;
        }
        return {};
    }

    std::string McpInstaller::GetAppDataDir()
    {
        if (const char* appData = std::getenv("APPDATA")) {
            return appData;
        }

        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
            std::string result;
            try {
                result = std::filesystem::path(path).string();
            } catch (const std::exception&) {
            }
            CoTaskMemFree(path);
            return result;
        }
        return {};
    }

}
