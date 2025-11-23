#pragma once

#include <zasm/zasm.hpp>

#include <format>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

struct Range {
    size_t Start;
    size_t End;
};

struct Highlight {
    Range Range;
    enum class Ty {
        Mnemonic,
        Register,
        Immediate,
        Displacement,
    } Type;
};

struct FormattedInstruction {
    std::string Text;
    std::vector<Highlight> Highlights;
};

class Formatter {
public:
    struct Options {
        std::function<std::string(int64_t)> ImmediateFormatter;
    };

    static FormattedInstruction Format(const zasm::Instruction& instr, const Options& opts = {});

    Formatter() = delete;
    explicit Formatter(const Options& opts)
        : m_Options(opts)
    {
    }

    FormattedInstruction Get() const { return FormattedInstruction { m_Output.str(), m_Highlights }; }

private:
    static zasm::Reg GetDefaultMemSegment(const zasm::Mem& mem);

    void AppendString(const std::string& str);
    void AppendStringHighlight(const std::string& str, Highlight::Ty type);

    void OpToString(const zasm::Operand::None& opNone) noexcept;
    void OpToString(const zasm::Reg& reg);
    void OpToString(const zasm::Imm& opImm);
    void OpToString(const zasm::Label& label);
    void OpToString(const zasm::Mem& opMem);

private:
    Options m_Options;
    std::ostringstream m_Output;
    std::vector<Highlight> m_Highlights;
    size_t m_CurrentPos = 0;
};