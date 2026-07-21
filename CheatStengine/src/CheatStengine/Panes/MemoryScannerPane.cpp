#include "MemoryScannerPane.h"

#include <CheatStengine/MainLayer.h>
#include <CheatStengine/UI/ImGui/Fonts.h>
#include <CheatStengine/Utils.h>
#include <Engine/Core/Log.h>

#include <IconsFontAwesome6.h>
#include <IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <limits>

static const char* s_ScanTypeNames[] = {
    "Exact Value",
    "Bigger than",
    "Smaller than",
    "Value between",
    "Unknown initial value"
};
static_assert(IM_ARRAYSIZE(s_ScanTypeNames) == static_cast<size_t>(ScanType::COUNT));

static const char* s_ValueTypeNames[] = {
    "Int8",
    "Int16",
    "Int32",
    "Int64",
    "UInt8",
    "UInt16",
    "UInt32",
    "UInt64",
    "Float",
    "Double",
};
static_assert(IM_ARRAYSIZE(s_ValueTypeNames) == static_cast<size_t>(ValueType::COUNT));

static bool IsIntegerType(ValueType type)
{
    return type != ValueType::Float
        && type != ValueType::Double
        && type != ValueType::COUNT;
}

static std::string GetScanInput(const std::string& input, ValueType type, bool isHex)
{
    if (!isHex || !IsIntegerType(type)) {
        return input;
    }

    const size_t first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return input;
    }
    const size_t last = input.find_last_not_of(" \t\r\n");
    std::string value = input.substr(first, last - first + 1);
    const size_t prefix = value.front() == '+' || value.front() == '-' ? 1 : 0;
    if (value.size() >= prefix + 2
        && value[prefix] == '0'
        && (value[prefix + 1] == 'x' || value[prefix + 1] == 'X')) {
        return value;
    }
    value.insert(prefix, "0x");
    return value;
}

MemoryScannerPane::MemoryScannerPane(State& state)
    : Pane(ICON_MDI_RADAR " Memory Scanner", state)
{
}

void MemoryScannerPane::Draw(double deltaTime)
{
    ImGui::Begin(m_Name.c_str(), &m_Open);

    const uint32_t processId = m_State.Process && m_State.Process->IsValid()
        ? m_State.Process->GetPid()
        : 0;
    if (processId != m_ProcessId) {
        m_Scanner.reset();
        m_ProcessId = processId;
        m_ModuleFilter = ModuleFilter::Any;
        m_SelectedModuleIndex = 0;
        m_MinAddress = 0;
        m_MaxAddress = 0x7FFFFFFFFFFF;
    }
    if (m_ModuleFilter == ModuleFilter::Specific
        && (m_SelectedModuleIndex < 0
            || static_cast<size_t>(m_SelectedModuleIndex) >= m_State.Modules.size())) {
        m_ModuleFilter = ModuleFilter::Any;
        m_SelectedModuleIndex = 0;
        m_MinAddress = 0;
        m_MaxAddress = 0x7FFFFFFFFFFF;
    }

    size_t resultCount = 0;
    if (m_Scanner) {
        std::lock_guard lock(m_Scanner->GetMutex());
        resultCount = m_Scanner->GetResults().size();
    }
    ImGui::Text("Found %llu", static_cast<unsigned long long>(resultCount));
    if (ImGui::BeginTable("MemoryScannerView", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        if (ImGui::BeginTable("AddressTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Value");
            ImGui::TableSetupColumn("Previous");
            ImGui::TableSetupColumn("First");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            if (m_Scanner) {
                std::lock_guard lock(m_Scanner->GetMutex());
                const std::vector<ScannedAddress>& results = m_Scanner->GetResults();
                ImGuiListClipper clipper;
                clipper.Begin(results.size());
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        const ScannedAddress& result = results[row];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        std::optional<MODULEENTRY32> modEntry = Utils::GetModuleForAddress(result.Address, m_State.Modules);
                        if (modEntry) {
                            ImGui::Text("%s+0x%llX", modEntry->szModule, result.Address - reinterpret_cast<uintptr_t>(modEntry->modBaseAddr));
                        } else {
                            ImGui::Text("0x%llX", result.Address);
                        }

                        std::string valueStr = ScanValueToString(result.ReadValue(*m_State.Process), m_IsInputHex);
                        std::string previousStr = ScanValueToString(result.PreviousValue, m_IsInputHex);
                        std::string firstStr = ScanValueToString(result.FirstValue, m_IsInputHex);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", valueStr.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", previousStr.c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", firstStr.c_str());

                        ImGui::TableSetColumnIndex(0);
                        std::string selectableLabel = std::format("##scanned_address_{:X}", result.Address);
                        if (ImGui::Selectable(selectableLabel.c_str(), false,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick)) {
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                m_State.WatchAddresses.push_back(MemoryAddress { "", result.Address, m_ValueType });
                            }
                        }
                    }
                }
            }

            ImGui::EndTable();
        }

        ImGui::TableSetColumnIndex(1);

        ImGui::SetNextItemWidth(100.0f);
        if (m_Scanner) {
            if (m_Scanner->IsScanning()) {
                if (ImGui::Button("Cancel Scan", ImVec2 { 80.0f, 0.0f })) {
                    m_Scanner->Cancel();
                }
            } else if (ImGui::Button("New Scan", ImVec2 { 80.0f, 0.0f })) {
                m_Scanner.reset();
            }
        } else {
            if (ImGui::Button("First Scan", ImVec2 { 80.0f, 0.0f })) {
                StartFirstScan();
            }
        }
        ImGui::SameLine();

        const bool isDisabled = !m_Scanner || m_Scanner->IsScanning();
        if (isDisabled) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        if (ImGui::Button("Next Scan", ImVec2 { 80.0f, 0.0f })) {
            if (m_Scanner) {
                const std::string lowerValue = GetScanInput(m_ScanInputLower, m_ValueType, m_IsInputHex);
                const std::string upperValue = GetScanInput(m_ScanInputUpper, m_ValueType, m_IsInputHex);
                m_Scanner->NextScan(m_ValueType, m_ScanType, m_MinAddress, m_MaxAddress, lowerValue, upperValue);
            }
        }

        if (isDisabled) {
            ImGui::PopStyleVar();
            ImGui::PopItemFlag();
        }

        ImGui::Checkbox("Hex", &m_IsInputHex);
        float checkboxWidth = ImGui::GetItemRectSize().x;
        ImGui::SameLine();

        float fullWidth = ImGui::CalcItemWidth() - checkboxWidth - ImGui::GetStyle().ItemSpacing.x;

        if (m_ScanType == ScanType::ValueBetween) {
            float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            float textWidth = ImGui::CalcTextSize("and").x + spacing * 3;
            float halfWidth = (fullWidth - textWidth) * 0.5f;

            ImGui::SetNextItemWidth(halfWidth);
            ImGui::InputText("##lower_value", &m_ScanInputLower);

            ImGui::SameLine();
            ImGui::Text("and");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(halfWidth);
            ImGui::InputText("Value", &m_ScanInputUpper);
        } else {
            ImGui::SetNextItemWidth(fullWidth);
            ImGui::InputText("Value", &m_ScanInputLower);
        }

        if (ImGui::BeginCombo("Scan Type", s_ScanTypeNames[static_cast<size_t>(m_ScanType)])) {
            for (size_t n = 0; n < IM_ARRAYSIZE(s_ScanTypeNames); n++) {
                bool isSelected = (m_ScanType == static_cast<ScanType>(n));
                if (ImGui::Selectable(s_ScanTypeNames[n], isSelected)) {
                    m_ScanType = static_cast<ScanType>(n);
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (m_Scanner) {
            ImGui::BeginDisabled();
        }
        if (ImGui::BeginCombo("Value Type", s_ValueTypeNames[static_cast<size_t>(m_ValueType)])) {
            for (size_t n = 0; n < IM_ARRAYSIZE(s_ValueTypeNames); n++) {
                bool isSelected = (m_ValueType == static_cast<ValueType>(n));
                if (ImGui::Selectable(s_ValueTypeNames[n], isSelected)) {
                    m_ValueType = static_cast<ValueType>(n);
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (m_Scanner) {
            ImGui::EndDisabled();
        }

        std::string comboLabel = (m_ModuleFilter == ModuleFilter::Any) ? "Any" : m_State.Modules[m_SelectedModuleIndex].szModule;
        if (ImGui::BeginCombo("Module", comboLabel.c_str())) {
            // "Any" option
            bool isAnySelected = (m_ModuleFilter == ModuleFilter::Any);
            if (ImGui::Selectable("Any", isAnySelected)) {
                m_ModuleFilter = ModuleFilter::Any;
                m_MinAddress = 0;
                m_MaxAddress = 0x7FFFFFFFFFFF;
            }
            if (isAnySelected) {
                ImGui::SetItemDefaultFocus();
            }

            // Module-specific options
            for (size_t i = 0; i < m_State.Modules.size(); i++) {
                bool isSelected = (m_ModuleFilter == ModuleFilter::Specific && m_SelectedModuleIndex == i);
                if (ImGui::Selectable(m_State.Modules[i].szModule, isSelected)) {
                    m_ModuleFilter = ModuleFilter::Specific;
                    m_SelectedModuleIndex = i;
                    m_MinAddress = reinterpret_cast<uintptr_t>(m_State.Modules[i].modBaseAddr);
                    const uintptr_t moduleSize = static_cast<uintptr_t>(m_State.Modules[i].modBaseSize);
                    m_MaxAddress = moduleSize > std::numeric_limits<uintptr_t>::max() - m_MinAddress
                        ? std::numeric_limits<uintptr_t>::max()
                        : m_MinAddress + moduleSize;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputScalar("Start", ImGuiDataType_U64, &m_MinAddress, nullptr, nullptr, "%012llX");
        ImGui::InputScalar("Stop", ImGuiDataType_U64, &m_MaxAddress, nullptr, nullptr, "%012llX");

        ImGui::EndTable();
    }

    ImGui::End();
}

void MemoryScannerPane::StartFirstScan()
{
    if (!m_State.Process || !m_State.Process->IsValid()) {
        ERR("Select a valid process before starting a memory scan");
        return;
    }

    INFO("Scanning...");
    const std::string lowerValue = GetScanInput(m_ScanInputLower, m_ValueType, m_IsInputHex);
    const std::string upperValue = GetScanInput(m_ScanInputUpper, m_ValueType, m_IsInputHex);
    m_Scanner = std::make_unique<MemoryScanner>(m_State.Process);
    if (!m_Scanner->FirstScan(m_ValueType, m_ScanType, m_MinAddress, m_MaxAddress, lowerValue, upperValue)) {
        m_Scanner = nullptr;
    }
}
