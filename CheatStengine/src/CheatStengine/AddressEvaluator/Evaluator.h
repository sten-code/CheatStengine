#pragma once

#include "Ast.h"
#include "Error.h"

#include <unordered_map>

namespace AddressEvaluator {

    class EvalVisitor final : public Visitor<Result> {
    public:
        explicit EvalVisitor(const std::unordered_map<std::string, uintptr_t>& identifiers)
            : m_Identifiers(identifiers)
        {
        }

        Result operator()(BinaryExpr&) override;
        Result operator()(IntLiteral&) override;
        Result operator()(Identifier&) override;

    private:
        const std::unordered_map<std::string, uintptr_t>& m_Identifiers;
    };

    Result Evaluate(const std::string& source, const std::unordered_map<std::string, uintptr_t>& identifiers);

}