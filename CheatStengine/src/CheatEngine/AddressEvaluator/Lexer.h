#pragma once

#include "Token.h"

#include <string>
#include <vector>

namespace AddressEvaluator {

    class Lexer {
    public:
        explicit Lexer(const std::string& source);
        ~Lexer() = default;

        std::vector<Token> Tokenize();

    private:
        static bool IsCharIdentifier(char c) { return isalnum(c) || c == '_' || c == '.'; }

    private:
        std::string m_Source;
        size_t m_Position = 0;
        std::vector<Token> m_Tokens;
    };

}