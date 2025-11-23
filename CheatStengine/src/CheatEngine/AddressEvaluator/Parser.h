#pragma once

#include "Ast.h"
#include "Error.h"
#include "Token.h"

#include <optional>
#include <vector>

namespace AddressEvaluator {

    class Parser {
    public:
        explicit Parser(const std::vector<Token>& tokens);
        ~Parser() = default;

        void Parse();

        bool HasError() const { return m_Error != Error::None; }
        Error GetError() const { return m_Error; }
        Expr& GetExpression() const { return *m_Expression; }

    private:
        std::unique_ptr<Expr> ParseAdditiveBinOp();
        std::unique_ptr<Expr> ParsePrimary();

        std::optional<Token> At() const;
        std::optional<Token> Eat();

    private:
        std::vector<Token> m_Tokens;
        size_t m_Position = 0;
        Error m_Error = Error::None;
        std::unique_ptr<Expr> m_Expression = nullptr;
    };

}