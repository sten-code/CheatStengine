#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace Server {

    class McpInstaller {
    public:
        McpInstaller(std::string url, std::string token)
            : m_Url(std::move(url))
            , m_Token(std::move(token))
        {
        }

        struct Target {
            std::string Key;
            std::string Label;
            std::string Path;
            bool Available = true; 
            bool Installed = false;
        };

        void InstallAll() const;
        bool InstallOne(const std::string& key) const;
        [[nodiscard]] std::vector<Target> Describe() const;

    private:
        [[nodiscard]] std::vector<Target> ResolveTargets() const;

        [[nodiscard]] bool IsInstalled(const std::string& key, const std::string& path) const;

        void WritePortableConfig() const;
        void WriteClaudeCodeConfig() const;
        void WriteCursorConfig() const;
        void WriteClaudeDesktopConfig() const;
        void WriteCodexConfig() const;
        void WriteSkill() const;

        [[nodiscard]] nlohmann::json HttpServerEntry() const;
        [[nodiscard]] nlohmann::json BridgeServerEntry() const;
        [[nodiscard]] std::string SkillContent() const;

        void MergeJsonServer(const std::string& path, const std::string& topKey,
            const std::string& serverKey, const std::string& serverJson) const;

        [[nodiscard]] static std::string GetExecutableDir();
        [[nodiscard]] static std::string GetHomeDir();
        [[nodiscard]] static std::string GetAppDataDir();

        std::string m_Url;
        std::string m_Token;
    };

}
