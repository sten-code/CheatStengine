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
        std::unique_ptr<Expr> ParseMultiplicativeBinOp();
        std::unique_ptr<Expr> ParsePrimary();

        template <typename NextFn, typename MapOpFn>
        std::unique_ptr<Expr> ParseBinaryOp(NextFn next, MapOpFn mapOp);

        std::optional<Token> At() const;
        std::optional<Token> Eat();

    private:
        std::vector<Token> m_Tokens;
        size_t m_Position = 0;
        Error m_Error = Error::None;
        std::unique_ptr<Expr> m_Expression = nullptr;
    };

    template <typename NextFn, typename MapOpFn>
    std::unique_ptr<Expr> Parser::ParseBinaryOp(NextFn next, MapOpFn mapOp)
    {
        std::unique_ptr<Expr> left = (this->*next)();
        if (!left) {
            return nullptr;
        }

        while (true) {
            std::optional<Token> op = At();
            if (!op) {
                break;
            }
            std::optional<Operation> operation = mapOp(op->Type);
            if (!operation) {
                break;
            }
            Eat();
            std::unique_ptr<Expr> right = (this->*next)();
            if (!right) {
                return nullptr;
            }
            left = std::make_unique<Expr>(BinaryExpr { *operation, std::move(left), std::move(right) });
        }

        return left;
    }

}