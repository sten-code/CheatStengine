#include "Ast.h"

#include <iostream>

namespace AddressEvaluator {

    void PrintVisitor::operator()(BinaryExpr& expr)
    {
        std::cout << "(";
        std::visit(*this, *expr.Left);
        std::cout << " " << expr.Op << " ";
        std::visit(*this, *expr.Right);
        std::cout << ")";
    }

    void PrintVisitor::operator()(IntLiteral& lit)
    {
        std::cout << lit.Value;
    }

    void PrintVisitor::operator()(Identifier& ident)
    {
        std::cout << ident.Value;
    }

}
