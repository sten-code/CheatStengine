#include "DisassemblyPane.h"

#include <CheatEngine/AddressEvaluator/Evaluator.h>
#include <CheatEngine/Assembly/Assembler.h>
#include <CheatEngine/Icons/MaterialDesignIcons.h>
#include <CheatEngine/MainLayer.h>
#include <CheatEngine/UI/ImGui/Fonts.h>
#include <CheatEngine/UI/ImGui/Menu.h>
#include <CheatEngine/Utils.h>
#include <Engine/Core/Application.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <zasm/formatter/formatter.hpp>
#include <zasm/x86/mnemonic.hpp>

#include <format>

DisassemblyPane::DisassemblyPane(State& state, ModalManager& modalManager)
    : Pane(ICON_MDI_HAMMER_WRENCH " Disassembly", state)
    , m_Decoder(zasm::MachineMode::AMD64)
    , m_ModalManager(modalManager)
{
    m_ModalManager.RegisterModal("Goto Address", BIND_FN(DisassemblyPane::GotoAddressModal));
    m_ModalManager.RegisterModal("Assemble", BIND_FN(DisassemblyPane::AssembleModal));
}

void DisassemblyPane::HandleKeybinds()
{
    // Handle 'G' key to open the goto address popup
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_G)) {
        m_AddressInput.clear();
        m_ModalManager.OpenModal("Goto Address");
    }
}

void DisassemblyPane::Analyze(uintptr_t address)
{
    INFO("Analyzing Address: 0x{:X}", address);
    std::optional<MEMORY_BASIC_INFORMATION> mbi = m_State.Process.Query(address);
    if (!mbi) {
        ERR("Failed to query memory at address 0x{:X}", address);
        return;
    }

    uintptr_t start = reinterpret_cast<uintptr_t>(mbi->BaseAddress);
    INFO("  Start: 0x{:X}, Size: 0x{:X}", start, mbi->RegionSize);
    AnalyzePage(start, mbi->RegionSize);
}

void DisassemblyPane::AnalyzePage(uintptr_t pageAddr, size_t pageSize)
{
    // INFO("Analyzing page: 0x{:X} (size: 0x{:X})", pageAddr, pageSize);
    std::vector<uint8_t> code = m_State.Process.ReadBuffer(pageAddr, pageSize);

    uintptr_t bytesDecoded = 0;
    while (bytesDecoded < code.size()) {
        const zasm::Decoder::Result res = m_Decoder.decode(code.data() + bytesDecoded, code.size() - bytesDecoded, pageAddr + bytesDecoded);
        if (!res.hasValue()) {
            m_Decoder = zasm::Decoder(m_Decoder.getMode());
            m_Instructions[pageAddr + bytesDecoded] = DisassemblyLine { { code[bytesDecoded] }, std::monostate {} };
            // ERR("Decoding error at address 0x{:X}: {}", bytesDecoded, res.error().getErrorMessage());
            bytesDecoded++;
            continue;
        }

        const zasm::InstructionDetail& instrDetail = res.value();
        std::vector<uint8_t> instrBytes(code.begin() + bytesDecoded, code.begin() + bytesDecoded + instrDetail.getLength());
        m_Instructions[pageAddr + bytesDecoded] = DisassemblyLine { instrBytes, instrDetail };
        // const zasm::Instruction instr = instrDetail.getInstruction();
        // std::string formatted = zasm::formatter::toString(&instr);
        // INFO("0x{:X} (0x{:X}): {}", pageAddr + bytesDecoded, bytesDecoded, formatted);

        bytesDecoded += instrDetail.getLength();
    }
}

void DisassemblyPane::Draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0.0f, 0.0f });
    ImGui::Begin(m_Name.c_str(), &m_Open);
    ImGui::PopStyleVar();

    HandleKeybinds();

    ImGui::BeginChild("ScrollingRegion", ImVec2 { 0.0f, 0.0f }, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (ImGui::IsWindowHovered()) {
        int64_t scrollWheel = ImGui::GetIO().MouseWheel;
        if (scrollWheel > 0 && m_FocussedAddress > 0) {
            if (auto it = m_Instructions.lower_bound(m_FocussedAddress); it != m_Instructions.begin()) {
                FocusAddress((--it)->first);
            } else if (m_FocussedAddress > 0) {
                m_FocussedAddress--;
            }
        } else if (scrollWheel < 0) {
            if (auto it = m_Instructions.upper_bound(m_FocussedAddress); it != m_Instructions.end()
                && m_Instructions.contains(m_FocussedAddress)) {
                FocusAddress(it->first);
            } else {
                m_FocussedAddress++;
            }
        }
    }

    size_t visibleInstructionCount = ImGui::GetContentRegionAvail().y / ImGui::GetTextLineHeightWithSpacing() + 1;

    Fonts::Push(Fonts::Type::JetBrainsMono);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    if (ImGui::BeginTable("Disassembly", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Bytes");
        ImGui::TableSetupColumn("Instruction");
        ImGui::TableHeadersRow();

        uintptr_t currentAddress = m_FocussedAddress;
        size_t instructionCount = 0;
        while (instructionCount++ < visibleInstructionCount) {
            ImGui::TableNextRow();
            uintptr_t address = currentAddress;

            bool isRowSelected = (m_SelectedAddress == currentAddress);

            ImGui::TableSetColumnIndex(0);
            ImVec2 rowMin = { cursorPos.x, ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeightWithSpacing() };
            ImGui::TableSetColumnIndex(2);
            ImVec2 rowMax = { ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionMax().x, rowMin.y };

            std::ostringstream formattedBytes;
            std::string formattedInstruction;

            std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(currentAddress, m_State.Modules);
            if (!m_Instructions.contains(currentAddress)) {
                ImGui::TableSetColumnIndex(0);
                if (modEntry) {
                    ImGui::Text("%s+0x%llX", modEntry->szModule, currentAddress - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                } else {
                    ImGui::Text("0x%012llX", currentAddress);
                }
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("??");
                currentAddress++;
            } else {
                const DisassemblyLine& line = m_Instructions[currentAddress];

                const overloads visitor = {
                    [this, &rowMin, &rowMax](const zasm::InstructionDetail& instrDetail) -> std::pair<std::string, size_t> {
                        const zasm::Instruction instr = instrDetail.getInstruction();

                        FormattedInstruction formatted = Formatter::Format(instr, Formatter::Options { .ImmediateFormatter = [this](int64_t val) {
                            std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(val, m_State.Modules);
                            if (modEntry) {
                                return std::format("{}+0x{:X}", modEntry->szModule, val - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                            }

                            return std::format("0x{:X}", val);
                        } });
                        if (instr.getMnemonic() == zasm::x86::Mnemonic::Ret) {
                            ImGui::GetWindowDrawList()->AddLine(rowMin, rowMax, ImGui::GetColorU32(ImGuiCol_Border));
                        }

                        DrawFormattedInstruction(formatted);

                        return { formatted.Text, instrDetail.getLength() };
                    },
                    [&line](std::monostate) -> std::pair<std::string, size_t> {
                        std::ostringstream oss;
                        oss << "db ";
                        for (size_t i = 0; i < line.Bytes.size(); i++) {
                            uint8_t byte = line.Bytes[i];
                            oss << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << "0x" << (int)byte;
                            if (i < line.Bytes.size() - 1) {
                                oss << ", ";
                            }
                        }

                        ImGui::Text("%s", oss.str().c_str());
                        return { oss.str(), 1 };
                    }
                };

                ImGui::TableSetColumnIndex(2);
                auto [formatted, length] = std::visit(visitor, line.Value);
                formattedInstruction = formatted;

                ImGui::TableSetColumnIndex(0);
                if (modEntry) {
                    ImGui::Text("%s+0x%llX", modEntry->szModule, currentAddress - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                } else {
                    ImGui::Text("0x%012llX", currentAddress);
                }

                for (size_t i = 0; i < line.Bytes.size(); i++) {
                    formattedBytes << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (int)line.Bytes[i];
                    if (i < line.Bytes.size() - 1) {
                        formattedBytes << " ";
                    }
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", formattedBytes.str().c_str());

                currentAddress += length;
            }

            ImGui::TableSetColumnIndex(0);
            std::string selectableLabel = std::format("##selectable_{:X}", currentAddress);
            if (ImGui::Selectable(selectableLabel.c_str(), isRowSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                m_SelectedAddress = address;
            }

            std::string popupLabel = std::format("##popup_{:X}", address);
            if (ImGui::BeginPopupContextItem(popupLabel.c_str())) {
                m_SelectedAddress = address;
                if (ImGui::RoundedMenuItem("Copy Address")) {
                    ImGui::SetClipboardText(std::format("0x{:012X}", address).c_str());
                }
                if (ImGui::RoundedMenuItem("Copy Bytes")) {
                    ImGui::SetClipboardText(formattedBytes.str().c_str());
                }
                if (ImGui::RoundedMenuItem("Copy Instruction")) {
                    ImGui::SetClipboardText(formattedInstruction.c_str());
                }
                if (ImGui::RoundedMenuItem("Assemble")) {
                    m_ModalManager.OpenModal("Assemble");
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndTable();
    }
    Fonts::Pop();

    ImGui::EndChild();

    ImGui::End();
}

void DisassemblyPane::FocusAddress(uintptr_t address)
{
    m_FocussedAddress = address;
    if (!m_Instructions.contains(address)) {
        Analyze(address);
    }
}

void DisassemblyPane::AssembleModal(const std::string& name, const std::any& payload)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2 { 0.5f, 0.5f });
    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        static std::string assembly;
        ImGui::Text("Assemble instruction at 0x%llX", m_SelectedAddress);
        ImGui::InputTextMultiline("##source", &assembly);

        if (ImGui::Button("Assemble")) {
            Assembly::Assembler assembler;
            assembler.Assemble(assembly);
            const std::vector<zasm::Instruction>& instructions = assembler.GetInstructions();

            zasm::Program program(zasm::MachineMode::AMD64);
            zasm::x86::Assembler a(program);
            for (const zasm::Instruction& instruction : instructions) {
                a.emit(instruction);
            }

            zasm::Serializer serializer;
            auto res = serializer.serialize(program, 0);
            const uint8_t* code = serializer.getCode();
            size_t codeSize = serializer.getCodeSize();
            m_State.Process.WriteBuffer(m_SelectedAddress, code, codeSize);
            Analyze(m_SelectedAddress);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DisassemblyPane::GotoAddressInput()
{
    std::unordered_map<std::string, uintptr_t> identifiers;
    for (const MODULEENTRY32& entry : m_State.Modules) {
        std::string name = entry.szModule;
        std::ranges::transform(name, name.begin(), ::tolower);
        identifiers[name] = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
    }

    AddressEvaluator::Result result = AddressEvaluator::Evaluate(m_AddressInput, identifiers);
    if (result.IsError()) {
        return;
    }

    uintptr_t address = result.Value;
    INFO("Going to address: 0x{:X}", address);
    FocusAddress(address);
    m_SelectedAddress = address;
}

void DisassemblyPane::GotoAddressModal(const std::string& name, const std::any& payload)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2 { 0.5f, 0.5f });
    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Text("Fill in the address you want to go to");
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("Address", &m_AddressInput, ImGuiInputTextFlags_EnterReturnsTrue)) {
            GotoAddressInput();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
            GotoAddressInput();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void DisassemblyPane::DrawFormattedInstruction(const FormattedInstruction& instr)
{
    if (instr.Text.empty()) {
        return;
    }

    size_t currentPos = 0;
    for (const Highlight& highlight : instr.Highlights) {
        const Range& range = highlight.Range;

        if (currentPos < range.Start) {
            ImGui::TextUnformatted(instr.Text.substr(currentPos, range.Start - currentPos).c_str());
            ImGui::SameLine(0, 0);
        }

        ImU32 color = IM_COL32(186, 194, 222, 255);
        switch (highlight.Type) {
            case Highlight::Ty::Mnemonic: color = IM_COL32(116, 199, 236, 255); break;
            case Highlight::Ty::Register: color = IM_COL32(243, 139, 168, 255); break;
            case Highlight::Ty::Displacement:
            case Highlight::Ty::Immediate: color = IM_COL32(250, 226, 174, 255); break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(instr.Text.substr(range.Start, range.End - range.Start).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        currentPos = range.End;
    }

    if (currentPos < instr.Text.size()) {
        ImGui::TextUnformatted(instr.Text.substr(currentPos).c_str());
        ImGui::SameLine(0, 0);
    }

    ImGui::NewLine();
}
