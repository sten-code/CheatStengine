#include "Evaluator.h"

#include "Lexer.h"
#include "Parser.h"

#include <Engine/Core/Log.h>
#include <iostream>

namespace AddressEvaluator {

    Result EvalVisitor::operator()(BinaryExpr& expr)
    {
        Result left = std::visit(*this, *expr.Left);
        if (left.IsError()) {
            return left;
        }

        Result right = std::visit(*this, *expr.Right);
        if (right.IsError()) {
            return right;
        }

        switch (expr.Op) {
            case Operation::Add: return left.Value + right.Value;
            case Operation::Sub: return left.Value - right.Value;
            case Operation::Mul: return left.Value * right.Value;
            case Operation::Div: {
                INFO("Dividing {} by {}", left.Value, right.Value);
                if (right.Value == 0) {
                    return Error::DivisionByZero;
                }
                return left.Value / right.Value;
            }
        }

        ERR("Unknown operation");
        return 0;
    }

    Result EvalVisitor::operator()(IntLiteral& lit)
    {
        return lit.Value;
    }

    Result EvalVisitor::operator()(Identifier& ident)
    {
        std::ranges::transform(ident.Value, ident.Value.begin(), ::tolower);
        auto it = m_Identifiers.find(ident.Value);
        if (it != m_Identifiers.end()) {
            return it->second;
        }

        return Error::UnknownIdentifier;
    }

    Result Evaluate(const std::string& source, const std::unordered_map<std::string, uintptr_t>& identifiers)
    {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.Tokenize();

        Parser parser(tokens);
        parser.Parse();
        if (parser.HasError()) {
            ERR("Failed to parse expression: {}", parser.GetError());
            return parser.GetError();
        }

        Expr& expr = parser.GetExpression();
        EvalVisitor evaluator(identifiers);
        return std::visit(evaluator, expr);
    }

}