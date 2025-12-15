#include "StructDissectPane.h"

#include <CheatEngine/AddressEvaluator/Evaluator.h>
#include <CheatEngine/Icons/MaterialDesignIcons.h>
#include <CheatEngine/MainLayer.h>
#include <CheatEngine/UI/ImGui/Fonts.h>
#include <CheatEngine/UI/ImGui/Menu.h>
#include <CheatEngine/Utils.h>

#include <imgui.h>
#include <imgui_stdlib.h>

std::string StructDissectPane::FormatFieldValue(const FieldValue& fieldValue, const Field& field)
{
    overloads visitor {
        [&]<std::integral T>(T value) -> std::string {
            switch (field.Type) {
                case FieldType::HexInt: return std::format("0x{:X}", value);
                case FieldType::SignedDecInt: return std::format("{}", static_cast<std::make_signed_t<T>>(value));
                case FieldType::UnsignedDecInt: return std::format("{}", value);
                default: return "Invalid Type";
            }
        },
        [&]<std::floating_point T>(T value) { return std::format("{}", value); },
        [&](const Pointer& ptr) { return std::format("0x{:X}", ptr.Address); },
        [&](const StartEndPointer& ptr) { return std::format("[0x{:X} - 0x{:X}]", ptr.Start, ptr.End); },
        [&](const std::string& str) { return Utils::EscapeString(str); },
    };
    return std::visit(visitor, fieldValue);
}

static FieldValue ParseUnsignedInteger(const std::string& str, size_t size, int base)
{
    uint64_t value = std::stoull(str, nullptr, base);
    switch (size) {
        case 1: return static_cast<uint8_t>(value);
        case 2: return static_cast<uint16_t>(value);
        case 4: return static_cast<uint32_t>(value);
        case 8: return value;
        default: return {};
    }
}

static FieldValue ParseSignedInteger(const std::string& str, size_t size, int base)
{
    int64_t value = std::stoll(str, nullptr, base);
    switch (size) {
        case 1: return static_cast<int8_t>(value);
        case 2: return static_cast<int16_t>(value);
        case 4: return static_cast<int32_t>(value);
        case 8: return value;
        default: return {};
    }
}

static FieldValue ParseStartEndPointer(const std::string& str)
{
    size_t dashPos = str.find('-');
    if (dashPos == std::string::npos) {
        return {};
    }

    std::string startStr = str.substr(0, dashPos);
    std::string endStr = str.substr(dashPos + 1);

    uintptr_t startAddr = std::stoull(startStr, nullptr, 16);
    uintptr_t endAddr = std::stoull(endStr, nullptr, 16);
    return StartEndPointer { startAddr, endAddr };
}

FieldValue StructDissectPane::ParseFieldValue(const std::string& str, const Field& field)
{
    if (field.Type == FieldType::String) {
        return str;
    }

    std::string parsed = str;
    int base = 10;
    if (field.Type == FieldType::HexInt || field.Type == FieldType::UnsignedDecInt) {
        if (parsed.starts_with("0x") || parsed.starts_with("0X")) {
            parsed = parsed.substr(2);
            base = 16;
        }
    }

    switch (field.Type) {
        case FieldType::UnsignedDecInt: return ParseUnsignedInteger(parsed, field.Size, base);
        case FieldType::SignedDecInt: return ParseSignedInteger(parsed, field.Size, base);
        case FieldType::HexInt: return ParseUnsignedInteger(parsed, field.Size, 16);
        case FieldType::Float: return std::stof(parsed);
        case FieldType::Double: return std::stod(parsed);
        case FieldType::Pointer: return Pointer { std::stoull(parsed, nullptr, 16) };
        case FieldType::StartEndPointer: return ParseStartEndPointer(parsed);
        case FieldType::Dissection:
        case FieldType::String: break;
    }
    return {};
}

StructDissectPane::StructDissectPane(State& state, ModalManager& modalManager)
    : Pane(ICON_MDI_CONTENT_CUT " Struct Dissect", state)
    , m_ModalManager(modalManager)
{
    m_ModalManager.RegisterModal("Add Dissection", BIND_FN(StructDissectPane::AddDissectionModal));
    m_ModalManager.RegisterModal("Add Element", BIND_FN(StructDissectPane::AddElementModal));
    m_ModalManager.RegisterModal("Edit Value", BIND_FN(StructDissectPane::EditValueModal));
    m_ModalManager.RegisterModal("Change Size", BIND_FN(StructDissectPane::ChangeSizeModal));
}

void StructDissectPane::Draw()
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    if (ImGui::Button("New", ImVec2 { 70.0f, 0 })) {
        m_ModalManager.OpenModal("Add Dissection");
    }

    if (ImGui::BeginTabBar("DissectionsTabBar")) {
        for (size_t i = 0; i < m_State.Dissections.size(); i++) {
            Dissection& dissection = m_State.Dissections[i];
            ImGui::PushID(&dissection);
            bool open = true;
            if (ImGui::BeginTabItem(dissection.GetName().data(), &open)) {
                DrawDissection(dissection);
                ImGui::EndTabItem();
            }
            if (!open) {
                m_State.Dissections.erase(m_State.Dissections.begin() + i);
                i--;
            }
            ImGui::PopID();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void StructDissectPane::AddDissection(const std::string& name, uintptr_t address) const
{
    m_State.Dissections.emplace_back(m_State.Process, name, address);
}

void StructDissectPane::DrawDissection(Dissection& dissection) const
{
    Fonts::Push(Fonts::Type::JetBrainsMono);

    float availY = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("FieldsTableScrollView", ImVec2(0.0f, availY), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("FieldsTable", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY
                | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        DrawField(dissection.GetField(), dissection.GetAddress());

        ImGui::EndTable();
    }

    ImGui::EndChild();
    Fonts::Pop();
}

static bool IsExpandableType(FieldType type)
{
    return type == FieldType::Dissection || type == FieldType::Pointer || type == FieldType::StartEndPointer;
}

void StructDissectPane::DrawField(Field& field, uintptr_t baseAddress, size_t depth) const
{
    static Field* selectedField = nullptr;

    struct RowInfo {
        Field* ChildField;
        Field* ParentField;
        uintptr_t BaseAddress;
        size_t Depth;
        bool Selected;
        size_t ChildIndex;
    };

    // Collect all rows in a flat list
    std::vector<RowInfo> rowsToRender;
    std::function<void(Field&, Field*, uintptr_t, size_t, size_t)> collectRows = [&](Field& field, Field* parent, uintptr_t addr, size_t depth, size_t childIdx) {
        bool selected = (&field == selectedField);

        rowsToRender.push_back({ &field, parent, addr, depth, selected, childIdx });

        if (IsExpandableType(field.Type) && field.Expanded) {
            if (!field.Explored) {
                Field::Pointed pointed = field.GetPointedAddress(m_State.Process, addr);
                field.Children = ExploreAddress(m_State.Process, pointed.Address, pointed.Size);
                field.Explored = true;
            }

            uintptr_t childBaseAddr = (field.Type == FieldType::Dissection) ? addr + field.Offset : field.GetPointedAddress(m_State.Process, addr).Address;

            for (size_t i = 0; i < field.Children.size(); i++) {
                collectRows(field.Children[i], &field, childBaseAddr, depth + 1, i);
            }
        }
    };

    collectRows(field, nullptr, baseAddress, depth, 0);

    // Render the calculated rows with clipping

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rowsToRender.size()));

    std::vector<bool> deleteFlags(rowsToRender.size(), false);

    while (clipper.Step()) {
        for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; rowIdx++) {
            RowInfo& rowInfo = rowsToRender[rowIdx];
            Field& currentField = *rowInfo.ChildField;
            uintptr_t address = rowInfo.BaseAddress + currentField.Offset;

            ImGui::PushID(&currentField);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            std::string label;
            if (currentField.Name.empty()) {
                label = std::format(
                    "0x{:04X} - {} {} {}",
                    currentField.Offset, currentField.Size, currentField.Size == 1 ? "Byte" : "Bytes",
                    currentField.Type);
            } else {
                label = std::format(
                    "0x{:04X} - {} {} {}",
                    currentField.Offset, currentField.Size, currentField.Size == 1 ? "Byte" : "Bytes",
                    currentField.Name);
            }

            FieldValue value = currentField.ReadField(m_State.Process, rowInfo.BaseAddress);

            if (ImGui::Selectable("", rowInfo.Selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                    ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing()))) {
                selectedField = &currentField;
            }

            std::string contextMenuLabel = std::format("##field_value_context_menu_{:X}", reinterpret_cast<uintptr_t>(&currentField));
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                selectedField = &currentField;
                ImGui::OpenPopup(contextMenuLabel.c_str());
            }

            bool shouldDelete = FieldContextMenu(contextMenuLabel, currentField, address, value);
            deleteFlags[rowIdx] = shouldDelete;

            ImGui::SameLine();

            // Render the field
            float cursorX = ImGui::GetCursorPosX();
            float cursorY = ImGui::GetCursorPosY();

            if (IsExpandableType(currentField.Type)) {
                ImGui::SetCursorPosX(cursorX + static_cast<float>(rowInfo.Depth) * 16.0f);
                ImGui::SetCursorPosY(cursorY - 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
                if (ImGui::ArrowButton(label.c_str(), currentField.Expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
                    currentField.Expanded = !currentField.Expanded;
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::SetCursorPosX(cursorX + static_cast<float>(rowInfo.Depth) * 16.0f + 25.0f);
                ImGui::SetCursorPosY(cursorY - 2.0f);
                ImGui::TextUnformatted(label.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::SetCursorPosY(cursorY - 1.0f);
                if (currentField.Type != FieldType::Dissection) {
                    std::string formattedValue = FormatFieldValue(value, currentField);
                    std::string formattedAddr = std::format("0x{:X}", address);
                    ImGui::Text("%s : %s", formattedAddr.c_str(), formattedValue.c_str());
                }
            } else {
                ImGui::SetCursorPosX(cursorX + static_cast<float>(rowInfo.Depth) * 16.0f + 25.0f);
                ImGui::SetCursorPosY(cursorY + 1.0f);
                ImGui::TextUnformatted(label.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::SetCursorPosY(cursorY + 2.0f);
                std::string formattedValue = FormatFieldValue(value, currentField);
                std::string formattedAddr = std::format("0x{:X}", address);
                ImGui::Text("%s : %s", formattedAddr.c_str(), formattedValue.c_str());
            }

            ImGui::PopID();
        }
    }

    clipper.End();

    for (int i = static_cast<int>(rowsToRender.size()) - 1; i >= 0; i--) {
        if (!deleteFlags[i]) {
            continue;
        }

        RowInfo& rowInfo = rowsToRender[i];
        if (rowInfo.ParentField == nullptr) {
            continue;
        }

        if (rowInfo.ChildIndex < rowInfo.ParentField->Children.size()) {
            rowInfo.ParentField->Children.erase(rowInfo.ParentField->Children.begin() + rowInfo.ChildIndex);
            if (selectedField == rowInfo.ChildField) {
                selectedField = nullptr;
            }
        }
    }
}

bool StructDissectPane::FieldContextMenu(
    const std::string& label,
    Field& field, uintptr_t address, const FieldValue& value) const
{
    bool shouldDelete = false;
    if (ImGui::BeginPopup(label.c_str())) {
        if (ImGui::RoundedMenuItem("Copy Address")) {
            std::string addrStr = std::format("0x{:X}", address);
            ImGui::SetClipboardText(addrStr.c_str());
        }

        if (ImGui::RoundedMenuItem("Copy Value")) {
            std::string formattedValue = FormatFieldValue(value, field);
            ImGui::SetClipboardText(formattedValue.c_str());
        }

        ImGui::Separator();

        if (ImGui::BeginRoundedMenu("Change Type")) {
            if (ImGui::RoundedMenuItem("Int8")) {
                field.Size = 1;
                field.Type = FieldType::SignedDecInt;
            }
            if (ImGui::RoundedMenuItem("Int16")) {
                field.Size = 2;
                field.Type = FieldType::SignedDecInt;
            }
            if (ImGui::RoundedMenuItem("Int32")) {
                field.Size = 4;
                field.Type = FieldType::SignedDecInt;
            }
            if (ImGui::RoundedMenuItem("Int64")) {
                field.Size = 8;
                field.Type = FieldType::SignedDecInt;
            }

            ImGui::Separator();

            if (ImGui::RoundedMenuItem("UInt8")) {
                field.Size = 1;
                field.Type = FieldType::UnsignedDecInt;
            }
            if (ImGui::RoundedMenuItem("UInt16")) {
                field.Size = 2;
                field.Type = FieldType::UnsignedDecInt;
            }
            if (ImGui::RoundedMenuItem("UInt32")) {
                field.Size = 4;
                field.Type = FieldType::UnsignedDecInt;
            }
            if (ImGui::RoundedMenuItem("UInt64")) {
                field.Size = 8;
                field.Type = FieldType::UnsignedDecInt;
            }

            ImGui::Separator();

            if (ImGui::RoundedMenuItem("UInt8 (Hex)")) {
                field.Size = 1;
                field.Type = FieldType::HexInt;
            }
            if (ImGui::RoundedMenuItem("UInt16 (Hex)")) {
                field.Size = 2;
                field.Type = FieldType::HexInt;
            }
            if (ImGui::RoundedMenuItem("UInt32 (Hex)")) {
                field.Size = 4;
                field.Type = FieldType::HexInt;
            }
            if (ImGui::RoundedMenuItem("UInt64 (Hex)")) {
                field.Size = 8;
                field.Type = FieldType::HexInt;
            }

            ImGui::Separator();

            if (ImGui::RoundedMenuItem("Float")) {
                field.Size = 4;
                field.Type = FieldType::Float;
            }
            if (ImGui::RoundedMenuItem("Double")) {
                field.Size = 8;
                field.Type = FieldType::Double;
            }

            ImGui::Separator();

            if (ImGui::RoundedMenuItem("Pointer")) {
                field.Size = 8;
                field.Type = FieldType::Pointer;
            }

            if (ImGui::RoundedMenuItem("Start/End Pointer")) {
                field.Size = 16;
                field.Type = FieldType::StartEndPointer;
            }

            ImGui::EndMenu();
        }

        if (ImGui::RoundedMenuItem("Edit Value")) {
            m_ModalManager.OpenModal("Edit Value", EditValuePayload { address, field, value });
        }

        if (field.Type == FieldType::Pointer || field.Type == FieldType::StartEndPointer || field.Type == FieldType::Dissection) {
            ImGui::Separator();

            if (ImGui::RoundedMenuItem("Add Element")) {
                AddElementPayload payload = {
                    0,
                    field
                };
                m_ModalManager.OpenModal("Add Element", payload);
            }

            if (ImGui::RoundedMenuItem("Re-explore")) {
                if (field.Type == FieldType::StartEndPointer) {
                    StartEndPointer pair = m_State.Process.Read<StartEndPointer>(address);
                    field.Children = ExploreAddress(m_State.Process, pair.Start, pair.End - pair.Start);
                } else if (field.Type == FieldType::Pointer) {
                    uintptr_t pointedAddress = m_State.Process.Read<uintptr_t>(address);
                    field.Children = ExploreAddress(m_State.Process, pointedAddress, field.Size);
                } else {
                    field.Children = ExploreAddress(m_State.Process, address, field.Size);
                }
                field.Explored = true;
            }

            if (ImGui::RoundedMenuItem("Change Size")) {
                ChangeSizePayload payload = {
                    field.Size,
                    address,
                    field
                };
                m_ModalManager.OpenModal("Change Size", payload);
            }
        }

        if (field.Type != FieldType::Dissection) {
            ImGui::Separator();

            if (ImGui::RoundedMenuItem("Delete")) {
                INFO("Deleting field at offset 0x{:X}", field.Offset);
                shouldDelete = true;
            }
        }

        ImGui::EndPopup();
    }
    return shouldDelete;
}

void StructDissectPane::AddDissectionModal(const std::string& name, const std::any& rawPayload) const
{
    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }

        static std::string dissectionName = "unnamed";
        static std::string addressInput;

        bool commitNow = false;
        if (ImGui::InputText("Address", &addressInput, ImGuiInputTextFlags_EnterReturnsTrue)) {
            commitNow = true;
        }
        if (ImGui::InputText("Name", &dissectionName, ImGuiInputTextFlags_EnterReturnsTrue)) {
            commitNow = true;
        }

        if (commitNow || ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
            std::unordered_map<std::string, uintptr_t> identifiers;
            for (const MODULEENTRY32& entry : m_State.Modules) {
                std::string moduleName = entry.szModule;
                std::ranges::transform(moduleName, moduleName.begin(), ::tolower);
                identifiers[moduleName] = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
            }

            AddressEvaluator::Result result = AddressEvaluator::Evaluate(addressInput, identifiers);
            if (!result.IsError()) {
                uintptr_t address = result.Value;
                AddDissection(dissectionName, address);

                dissectionName = "unnamed";
                addressInput.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            dissectionName = "unnamed";
            addressInput.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void StructDissectPane::AddElementModal(const std::string& name, const std::any& rawPayload)
{
    static std::string offsetInput;
    static int selectedTypeIndex = 8;
    static int stringSize = 16;

    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        AddElementPayload payload = std::any_cast<AddElementPayload>(rawPayload);

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing()) {
            offsetInput = std::format("0x{:X}", payload.Offset);
            selectedTypeIndex = 0;
            stringSize = 16;
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::InputText("Offset", &offsetInput);

        const char* items[] = {
            "Int8", "Int16", "Int32", "Int64",
            "UInt8", "UInt16", "UInt32", "UInt64",
            "UInt8 (Hex)", "UInt16 (Hex)", "UInt32 (Hex)", "UInt64 (Hex)",
            "Float", "Double",
            "Pointer", "Start/End Pointer",
            "String"
        };
        ImGui::Combo("Type", &selectedTypeIndex, items, IM_ARRAYSIZE(items));
        if (selectedTypeIndex == 16) {
            ImGui::InputInt("String Size", &stringSize);
            if (stringSize < 1) {
                stringSize = 1;
            }
        }

        if (ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
            auto trim = [](std::string s) {
                size_t start = s.find_first_not_of(" \t\n\r");
                size_t end = s.find_last_not_of(" \t\n\r");
                if (start == std::string::npos)
                    return std::string {};
                return s.substr(start, end - start + 1);
            };
            std::string s = trim(offsetInput);
            if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
                s = s.substr(2);
            }

            if (!s.empty()) {
                try {
                    uintptr_t newOffset = std::stoull(s, nullptr, 16);

                    FieldType newType;
                    size_t newSize;
                    switch (selectedTypeIndex) {
                        case 0:
                            newType = FieldType::SignedDecInt;
                            newSize = 1;
                            break;
                        case 1:
                            newType = FieldType::SignedDecInt;
                            newSize = 2;
                            break;
                        case 2:
                            newType = FieldType::SignedDecInt;
                            newSize = 4;
                            break;
                        case 3:
                            newType = FieldType::SignedDecInt;
                            newSize = 8;
                            break;
                        case 4:
                            newType = FieldType::UnsignedDecInt;
                            newSize = 1;
                            break;
                        case 5:
                            newType = FieldType::UnsignedDecInt;
                            newSize = 2;
                            break;
                        case 6:
                            newType = FieldType::UnsignedDecInt;
                            newSize = 4;
                            break;
                        case 7:
                            newType = FieldType::UnsignedDecInt;
                            newSize = 8;
                            break;
                        case 8:
                            newType = FieldType::HexInt;
                            newSize = 1;
                            break;
                        case 9:
                            newType = FieldType::HexInt;
                            newSize = 2;
                            break;
                        case 10:
                            newType = FieldType::HexInt;
                            newSize = 4;
                            break;
                        case 11:
                            newType = FieldType::HexInt;
                            newSize = 8;
                            break;
                        case 12:
                            newType = FieldType::Float;
                            newSize = 4;
                            break;
                        case 13:
                            newType = FieldType::Double;
                            newSize = 8;
                            break;
                        case 14:
                            newType = FieldType::Pointer;
                            newSize = 8;
                            break;
                        case 15:
                            newType = FieldType::StartEndPointer;
                            newSize = 16;
                            break;
                        case 16:
                            newType = FieldType::String;
                            newSize = static_cast<size_t>(stringSize);
                            break;
                        default:
                            newType = FieldType::HexInt;
                            newSize = 1;
                            break;
                    }

                    payload.Field.Children.emplace_back(newOffset, newType, newSize);
                    std::ranges::sort(payload.Field.Children, [](const Field& a, const Field& b) {
                        return a.Offset < b.Offset;
                    });
                    offsetInput.clear();
                    ImGui::CloseCurrentPopup();
                } catch (...) {
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            offsetInput.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void StructDissectPane::EditValueModal(const std::string& name, const std::any& rawPayload) const
{
    static std::string valueInput;

    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        EditValuePayload payload = std::any_cast<EditValuePayload>(rawPayload);

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing()) {
            valueInput = FormatFieldValue(payload.CurrentValue, payload.Field);
            ImGui::SetKeyboardFocusHere();
        }

        bool commitNow = ImGui::InputText("New Value", &valueInput, ImGuiInputTextFlags_EnterReturnsTrue);

        if (commitNow || ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
            FieldValue value = ParseFieldValue(valueInput, payload.Field);
            payload.Field.WriteField(m_State.Process, payload.Address - payload.Field.Offset, value);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            valueInput.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void StructDissectPane::ChangeSizeModal(const std::string& name, const std::any& rawPayload) const
{
    static std::string sizeInput;

    if (ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ChangeSizePayload payload = std::any_cast<ChangeSizePayload>(rawPayload);

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing()) {
            sizeInput = std::format("0x{:X}", payload.CurrentSize);
            ImGui::SetKeyboardFocusHere();
        }

        bool commitNow = ImGui::InputText("New Size", &sizeInput, ImGuiInputTextFlags_EnterReturnsTrue);

        if (commitNow || ImGui::Button("OK", ImVec2 { 70.0f, 0 })) {
            try {
                size_t size = std::stoull(sizeInput, nullptr, 16);
                uintptr_t pointedAddress = m_State.Process.Read<uintptr_t>(payload.Address);

                payload.Field.Size = size;
                payload.Field.Children = ExploreAddress(m_State.Process, pointedAddress, size);
                ImGui::CloseCurrentPopup();
            } catch (...) {
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2 { 70.0f, 0 })) {
            sizeInput.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}