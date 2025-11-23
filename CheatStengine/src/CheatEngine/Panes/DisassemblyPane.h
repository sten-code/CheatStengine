#pragma once

#include <CheatEngine/Assembly/Formatter.h>
#include <CheatEngine/Panes/Pane.h>
#include <CheatEngine/Core/ModalManager.h>

#include <zasm/decoder/decoder.hpp>

#include <map>
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
    explicit DisassemblyPane(State& state, ModalManager& modalManager);

    void HandleKeybinds();

    void Analyze(uintptr_t address);
    void AnalyzePage(uintptr_t pageAddr, size_t pageSize);

    void Draw() override;

    void FocusAddress(uintptr_t address);

private:
    static void DrawFormattedInstruction(const FormattedInstruction& instr);

    void GotoAddressInput();

    void AssembleModal(const std::string& name, const std::any& payload);
    void GotoAddressModal(const std::string& name, const std::any& payload);

private:
    ModalManager& m_ModalManager;

    zasm::Decoder m_Decoder;
    std::string m_AddressInput;
    uintptr_t m_FocussedAddress = 0;
    uintptr_t m_SelectedAddress = 0;

    std::map<uintptr_t, DisassemblyLine> m_Instructions;
};
