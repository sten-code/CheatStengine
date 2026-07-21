#pragma once

#include <httplib.h>

#include <string>

namespace Server {

    class AuthManager {
    public:
        AuthManager();
        [[nodiscard]] const std::string& GetToken() const { return m_Token; }
        [[nodiscard]] bool IsAuthorized(const httplib::Request& request) const;

        void SetRequired(bool required) { m_Required = required; }
        [[nodiscard]] bool IsRequired() const { return m_Required; }

    private:
        [[nodiscard]] static std::string GenerateToken(size_t byteCount = 32);
        [[nodiscard]] static bool ConstantTimeEquals(const std::string& a, const std::string& b);

        std::string m_Token;
        bool m_Required = false;
    };

}
