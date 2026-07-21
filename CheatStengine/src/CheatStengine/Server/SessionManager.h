#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Server {

    class SessionManager {
    public:
        struct Session {
            std::string Id;
            std::string ClientName;
            std::string ClientVersion;
            std::chrono::steady_clock::time_point CreatedAt;
            std::chrono::steady_clock::time_point LastSeenAt;
        };

        explicit SessionManager(std::chrono::seconds ttl = std::chrono::minutes(30))
            : m_Ttl(ttl)
        {
        }

        [[nodiscard]] std::string Create(const std::string& clientName, const std::string& clientVersion);
        [[nodiscard]] bool Touch(const std::string& id);
        void Remove(const std::string& id);
        [[nodiscard]] nlohmann::json DescribeAll();
        [[nodiscard]] size_t GetActiveCount();

    private:
        void ReapExpired();

        [[nodiscard]] static std::string GenerateId();

        std::chrono::seconds m_Ttl;

        std::mutex m_Mutex;
        std::unordered_map<std::string, Session> m_Sessions;
        uint64_t m_NextId = 1;
    };

}
