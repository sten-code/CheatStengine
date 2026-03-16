#include "PatternScannerPane.h"

#include "DisassemblyPane.h"

#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Icons/MaterialDesignIcons.h>
#include <CheatStengine/Utils.h>

#include <CheatStengine/Assembly/Formatter.h>
#include <CheatStengine/UI/ImGui/Menu.h>
#include <imgui.h>
#include <imgui_stdlib.h>

PatternScannerPane::PatternScannerPane(State& state, MainLayer& mainLayer)
    : Pane(ICON_MDI_REGEX " Pattern Scanner", state)
    , m_MainLayer(mainLayer)
    , m_ModalManager(mainLayer.GetModalManager())
    , m_KeybindManager(mainLayer.GetKeybindManager())
    , m_PatternScanner(state.Process)
{
    m_ModalManager.RegisterModal("Scan Pattern", BIND_FN(PatternScannerPane::ScanPatternModal));
    m_KeybindManager.RegisterKeybind(
        "Scan Pattern",
        "Scan a byte pattern in the process's memory",
        "Pattern Scanner", ImGuiKey_B | ImGuiMod_Alt, [this]() {
            m_ModalManager.OpenModal("Scan Pattern");
        });
}

void PatternScannerPane::Draw()
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    if (ImGui::BeginTabBar("DissectionsTabBar")) {
        for (size_t i = 0; i < m_State.PatternScanResults.size(); i++) {
            PatternScan& result = m_State.PatternScanResults[i];
            bool open = true;
            if (ImGui::BeginTabItem(result.Pattern.c_str(), &open)) {
                DrawPatternScanResults(result);
                ImGui::EndTabItem();
            }
            if (!open) {
                m_State.PatternScanResults.erase(m_State.PatternScanResults.begin() + i);
                i--;
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void PatternScannerPane::PerformPatternScan(std::string_view pattern, const MODULEENTRY32& moduleEntry) const
{
    INFO("Performing pattern scan for pattern '{}' in module '{}'", pattern, moduleEntry.szModule);
    std::vector<uintptr_t> results = m_PatternScanner.PatternScan(
        pattern,
        reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr),
        reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr) + moduleEntry.modBaseSize);
    std::vector<PatternScan::Result> formattedResults;
    formattedResults.reserve(results.size());
    zasm::Decoder decoder(zasm::MachineMode::AMD64);
    for (uintptr_t address : results) {
        INFO("Decoding instruction at 0x{:X}", address);
        std::vector<uint8_t> code = m_State.Process.ReadBuffer(address, 16);
        zasm::Decoder::Result result = decoder.decode(code.data(), code.size(), address);
        if (result.hasValue()) {
            FormattedInstruction formatted = Formatter::Format(result->getInstruction(), Formatter::Options { .ImmediateFormatter = [this](uint64_t val) {
                std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(val, m_State.Modules);
                if (modEntry) {
                    return std::format("{}+0x{:X}", modEntry->szModule, val - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                }

                return std::format("0x{:X}", val);
            } });

            formattedResults.push_back({ address, formatted.Text });
        } else {
            std::ostringstream oss;
            oss << "db ";
            for (size_t i = 0; i < code.size(); i++) {
                uint8_t byte = code[i];
                oss << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << "0x" << static_cast<int>(byte);
                if (i < code.size() - 1) {
                    oss << ", ";
                }
            }
            formattedResults.push_back({ address, oss.str() });
        }
    }
    m_State.PatternScanResults.emplace_back(std::string(pattern), formattedResults);
}

void PatternScannerPane::DrawPatternScanResults(PatternScan& patternScan) const
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2 { 3, 2 });
    if (ImGui::BeginTable("PatternScannerResults", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Instruction");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(patternScan.Results.size());
        while (clipper.Step()) {
            for (size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                PatternScan::Result& result = patternScan.Results[i];
                bool isSelected = (patternScan.SelectedIndex == i);
                ImGui::TableNextRow();

                // Address
                ImGui::TableSetColumnIndex(0);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeightWithSpacing() / 2 - ImGui::GetTextLineHeight() / 2));
                std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(result.Address, m_State.Modules);
                if (modEntry) {
                    ImGui::Text("%s+0x%llX", modEntry->szModule, result.Address - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                } else {
                    ImGui::Text("0x%llX", result.Address);
                }

                // Instruction
                ImGui::TableSetColumnIndex(1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeightWithSpacing() / 2 - ImGui::GetTextLineHeight() / 2));
                ImGui::Text("%s", result.Instruction.c_str());

                ImGui::SameLine();

                ImGui::TableSetColumnIndex(0);
                std::string selectableLabel = std::format("##pattern_scan_span_{}_{:X}", i, result.Address);
                if (ImGui::Selectable(selectableLabel.c_str(), isSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    patternScan.SelectedIndex = i;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    DisassemblyPane& disassemblyPane = *m_MainLayer.GetPane<DisassemblyPane>();
                    disassemblyPane.SelectAddress(result.Address);
                    disassemblyPane.ForceFocus();
                }

                if (ImGui::BeginPopupContextItem()) {
                    patternScan.SelectedIndex = i;
                    if (ImGui::BeginRoundedMenu("Copy")) {
                        if (ImGui::RoundedMenuItem("Address")) {
                            ImGui::SetClipboardText(std::format("0x{:X}", result.Address).c_str());
                        }
                        if (ImGui::RoundedMenuItem("Instruction")) {
                            ImGui::SetClipboardText(result.Instruction.c_str());
                        }
                        if (modEntry) {
                            if (ImGui::RoundedMenuItem("RVA")) {
                                ImGui::SetClipboardText(std::format("0x{:X}", result.Address - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr)).c_str());
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void PatternScannerPane::ScanPatternModal(const std::string& name, const std::any& payload)
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2 { 0.5f, 0.5f });
    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }

        static std::string pattern;
        ImGui::Text("What pattern do you want to scan for? (IDA Style)");
        ImGui::InputText("##pattern", &pattern);

        std::vector<const char*> moduleItems;
        moduleItems.reserve(m_State.Modules.size());
        for (const MODULEENTRY32& moduleEntry : m_State.Modules) {
            moduleItems.emplace_back(moduleEntry.szModule);
        }

        static int selectedModuleIndex = 0;
        bool hasModules = !moduleItems.empty();
        if (hasModules) {
            ImGui::Combo("Module", &selectedModuleIndex, moduleItems.data(), static_cast<int>(moduleItems.size()));
        } else {
            ImGui::TextDisabled("No modules loaded");
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Ok", ImVec2 { 70.0f, 0 })) {
            PerformPatternScan(pattern, m_State.Modules[selectedModuleIndex]);
            ForceFocus();
            ImGui::CloseCurrentPopup();
        }
        if (!hasModules) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}