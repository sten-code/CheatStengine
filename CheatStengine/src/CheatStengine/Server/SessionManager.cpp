#include "SessionManager.h"

#include <Windows.h>

#include <bcrypt.h>

#include <array>

#pragma comment(lib, "bcrypt.lib")

namespace Server {

    std::string SessionManager::Create(const std::string& clientName, const std::string& clientVersion)
    {
        std::lock_guard lock(m_Mutex);
        ReapExpired();

        auto now = std::chrono::steady_clock::now();
        std::string id = GenerateId();

        m_Sessions.emplace(id, Session {
                                    .Id = id,
                                    .ClientName = clientName,
                                    .ClientVersion = clientVersion,
                                    .CreatedAt = now,
                                    .LastSeenAt = now,
                                });

        return id;
    }

    bool SessionManager::Touch(const std::string& id)
    {
        std::lock_guard lock(m_Mutex);
        ReapExpired();

        auto it = m_Sessions.find(id);
        if (it == m_Sessions.end()) {
            return false;
        }

        it->second.LastSeenAt = std::chrono::steady_clock::now();
        return true;
    }

    void SessionManager::Remove(const std::string& id)
    {
        std::lock_guard lock(m_Mutex);
        m_Sessions.erase(id);
    }

    nlohmann::json SessionManager::DescribeAll()
    {
        std::lock_guard lock(m_Mutex);
        ReapExpired();

        auto now = std::chrono::steady_clock::now();
        nlohmann::json sessions = nlohmann::json::array();
        for (const auto& [id, session] : m_Sessions) {
            auto ageSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - session.CreatedAt).count();

            sessions.push_back({
                { "id", id.substr(0, 8) },
                { "client", session.ClientName },
                { "version", session.ClientVersion },
                { "ageSeconds", ageSeconds },
            });
        }
        return sessions;
    }

    size_t SessionManager::GetActiveCount()
    {
        std::lock_guard lock(m_Mutex);
        ReapExpired();
        return m_Sessions.size();
    }

    void SessionManager::ReapExpired()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_Sessions.begin(); it != m_Sessions.end();) {
            if (now - it->second.LastSeenAt > m_Ttl) {
                it = m_Sessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::string SessionManager::GenerateId()
    {
        std::array<uint8_t, 16> bytes {};

        NTSTATUS status = BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status != 0) {
            static uint64_t counter = 0;
            uint64_t seed = GetTickCount64() ^ (++counter << 24);
            for (size_t i = 0; i < bytes.size(); ++i) {
                bytes[i] = static_cast<uint8_t>(seed >> ((i % 8) * 8));
            }
        }

        static constexpr std::array<char, 16> hex { '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

        std::string id;
        id.reserve(bytes.size() * 2);
        for (uint8_t byte : bytes) {
            id.push_back(hex[byte >> 4]);
            id.push_back(hex[byte & 0x0F]);
        }
        return id;
    }

}
