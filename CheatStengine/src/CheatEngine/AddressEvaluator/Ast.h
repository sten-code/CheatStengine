#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace AddressEvaluator {

    struct BinaryExpr;
    struct IntLiteral;
    struct Identifier;

    using Expr = std::variant<
        BinaryExpr,
        IntLiteral,
        Identifier>;

    enum class Operation {
        Add,
        Sub,
        Mul,
        Div,
    };

    struct BinaryExpr {
        Operation Op;
        std::unique_ptr<Expr> Left;
        std::unique_ptr<Expr> Right;
    };

    struct IntLiteral {
        uintptr_t Value;
    };

    struct Identifier {
        std::string Value;
    };

    template <typename T>
    class Visitor {
    public:
        virtual ~Visitor() = default;
        virtual T operator()(BinaryExpr&) = 0;
        virtual T operator()(IntLiteral&) = 0;
        virtual T operator()(Identifier&) = 0;
    };

    class PrintVisitor final : public Visitor<void> {
    public:
        void operator()(BinaryExpr&) override;
        void operator()(IntLiteral&) override;
        void operator()(Identifier&) override;
    };

    template <typename OStream>
    OStream& operator<<(OStream& os, const Operation& op)
    {
        switch (op) {
            case Operation::Add: os << "+"; break;
            case Operation::Sub: os << "-"; break;
            case Operation::Mul: os << "*"; break;
            case Operation::Div: os << "/"; break;
        }
        return os;
    }
}
