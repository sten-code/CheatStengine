#pragma once

#include <CheatStengine/Assembly/Formatter.h>
#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Panes/Pane.h>

#include <zasm/decoder/decoder.hpp>

#include <CheatStengine/Core/KeybindManager.h>
#include <map>
#include <stack>
#include <string>
#include <vector>

class ModulesPane;
class MainLayer;

using DisassemblyValue = std::variant<
    zasm::InstructionDetail,
    std::monostate>;

struct DisassemblyLine {
    std::vector<uint8_t> Bytes;
    DisassemblyValue Value;
};

class DisassemblyPane final : public Pane {
public:
    explicit DisassemblyPane(State& state, ModalManager& modalManager, KeybindManager& keybindManager);

    void HandleKeybinds();

    void Analyze(uintptr_t address);
    void AnalyzePage(uintptr_t pageAddr, size_t pageSize);

    void Draw(double deltaTime) override;

    void FocusAddress(uintptr_t address);
    void JumpToAddress(uintptr_t address);

private:
    static void DrawFormattedInstruction(const FormattedInstruction& instr);

    void GotoAddressInput();
    void JumpToPointedInstruction(const zasm::Instruction& instr);

    void AssembleModal(const std::string& name, const std::any& payload);
    void GotoAddressModal(const std::string& name, const std::any& payload);

private:
    ModalManager& m_ModalManager;
    KeybindManager& m_KeybindManger;

    zasm::Decoder m_Decoder;
    std::string m_AddressInput;
    uintptr_t m_FocussedAddress = 0;
    uintptr_t m_SelectedAddress = 0;

    struct JumpPoint {
        uintptr_t FocussedAddress;
        uintptr_t SelectedAddress;
    };

    std::stack<JumpPoint> m_JumpHistory;

    std::map<uintptr_t, DisassemblyLine> m_Instructions;
};
