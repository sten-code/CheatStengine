#pragma once

#include <Engine/Core/Core.h>
#include <format>
#include <sstream>
#include <string>
#include <variant>

namespace AddressEvaluator {

    enum class TokenType {
        Number,
        Identifier,
        Add,
        Sub,
        Div,
        Mul,
    };

    struct Token {
        TokenType Type;
        std::variant<std::monostate, uintptr_t, std::string> Value;
    };

    template <typename OStream>
    OStream& operator<<(OStream& os, TokenType type)
    {
        switch (type) {
            case TokenType::Number: os << "Number"; break;
            case TokenType::Identifier: os << "Identifier"; break;
            case TokenType::Add: os << "Add"; break;
            case TokenType::Sub: os << "Sub"; break;
            case TokenType::Mul: os << "Mul"; break;
            case TokenType::Div: os << "Div"; break;
            default: os << "Unknown"; break;
        }
        return os;
    }

    template <typename OStream>
    OStream& operator<<(OStream& os, const Token& token)
    {
        os << token.Type << "(";
        switch (token.Type) {
            case TokenType::Number: os << std::get<uintptr_t>(token.Value); break;
            case TokenType::Identifier: os << std::get<std::string>(token.Value); break;
            case TokenType::Add: os << "+"; break;
            case TokenType::Sub: os << "-"; break;
            case TokenType::Mul: os << "*"; break;
            case TokenType::Div: os << "/"; break;
            default: os << "Unknown"; break;
        }
        os << ")";
        return os;
    }

}

IMPLEMENT_FORMATTER(AddressEvaluator::TokenType);
IMPLEMENT_FORMATTER(AddressEvaluator::Token);
