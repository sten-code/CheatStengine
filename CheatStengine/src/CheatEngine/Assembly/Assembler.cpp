#include "Assembler.h"

#include <CheatEngine/Assembly/Mnemonic.h>
#include <Engine/Core/Log.h>

#include <zasm/zasm.hpp>

#include <algorithm>

namespace Assembly {

    void Assembler::Assemble(const std::string& source)
    {
        m_Source = source;
        m_CurrentPos = 0;
        m_Tokens.clear();
        m_Instructions.clear();
        Tokenize();
        Parse();
    }

    const Token& Assembler::At(size_t index) const
    {
        if (m_CurrentPos + index >= m_Tokens.size()) {
            throw std::out_of_range("Token index out of range");
        }
        return m_Tokens[m_CurrentPos + index];
    }

    const Token& Assembler::Eat()
    {
        if (m_CurrentPos >= m_Tokens.size()) {
            throw std::out_of_range("Token index out of range");
        }
        return m_Tokens[m_CurrentPos++];
    }

    const Token& Assembler::Expect(TokenType type)
    {
        if (At().Type != type) {
            throw std::runtime_error("Unexpected token");
        }
        return Eat();
    }

    void Assembly::Assembler::Tokenize()
    {
        size_t pos = 0;
        while (pos < m_Source.size()) {
            // Skip whitespace
            if (isspace(m_Source[pos])) {
                pos++;
                continue;
            }

            if (isalpha(m_Source[pos])) {
                size_t start = pos;
                while (pos < m_Source.size() && isalnum(m_Source[pos])) {
                    pos++;
                }

                std::string identifier = m_Source.substr(start, pos - start);
                std::transform(identifier.begin(), identifier.end(), identifier.begin(), ::tolower);
                if (ZydisMnemonic mnemonic = Mnemonic::FromString(identifier); mnemonic != ZYDIS_MNEMONIC_INVALID) {
                    m_Tokens.push_back(Token { TokenType::Mnemonic, { .Mnemonic = mnemonic } });
                } else if (ZydisRegister reg = Register::FromString(identifier); reg != ZYDIS_REGISTER_NONE) {
                    m_Tokens.push_back(Token { TokenType::Register, { .Reg = zasm::Reg(static_cast<zasm::Reg::Id>(reg)) } });
                } else if (zasm::BitSize bitSize = BitSize::FromString(identifier); bitSize != zasm::BitSize::_0) {
                    m_Tokens.push_back(Token { TokenType::BitSize, { .BitSize = bitSize } });
                } else {
                    throw std::runtime_error("Unknown identifier: " + identifier);
                }
            } else if (isdigit(m_Source[pos]) || (m_Source[pos] == '-' && isdigit(m_Source[pos + 1]))) {
                size_t start = pos;
                if (m_Source[pos] == '-') {
                    pos++;
                }
                while (pos < m_Source.size() && isdigit(m_Source[pos])) {
                    pos++;
                }
                std::string immStr = m_Source.substr(start, pos - start);
                int64_t immVal = std::stoll(immStr);
                m_Tokens.push_back(Token { TokenType::Immediate, { .Imm = zasm::Imm(immVal) } });
            } else {
                switch (m_Source[pos]) {
                    case ',': m_Tokens.push_back({ TokenType::Comma }); break;
                    case '+': m_Tokens.push_back({ TokenType::Add }); break;
                    case '*': m_Tokens.push_back({ TokenType::Mul }); break;
                    case '[': m_Tokens.push_back({ TokenType::OpenBracket }); break;
                    case ']': m_Tokens.push_back({ TokenType::CloseBracket }); break;
                    case ':': m_Tokens.push_back({ TokenType::Colon }); break;
                    default: throw std::runtime_error(std::string("Unknown character: ") + m_Source[pos]);
                }
                pos++;
            }
        }
    }

    void Assembler::Parse()
    {
        INFO("Parsing...");
        while (m_CurrentPos < m_Tokens.size()) {
            ParseInstruction();
        }
    }

    void Assembler::ParseInstruction()
    {
        const Token& mnemonicToken = Eat();
        if (mnemonicToken.Type != TokenType::Mnemonic) {
            throw std::runtime_error("Expected mnemonic");
        }
        zasm::Instruction::Operands operands;
        size_t operandCount = 0;
        while (m_CurrentPos < m_Tokens.size() && At().Type != TokenType::Mnemonic) {
            if (At().Type == TokenType::Comma) {
                Eat();
                continue;
            }
            operands[operandCount] = ParseOperand();
            operandCount++;
        }
        m_Instructions.emplace_back(mnemonicToken.Mnemonic, operandCount, operands);
    }

    zasm::Operand Assembler::ParseOperand()
    {
        const Token& operandToken = At();
        if (operandToken.Type == TokenType::Register && operandToken.Reg.isSeg()) {
            if (m_CurrentPos + 1 < m_Tokens.size() && At(1).Type == TokenType::Colon) {
                return zasm::Operand(ParseMemoryOperand());
            }
        }
        switch (operandToken.Type) {
            case TokenType::Register: return zasm::Operand(Eat().Reg);
            case TokenType::Immediate: return zasm::Operand(Eat().Imm);
            case TokenType::BitSize:
            case TokenType::OpenBracket: return zasm::Operand(ParseMemoryOperand());
            default: throw std::runtime_error("Unexpected token in operand");
        }
    }

    zasm::Mem Assembler::ParseMemoryOperand()
    {
        // Possible memory operands:
        // [Displacement]
        // [BaseReg]
        // [BaseReg + Displacement]
        // [BaseReg + IndexReg]
        // [BaseReg + IndexReg * Scale]
        // [BaseReg + IndexReg * Scale + Displacement]
        // [rip + Displacement] or [rel Displacement]
        zasm::Reg segReg = zasm::x86::ds;
        zasm::Reg baseReg {};
        zasm::Reg indexReg {};
        int64_t disp = 0;
        int32_t scale = 1;
        zasm::BitSize bitSize = zasm::BitSize::_64;

        if (At().Type == TokenType::BitSize) {
            bitSize = Eat().BitSize;
        }

        if (At().Type == TokenType::Register) {
            if (At().Reg.isSeg()) {
                segReg = Eat().Reg;
                Expect(TokenType::Colon);
            }
        }

        Expect(TokenType::OpenBracket);

        if (At().Type == TokenType::Register) {
            // [BaseReg ...
            baseReg = zasm::Reg(Eat().Reg);
            if (At().Type == TokenType::Add) {
                // [BaseReg + ...
                Eat();
                if (At().Type == TokenType::Register) {
                    // [BaseReg + IndexReg ...
                    indexReg = zasm::Reg(Eat().Reg);
                    if (At().Type == TokenType::Mul) {
                        // [BaseReg + IndexReg * Scale ...
                        Eat();
                        scale = Expect(TokenType::Immediate).Imm.value<int32_t>();
                        if (At().Type == TokenType::Add) {
                            // [BaseReg + IndexReg * Scale + Displacement
                            Eat();
                            disp = Expect(TokenType::Immediate).Imm.value<int64_t>();
                        }
                    } else if (At().Type == TokenType::Add) {
                        // [BaseReg + IndexReg + Displacement
                        Eat(); // Consume '+'
                        disp = Expect(TokenType::Immediate).Imm.value<int64_t>();
                    }
                } else if (At().Type == TokenType::Immediate) {
                    // [BaseReg + Displacement
                    disp = Eat().Imm.value<int64_t>();
                } else {
                    throw std::runtime_error("Expected register or immediate after '+'");
                }
            }
        } else if (At().Type == TokenType::Immediate) {
            // [Displacement]
            disp = Eat().Imm.value<int64_t>();
        } else {
            throw std::runtime_error("Unexpected token in memory operand");
        }

        Expect(TokenType::CloseBracket);
        return zasm::Mem(bitSize, segReg, baseReg, indexReg, scale, disp);
    }

}