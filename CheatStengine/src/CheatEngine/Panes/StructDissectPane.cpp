#include "StructDissectPane.h"

#include <CheatEngine/AddressEvaluator/Evaluator.h>
#include <CheatEngine/Icons/MaterialDesignIcons.h>
#include <CheatEngine/MainLayer.h>
#include <CheatEngine/UI/ImGui/Fonts.h>
#include <CheatEngine/UI/ImGui/Menu.h>
#include <CheatEngine/Utils.h>

#include <imgui.h>
#include <imgui_stdlib.h>

std::string StructDissectPane::FormatFieldValue(const FieldValue& fieldValue, Field& field)
{
    auto formatInteger = []<typename T>(FieldType type, T value) -> std::string {
        switch (type) {
            case FieldType::HexInt: return std::format("0x{:X}", value);
            case FieldType::SignedDecInt: return std::format("{}", static_cast<std::make_signed_t<T>>(value));
            case FieldType::UnsignedDecInt: return std::format("{}", value);
            default: return "Invalid Type";
        }
    };

    overloads visitor {
        [&](int8_t value) { return formatInteger(field.Type, value); },
        [&](int16_t value) { return formatInteger(field.Type, value); },
        [&](int32_t value) { return formatInteger(field.Type, value); },
        [&](int64_t value) { return formatInteger(field.Type, value); },
        [&](uint8_t value) { return formatInteger(field.Type, value); },
        [&](uint16_t value) { return formatInteger(field.Type, value); },
        [&](uint32_t value) { return formatInteger(field.Type, value); },
        [&](uint64_t value) { return formatInteger(field.Type, value); },
        [&](float value) { return std::format("{}", value); },
        [&](double value) { return std::format("{}", value); },
        [&](const Pointer& ptr) { return std::format("0x{:X}", ptr.Address); },
        [&](const StartEndPointer& ptr) { return std::format("[0x{:X} - 0x{:X}]", ptr.Start, ptr.End); },
        [&](const std::string& str) {
            std::string escaped;
            escaped.reserve(str.size() * 2);
            static const char* hex = "0123456789ABCDEF";
            for (uint8_t c : str) {
                switch (c) {
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\"': escaped += "\\\""; break;
                    default: {
                        if (c >= 32 && c <= 126) {
                            escaped.push_back(static_cast<char>(c));
                        } else {
                            escaped += "\\x";
                            escaped += hex[c >> 4];
                            escaped += hex[c & 0xF];
                        }
                    }
                }
            }
            return std::format("{}", escaped);
        },
    };
    return std::visit(visitor, fieldValue);
}

FieldValue ParseFieldValue(const std::string& str, const Field& field)
{
    std::string parsed = str;
    int base = 10;
    if (field.Type != FieldType::String) {
        if (parsed.starts_with("0x") || parsed.starts_with("0X")) {
            parsed = parsed.substr(2);
            base = 16;
        }
    }

    switch (field.Type) {
        case FieldType::SignedDecInt: {
            switch (field.Size) {
                case 1: return static_cast<int8_t>(std::stoll(parsed, nullptr, base));
                case 2: return static_cast<int16_t>(std::stoll(parsed, nullptr, base));
                case 4: return static_cast<int32_t>(std::stoll(parsed, nullptr, base));
                case 8: return std::stoll(parsed);
                default: return {};
            }
        }
        case FieldType::UnsignedDecInt: {
            switch (field.Size) {
                case 1: return static_cast<uint8_t>(std::stoull(parsed, nullptr, base));
                case 2: return static_cast<uint16_t>(std::stoull(parsed, nullptr, base));
                case 4: return static_cast<uint32_t>(std::stoull(parsed, nullptr, base));
                case 8: return std::stoull(parsed, nullptr, base);
                default: return {};
            }
        }
        case FieldType::HexInt: {
            switch (field.Size) {
                case 1: return static_cast<uint8_t>(std::stoull(parsed, nullptr, 16));
                case 2: return static_cast<uint16_t>(std::stoull(parsed, nullptr, 16));
                case 4: return static_cast<uint32_t>(std::stoull(parsed, nullptr, 16));
                case 8: return std::stoull(parsed, nullptr, 16);
                default: return {};
            }
        }
        case FieldType::Float: return std::stof(parsed);
        case FieldType::Double: return std::stod(parsed);
        case FieldType::String: return parsed;
        case FieldType::Pointer: return Pointer { std::stoull(parsed, nullptr, 16) };
        case FieldType::StartEndPointer: {
            size_t dashPos = parsed.find('-');
            if (dashPos == std::string::npos) {
                return {};
            }
            std::string startStr = parsed.substr(0, dashPos);
            std::string endStr = parsed.substr(dashPos + 1);
            uintptr_t startAddr = std::stoull(startStr, nullptr, 16);
            uintptr_t endAddr = std::stoull(endStr, nullptr, 16);
            return StartEndPointer { startAddr, endAddr };
        }
        default: return {};
    }
}

StructDissectPane::StructDissectPane(State& state, ModalManager& modalManager)
    : Pane(ICON_MDI_CONTENT_CUT " Struct Dissect", state)
    , m_ModalManager(modalManager)
{
    m_ModalManager.RegisterModal("Add Dissection", BIND_FN(StructDissectPane::AddDissectionModal));
    m_ModalManager.RegisterModal("Add Element", BIND_FN(StructDissectPane::AddElementModal));
    m_ModalManager.RegisterModal("Edit Value", BIND_FN(StructDissectPane::EditValueModal));
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

void StructDissectPane::AddDissection(const std::string& name, uintptr_t address)
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

        // for (size_t i = 0; i < dissection.Field.Children.size(); i++) {
        //     Field& field = dissection.Field.Children[i];
        //     ImGui::PushID(&field);
        //     bool deleteField = DrawField(field, dissection.Address);
        //     if (deleteField) {
        //         dissection.Field.Children.erase(dissection.Field.Children.begin() + i);
        //         i--;
        //     }
        //     ImGui::PopID();
        // }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    Fonts::Pop();
}

bool StructDissectPane::DrawField(Field& field, uintptr_t baseAddress, size_t depth) const
{
    static Field* selectedField = nullptr;
    bool selected = (&field == selectedField);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    uintptr_t address = baseAddress + field.Offset;
    std::string label;
    if (field.Name.empty()) {
        label = std::format(
            "0x{:04X} - {} {} {}",
            field.Offset, field.Size, field.Size == 1 ? "Byte" : "Bytes",
            field.Type);
    } else {
        label = std::format(
            "0x{:04X} - {} {} {}",
            field.Offset, field.Size, field.Size == 1 ? "Byte" : "Bytes",
            field.Name);
    }

    // Determine row visibility
    ImVec2 rowMin = ImGui::GetCursorScreenPos();
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();
    ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x, rowMin.y + rowHeight);
    bool rowVisible = ImGui::IsRectVisible(rowMin, rowMax);

    FieldValue value {};
    bool valueRead = false;
    if (rowVisible) {
        value = field.ReadField(m_State.Process, baseAddress);
        valueRead = true;
    }

    // Draw selectable row
    ImGui::TableSetColumnIndex(0);
    if (ImGui::Selectable("", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, rowHeight))) {
        selectedField = &field;
    }

    std::string contextMenuLabel = std::format("##field_value_context_menu_{:X}", reinterpret_cast<uintptr_t>(&field));
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        selectedField = &field;
        ImGui::OpenPopup(contextMenuLabel.c_str());
    }

    bool shouldDelete = FieldContextMenu(contextMenuLabel, field, address, value);

    ImGui::SameLine();

    // Render the field value and/or contents
    float cursorX = ImGui::GetCursorPosX();
    float cursorY = ImGui::GetCursorPosY();

    if (field.Type == FieldType::Dissection || field.Type == FieldType::Pointer || field.Type == FieldType::StartEndPointer) {
        ImGui::SetCursorPosX(cursorX + static_cast<float>(depth) * 16.0f);
        ImGui::SetCursorPosY(cursorY - 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        if (ImGui::ArrowButton(label.c_str(), field.Expanded ? ImGuiDir_Down : ImGuiDir_Right)) {
            field.Expanded = !field.Expanded;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::SetCursorPosX(cursorX + static_cast<float>(depth) * 16.0f + 25.0f);
        ImGui::SetCursorPosY(cursorY - 2.0f);
        ImGui::Text(label.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosY(cursorY - 1.0f);
        if (field.Type != FieldType::Dissection) {
            std::string formattedValue = FormatFieldValue(value, field);
            std::string formattedAddr = std::format("0x{:X}", address);
            ImGui::Text("%s : %s", formattedAddr.c_str(), formattedValue.c_str());
        }

        if (field.Expanded) {
            size_t size;
            size_t pointedAddress;
            if (field.Type == FieldType::StartEndPointer) {
                if (!valueRead) {
                    value = field.ReadField(m_State.Process, baseAddress);
                }

                StartEndPointer pair = std::get<StartEndPointer>(value);
                pointedAddress = pair.Start;
                size = pair.End - pair.Start;
            } else if (field.Type == FieldType::Pointer) {
                if (!valueRead) {
                    value = field.ReadField(m_State.Process, baseAddress);
                }

                pointedAddress = std::get<Pointer>(value).Address;
                size = 0x400;
            } else {
                pointedAddress = baseAddress;
                size = 0x400;
            }

            if (!field.Explored) {
                field.Children = ExploreAddress(m_State.Process, pointedAddress, size);
                field.Explored = true;
            }

            for (size_t i = 0; i < field.Children.size(); i++) {
                Field& childField = field.Children[i];
                ImGui::PushID(&childField);
                bool deleteField = DrawField(childField, pointedAddress, depth + 1);
                if (deleteField) {
                    field.Children.erase(field.Children.begin() + i);
                    i--;
                }
                ImGui::PopID();
            }
        }
    } else {
        ImGui::SetCursorPosX(cursorX + static_cast<float>(depth) * 16.0f + 25.0f);
        ImGui::SetCursorPosY(cursorY + 1.0f);
        ImGui::Text(label.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosY(cursorY + 2.0f);
        std::string formattedValue = FormatFieldValue(value, field);
        std::string formattedAddr = std::format("0x{:X}", address);
        ImGui::Text("%s : %s", formattedAddr.c_str(), formattedValue.c_str());
    }
    return shouldDelete;
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
                    field.Children = ExploreAddress(m_State.Process, pointedAddress, 0x400);
                } else {
                    field.Children = ExploreAddress(m_State.Process, address, 0x400);
                }
                field.Explored = true;
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

void StructDissectPane::AddDissectionModal(const std::string& name, const std::any& payload)
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

void StructDissectPane::EditValueModal(const std::string& name, const std::any& rawPayload)
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