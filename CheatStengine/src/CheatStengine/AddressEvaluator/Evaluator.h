#pragma once

#include "Ast.h"
#include "Error.h"

#include <CheatStengine/Process/Process.h>

#include <unordered_map>

namespace AddressEvaluator {

    class EvalVisitor final : public Visitor<Result> {
    public:
        explicit EvalVisitor(const std::unordered_map<std::string, uintptr_t>& identifiers, Process& process)
            : m_Identifiers(identifiers)
            , m_Process(process)
        {
        }

        Result operator()(BinaryExpr&) override;
        Result operator()(IntLiteral&) override;
        Result operator()(Identifier&) override;

    private:
        const std::unordered_map<std::string, uintptr_t>& m_Identifiers;
        Process& m_Process;
    };

    Result Evaluate(const std::string& source, Process& process);

}