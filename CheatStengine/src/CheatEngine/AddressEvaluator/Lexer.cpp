#include "Lexer.h"
#include "Token.h"

#include <Engine/Core/Log.h>

namespace AddressEvaluator {

    Lexer::Lexer(const std::string& source)
        : m_Source(source)
    {
    }

    std::vector<Token> Lexer::Tokenize()
    {
        m_Tokens.clear();
        m_Position = 0;
        while (m_Position < m_Source.size()) {
            size_t start = m_Position;
            char c = m_Source[m_Position++];
            if (isspace(c)) {
                continue;
            }

            if (c == '0'
                && m_Position < m_Source.size()
                && m_Source[m_Position] == 'x') {
                m_Position++; // Skip '0x'
                while (m_Position < m_Source.size() && isxdigit(m_Source[m_Position])) {
                    m_Position++;
                }
                std::string hexStr = m_Source.substr(start, m_Position - start);
                uintptr_t number = std::stoull(hexStr, nullptr, 16);
                INFO("Parsed hex number: {}", number);
                m_Tokens.push_back(Token { TokenType::Number, number });
            } else if (isxdigit(c)) {
                size_t original = m_Position - 1;
                while (m_Position < m_Source.size() && isxdigit(m_Source[m_Position])) {
                    m_Position++;
                }
                if (isalpha(m_Source[m_Position]) || m_Source[m_Position] == '_') {
                    m_Position = original;
                    goto identifier;
                }
                std::string numberStr = m_Source.substr(start, m_Position - start);
                uintptr_t number = std::stoull(numberStr, nullptr, 16);
                INFO("Parsed number: {}", number);
                m_Tokens.push_back(Token { TokenType::Number, number });
            } else if (isalpha(c) || c == '_') {
            identifier:
                while (m_Position < m_Source.size() && IsCharIdentifier(m_Source[m_Position])) {
                    m_Position++;
                }
                std::string str = m_Source.substr(start, m_Position - start);
                INFO("Parsed identifier: {}", str);
                m_Tokens.push_back(Token { TokenType::Identifier, str });
            } else {
                switch (c) {
                    case '+': m_Tokens.push_back(Token { TokenType::Add, {} }); break;
                    case '-': m_Tokens.push_back(Token { TokenType::Sub, {} }); break;
                    case '*': m_Tokens.push_back(Token { TokenType::Mul, {} }); break;
                    case '/': m_Tokens.push_back(Token { TokenType::Div, {} }); break;
                    default: ERR("Unknown character in lexer: '{}'", c); break;
                }
            }
        }
        return m_Tokens;
    }

}
