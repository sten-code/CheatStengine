#include "Evaluator.h"
#include "Lexer.h"
#include "Parser.h"

#include <CheatStengine/Process/Process.h>
#include <Engine/Core/Log.h>

#include <iostream>

namespace AddressEvaluator {

    Result EvalVisitor::operator()(BinaryExpr& expr)
    {
        if (expr.Op == Operation::Arrow) {
            Identifier* left = std::get_if<Identifier>(&*expr.Left);
            Identifier* right = std::get_if<Identifier>(&*expr.Right);
            if (!left || !right) {
                ERR("Arrow operator requires identifiers on both sides");
                return 0;
            }

            std::string moduleName = left->Value;
            std::string procName = right->Value;
            auto it = m_Identifiers.find(moduleName);
            if (it == m_Identifiers.end()) {
                ERR("Unknown module: {}", moduleName);
                return Error::UnknownIdentifier;
            }

            uintptr_t moduleBase = it->second;
            uintptr_t procAddress = m_Process.GetModuleProc(moduleBase, procName);
            if (procAddress == 0) {
                ERR("Failed to get procedure address for {} in module {}", procName, moduleName);
                return Error::UnknownIdentifier;
            }

            return procAddress;
        }

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

    Result Evaluate(const std::string& source, Process& process)
    {
        std::unordered_map<std::string, uintptr_t> identifiers;
        for (const MODULEENTRY32& entry : process.GetModuleEntries()) {
            std::string name = entry.szModule;
            std::ranges::transform(name, name.begin(), ::tolower);
            identifiers[name] = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
        }

        Lexer lexer(source);
        std::vector<Token> tokens = lexer.Tokenize();
        // for (const Token& token : tokens) {
        //     std::cout << token << std::endl;
        // }

        Parser parser(tokens);
        parser.Parse();
        if (parser.HasError()) {
            ERR("Failed to parse expression: {}", parser.GetError());
            return parser.GetError();
        }

        Expr& expr = parser.GetExpression();
        // PrintVisitor visitor;
        // std::visit(visitor, expr);

        EvalVisitor evaluator(identifiers, process);
        return std::visit(evaluator, expr);
    }

}