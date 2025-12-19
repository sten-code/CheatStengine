#include "WatchPane.h"

#include <CheatStengine/AddressEvaluator/Evaluator.h>
#include <CheatStengine/Icons/MaterialDesignIcons.h>
#include <CheatStengine/MainLayer.h>
#include <CheatStengine/UI/ImGui/EditableLabel.h>
#include <CheatStengine/Utils.h>

#include <imgui.h>
#include <imgui_stdlib.h>

WatchPane::WatchPane(State& state, ModalManager& modalManager)
    : Pane(ICON_MDI_MAGNIFY " Watch", state)
    , m_ModalManager(modalManager)
{
    m_ModalManager.RegisterModal("Add Address", BIND_FN(WatchPane::AddAddressModal));
}

void WatchPane::Draw()
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    HandleKeyboardShortcuts();

    if (ImGui::Button("Add Address")) {
        m_ModalManager.OpenModal("Add Address");
        m_AddressInput.clear();
        m_AddressType = ValueType::Int32;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2 { 3, 2 });
    if (ImGui::BeginTable("AddressWatchTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Description");
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(m_State.WatchAddresses.size());
        while (clipper.Step()) {
            for (size_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const MemoryAddress& memoryAddr = m_State.WatchAddresses[i];
                bool isSelected = (m_SelectedIndex == i);
                ImGui::TableNextRow();

                // Description
                ImGui::TableSetColumnIndex(0);
                std::string descLabel = std::format("watch_desc_{}_{:X}", i, memoryAddr.Address);
                ImGui::EditableLabel(descLabel.c_str(), &m_State.WatchAddresses[i].Description);

                // Address
                ImGui::TableSetColumnIndex(1);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeightWithSpacing() / 2 - ImGui::GetTextLineHeight() / 2));
                std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(memoryAddr.Address, m_State.Modules);
                if (modEntry) {
                    ImGui::Text("%s+0x%llX", modEntry->szModule, memoryAddr.Address - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                } else {
                    ImGui::Text("0x%llX", memoryAddr.Address);
                }

                // Value Type
                ImGui::TableSetColumnIndex(2);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::BeginCombo(std::format("##watch_type_{}_{:X}", i, memoryAddr.Address).c_str(),
                        ValueTypeToString(memoryAddr.Type).c_str())) {
                    for (size_t n = 0; n < static_cast<size_t>(ValueType::COUNT); n++) {
                        bool isTypeSelected = (memoryAddr.Type == static_cast<ValueType>(n));
                        if (ImGui::Selectable(ValueTypeToString(static_cast<ValueType>(n)).c_str(), isTypeSelected)) {
                            m_State.WatchAddresses[i].Type = static_cast<ValueType>(n);
                        }
                        if (isTypeSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleColor();

                // Value
                std::string valueStr = memoryAddr.ReadValue(m_State.Process);
                ImGui::TableSetColumnIndex(3);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
                std::string valueLabel = std::format("watch_value_{}_{:X}", i, memoryAddr.Address);
                if (ImGui::EditableLabel(valueLabel.c_str(), &valueStr)) {
                    memoryAddr.WriteValue(m_State.Process, valueStr);
                }

                // All columns
                ImGui::TableSetColumnIndex(0);
                std::string selectableLabel = std::format("##watch_span_{}_{:X}", i, memoryAddr.Address);
                if (ImGui::Selectable(selectableLabel.c_str(), isSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    m_SelectedIndex = i;
                }

                if (ImGui::BeginPopupContextItem()) {
                    m_SelectedIndex = i;
                    if (ImGui::BeginMenu("Copy Address")) {
                        if (ImGui::MenuItem("As Hex")) {
                            ImGui::SetClipboardText(std::format("0x{:X}", memoryAddr.Address).c_str());
                        }
                        if (ImGui::MenuItem("As Decimal")) {
                            ImGui::SetClipboardText(std::to_string(memoryAddr.Address).c_str());
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Delete")) {
                        DeleteSelectedAddress();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    ImGui::End();
}

void WatchPane::HandleKeyboardShortcuts()
{
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelectedAddress();
    }
}

void WatchPane::AddAddressModal(const std::string& name, const std::any& payload)
{
    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Address", &m_AddressInput);

        if (ImGui::BeginCombo("##watch_add_address_type", ValueTypeToString(m_AddressType).c_str())) {
            for (size_t n = 0; n < static_cast<size_t>(ValueType::COUNT); n++) {
                bool isTypeSelected = (m_AddressType == static_cast<ValueType>(n));
                if (ImGui::Selectable(ValueTypeToString(static_cast<ValueType>(n)).c_str(), isTypeSelected)) {
                    m_AddressType = static_cast<ValueType>(n);
                }
                if (isTypeSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("OK")) {
            std::unordered_map<std::string, uintptr_t> identifiers;
            for (const MODULEENTRY32& entry : m_State.Modules) {
                std::string name = entry.szModule;
                std::ranges::transform(name, name.begin(), ::tolower);
                identifiers[name] = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
            }

            AddressEvaluator::Result result = AddressEvaluator::Evaluate(m_AddressInput, identifiers);
            if (!result.IsError()) {
                uintptr_t address = result.Value;
                INFO("Going to address: 0x{:X}", address);
                m_State.WatchAddresses.push_back(MemoryAddress { "", address, m_AddressType });
                ImGui::CloseCurrentPopup();
            } else {
                ERR("Error: {}", result.Status);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WatchPane::DeleteSelectedAddress()
{
    if (m_SelectedIndex != -1 && m_SelectedIndex < m_State.WatchAddresses.size()) {
        m_State.WatchAddresses.erase(m_State.WatchAddresses.begin() + m_SelectedIndex);
        m_SelectedIndex = -1;
    }
}