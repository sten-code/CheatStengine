#include "Parser.h"
#include "Lexer.h"
#include "Token.h"

#include <Engine/Core/Log.h>
#include <iostream>

namespace AddressEvaluator {

    Parser::Parser(const std::vector<Token>& tokens)
        : m_Tokens(tokens)
    {
    }

    void Parser::Parse()
    {
        m_Expression = ParseAdditiveBinOp();
    }

    std::unique_ptr<Expr> Parser::ParseAdditiveBinOp()
    {
        return ParseBinaryOp(&Parser::ParseMultiplicativeBinOp,
            [](TokenType t) -> std::optional<Operation> {
                if (t == TokenType::Add)
                    return Operation::Add;
                if (t == TokenType::Sub)
                    return Operation::Sub;
                return std::nullopt;
            });
    }

    std::unique_ptr<Expr> Parser::ParseMultiplicativeBinOp()
    {
        return ParseBinaryOp(&Parser::ParsePrimary,
            [](TokenType t) -> std::optional<Operation> {
                if (t == TokenType::Mul)
                    return Operation::Mul;
                if (t == TokenType::Div)
                    return Operation::Div;
                return std::nullopt;
            });
    }

    std::unique_ptr<Expr> Parser::ParsePrimary()
    {
        std::optional<Token> token = Eat();
        if (!token) {
            m_Error = Error::UnexpectedEndOfInput;
            return nullptr;
        }

        switch (token->Type) {
            case TokenType::Number: {
                uintptr_t value = std::get<uintptr_t>(token->Value);
                return std::make_unique<Expr>(IntLiteral { value });
            }
            case TokenType::Identifier: {
                std::string value = std::get<std::string>(token->Value);
                return std::make_unique<Expr>(Identifier { value });
            }
            default:
                // std::cout << token->Type << std::endl;
                m_Error = Error::UnexpectedToken;
                return nullptr;
        }
    }

    std::optional<Token> Parser::At() const
    {
        if (m_Position >= m_Tokens.size()) {
            return std::nullopt;
        }
        return m_Tokens[m_Position];
    }

    std::optional<Token> Parser::Eat()
    {
        if (m_Position >= m_Tokens.size()) {
            return std::nullopt;
        }
        return m_Tokens[m_Position++];
    }

}