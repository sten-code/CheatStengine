#pragma once

#include <Zydis/Mnemonic.h>
#include <zasm/zasm.hpp>

#include <string>
#include <variant>
#include <vector>

namespace Assembly {

    enum class TokenType {
        Mnemonic,
        Register,
        Immediate,
        BitSize,

        Comma,
        Add,
        Mul,
        Colon,
        OpenBracket,
        CloseBracket
    };

    struct Token {
        TokenType Type;
        union {
            ZydisMnemonic Mnemonic;
            zasm::Reg Reg;
            zasm::Imm Imm;
            zasm::BitSize BitSize;
        };
    };

    class Assembler {
    public:
        void Assemble(const std::string& source);

        const std::vector<Token>& GetTokens() const { return m_Tokens; }
        const std::vector<zasm::Instruction>& GetInstructions() const { return m_Instructions; }

    private:
        const Token& At(size_t index = 0) const;
        const Token& Eat();
        const Token& Expect(TokenType type);

        void Tokenize();
        void Parse();
        void ParseInstruction();
        zasm::Operand ParseOperand();
        zasm::Mem ParseMemoryOperand();

    private:
        std::string m_Source;

        size_t m_CurrentPos = 0;
        std::vector<Token> m_Tokens;

        std::vector<zasm::Instruction> m_Instructions;
    };

}