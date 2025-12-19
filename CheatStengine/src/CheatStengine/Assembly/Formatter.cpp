#include "Formatter.h"

FormattedInstruction Formatter::Format(const zasm::Instruction& instr, const Options& opts)
{
    Formatter ctx(opts);
    if (instr.hasAttrib(zasm::x86::Attribs::Lock)) {
        ctx.AppendString("lock ");
    }
    if (instr.hasAttrib(zasm::x86::Attribs::Rep)) {
        ctx.AppendString("rep ");
    }
    if (instr.hasAttrib(zasm::x86::Attribs::Repe)) {
        ctx.AppendString("repe ");
    }
    if (instr.hasAttrib(zasm::x86::Attribs::Repne)) {
        ctx.AppendString("repne ");
    }

    const char* str = ZydisMnemonicGetString(static_cast<ZydisMnemonic>(instr.getMnemonic().value()));
    ctx.AppendStringHighlight(str, Highlight::Ty::Mnemonic);

    for (size_t opIndex = 0; opIndex < instr.getOperandCount(); opIndex++) {
        const zasm::Operand& operand = instr.getOperand(opIndex);
        if (operand.holds<zasm::Operand::None>()) {
            break;
        }

        operand.visit([&](auto&& opVal) {
            if (opIndex == 0) {
                ctx.AppendString(" ");
            } else if (opIndex > 0) {
                ctx.AppendString(", ");
            }
            ctx.OpToString(opVal);
        });
    }

    return ctx.Get();
}

zasm::Reg Formatter::GetDefaultMemSegment(const zasm::Mem& mem)
{
    if (const zasm::Reg& seg = mem.getSegment(); seg.isValid()) {
        return seg;
    }
    if (const zasm::Reg& base = mem.getBase(); base.isValid()) {
        if (base.getPhysicalIndex() == zasm::x86::sp.getPhysicalIndex()
            || base.getPhysicalIndex() == zasm::x86::bp.getPhysicalIndex()) {
            return zasm::x86::ss;
        }
    }
    return zasm::x86::ds;
}

void Formatter::AppendString(const std::string& str)
{
    m_Output << str;
    m_CurrentPos += str.size();
}

void Formatter::AppendStringHighlight(const std::string& str, Highlight::Ty type)
{
    m_Highlights.emplace_back(Range { m_CurrentPos, m_CurrentPos + str.size() }, type);
    AppendString(str);
}

void Formatter::OpToString([[maybe_unused]] const zasm::Operand::None& opNone) noexcept
{
}

void Formatter::OpToString(const zasm::Reg& reg)
{
    std::string str = ZydisRegisterGetString(static_cast<ZydisRegister>(reg.getId()));
    AppendStringHighlight(str, Highlight::Ty::Register);
}

void Formatter::OpToString(const zasm::Imm& opImm)
{
    int64_t val = opImm.value<int64_t>();
    if (m_Options.ImmediateFormatter) {
        std::string formatted = m_Options.ImmediateFormatter(val);
        AppendStringHighlight(formatted, Highlight::Ty::Immediate);
        return;
    }

    if (val < 0) {
        AppendStringHighlight(std::format("-0x{:X}", -val), Highlight::Ty::Immediate);
    } else {
        AppendStringHighlight(std::format("0x{:X}", val), Highlight::Ty::Immediate);
    }
}

void Formatter::OpToString(const zasm::Label& label)
{
    AppendString(std::format("L{}", static_cast<int32_t>(label.getId())));
}

void Formatter::OpToString(const zasm::Mem& opMem)
{
    switch (opMem.getBitSize()) {
        case zasm::BitSize::_8: AppendString("BYTE "); break;
        case zasm::BitSize::_16: AppendString("WORD "); break;
        case zasm::BitSize::_32: AppendString("DWORD "); break;
        case zasm::BitSize::_64: AppendString("QWORD "); break;
        case zasm::BitSize::_80: AppendString("TWORD "); break;
        case zasm::BitSize::_128: AppendString("XMMWORD "); break;
        case zasm::BitSize::_256: AppendString("YMMWORD "); break;
        case zasm::BitSize::_512: AppendString("ZMMWORD "); break;
        default: break;
    }

    if (const zasm::Reg& regSeg = GetDefaultMemSegment(opMem); regSeg.isValid() && regSeg != zasm::x86::ds) {
        OpToString(regSeg);
        AppendString(":");
    }

    AppendString("[");

    bool hasRel = false;
    bool hasBase = false;
    if (const zasm::Reg& regBase = opMem.getBase(); regBase.isValid()) {
        if (regBase.isIP()) {
            hasRel = true;
        } else {
            OpToString(regBase);
            hasBase = true;
        }
    }

    bool hasIndex = false;
    if (const zasm::Reg& reg = opMem.getIndex(); reg.isValid()) {
        if (hasBase) {
            AppendString("+");
        }
        OpToString(reg);
        hasIndex = true;

        if (opMem.getScale() > 1) {
            AppendString(std::format("*{}", opMem.getScale()));
        }
    }

    bool hasLabel = false;
    if (const zasm::Label& label = opMem.getLabel(); label.isValid()) {
        if (hasBase || hasIndex) {
            AppendString("+");
        }

        OpToString(label);
        hasLabel = true;
    }

    if (const int64_t disp = opMem.getDisplacement(); disp != 0) {
        if (hasBase || hasIndex || hasLabel) {
            if (disp < 0) {
                AppendString("-");
            } else {
                AppendString("+");
            }
        } else if (hasRel) {
            AppendString("rel ");
        }

        if (disp < 0) {
            AppendStringHighlight(std::format("0x{:02X}", -disp), Highlight::Ty::Displacement);
        } else {
            AppendStringHighlight(std::format("0x{:02X}", disp), Highlight::Ty::Displacement);
        }

    } else {
        if (!hasBase && !hasIndex && !hasLabel) {
            AppendString("0");
        }
    }

    AppendString("]");
}