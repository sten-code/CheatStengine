#include "AuthManager.h"

#include <Engine/Core/Log.h>

#include <Windows.h>

#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cctype>

#pragma comment(lib, "bcrypt.lib")

namespace Server {

    AuthManager::AuthManager()
        : m_Token(GenerateToken())
    {
    }

    bool AuthManager::IsAuthorized(const httplib::Request& request) const
    {
        if (!m_Required) {
            return true;
        }

        auto it = request.headers.find("Authorization");
        if (it == request.headers.end()) {
            return false;
        }

        std::string value = it->second;

        constexpr std::string_view scheme = "bearer ";
        if (value.size() >= scheme.size()) {
            std::string prefix = value.substr(0, scheme.size());
            std::ranges::transform(prefix, prefix.begin(), ::tolower);
            if (prefix == scheme) {
                value = value.substr(scheme.size());
            }
        }

        return ConstantTimeEquals(value, m_Token);
    }

    std::string AuthManager::GenerateToken(size_t byteCount)
    {
        std::vector<uint8_t> bytes(byteCount);

        NTSTATUS status = BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status != 0) {
            WARN("BCryptGenRandom failed (0x{:X}); falling back to a weaker token source", static_cast<uint32_t>(status));
            uint64_t ticks = GetTickCount64();
            for (size_t i = 0; i < bytes.size(); ++i) {
                bytes[i] = static_cast<uint8_t>(ticks >> (i % 8 * 8)) ^ static_cast<uint8_t>(i * 131 + 7);
            }
        }

        static constexpr std::array<char, 16> hex { '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

        std::string token;
        token.reserve(bytes.size() * 2);
        for (uint8_t byte : bytes) {
            token.push_back(hex[byte >> 4]);
            token.push_back(hex[byte & 0x0F]);
        }
        return token;
    }

    bool AuthManager::ConstantTimeEquals(const std::string& a, const std::string& b)
    {
        size_t diff = a.size() ^ b.size();
        size_t length = b.size();
        for (size_t i = 0; i < length; ++i) {
            uint8_t lhs = i < a.size() ? static_cast<uint8_t>(a[i]) : 0;
            diff |= static_cast<size_t>(lhs ^ static_cast<uint8_t>(b[i]));
        }
        return diff == 0;
    }

}
